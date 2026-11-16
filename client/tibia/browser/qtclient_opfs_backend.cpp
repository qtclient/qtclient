#if defined(__EMSCRIPTEN__)

// WasmFS backend: /home/web_user is the origin's OPFS, served by the portal's
// qtclient-fs Worker (qtc-aac apps/web/src/routes/play/qtclient-fs-worker).
// The Worker is the only OPFS client; the portal reaches it via qtclient-fs/bridge.ts.
//
// Every native file API (QFile, QSaveFile, QSettings, QLockFile, fopen,
// std::filesystem) reaches OPFS through plain POSIX calls, exactly as on
// desktop. File bytes are never cached here or in JS: reads and writes go
// between the Worker's SyncAccessHandle and the caller's buffer in the wasm
// heap.
//
// Structure follows Emscripten's own OPFS backend
// (system/lib/wasmfs/backends/opfs_backend.cpp). It cannot be used as is:
// its proxy thread busy-waits the main thread on OPFS promises, and Safari only
// settles a Worker's OPFS promises while the page's main thread is free
// (FileSystemHandle lookups, createSyncAccessHandle, removeEntry, move, …).
//
// Transport: a request block in this module's (shared) heap. Each calling
// thread owns one slot: it writes op + args, sets state=1 and rings the bell;
// the Worker sets state=3 with the results, then wakes the caller (futex
// notify for a pthread, a `main-done` message for the main thread). Wake-ups
// to and from the main thread are messages: Safari does not run an
// Atomics.waitAsync resolution until some other task arrives.
//  - sync ops (I/O on an open access handle, bookkeeping) never need the main
//    thread: the main thread spins on them (bounded) before suspending.
//  - async ops (lookups, create, list, remove, move, open) need a free main
//    thread in Safari: the main thread suspends (JSPI) until they finish.
//  Pthreads block in futex_wait either way.
// Contract with browser/jspi-runtime.js: the main thread only suspends inside
// its own activation; a file op from a raw JS→wasm re-entry while main is
// suspended aborts with both stacks.

#include "qtclient_opfs_backend.h"

#include "backend.h"
#include "file.h"
#include "wasmfs.h"

#include <emscripten.h>
#include <emscripten/console.h>
#include <emscripten/threading.h>

#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace wasmfs;

// From <emscripten/wasmfs.h>, which cannot be included next to the internal
// headers (it declares its own global Backend / backend_t).
extern "C" int wasmfs_create_directory(const char* path, mode_t mode, backend_t backend);

namespace {

// Must match the Worker's PROTOCOL and bridge.ts QTCLIENT_FS_PROTOCOL.
constexpr int32_t kProtocol = 5;
constexpr int kSlots = 64;
// Longest a main-thread sync op spins before it suspends instead. Sync ops
// complete in microseconds unless the Worker is mid-way through other work.
constexpr double kMainSpinMs = 4.0;

enum Op : int32_t {
    OpChild = 1,
    OpInsertFile = 2,
    OpInsertDir = 3,
    OpRemove = 4,
    OpEntries = 5,
    OpMove = 6,
    OpOpen = 7,
    OpClose = 8,
    OpRead = 9,
    OpWrite = 10,
    OpSizeAccess = 11,
    OpTruncAccess = 12,
    // 13: retired (fsync is not issued on web, see OpfsFile::flush).
    OpSizeFile = 14,
    OpTruncFile = 15,
    OpFreeNode = 16,
};

bool isSyncOp(Op op)
{
    switch (op) {
    case OpClose:
    case OpRead:
    case OpWrite:
    case OpSizeAccess:
    case OpTruncAccess:
    case OpFreeNode:
        return true;
    default:
        return false;
    }
}

enum SlotState : int32_t { Idle = 0, Posted = 1, Running = 2, Done = 3 };

struct alignas(8) Slot {
    std::atomic<int32_t> state;
    int32_t op;
    double args[7];
    double r0;
    double r1;
};

struct alignas(8) Control {
    std::atomic<int32_t> bell;
    Slot slots[kSlots];
};

Control g_control;
constexpr int kMainSlot = 0;
// Slots 1..kSlots-1 for pthreads; bit i set = slot i taken.
std::atomic<uint64_t> g_threadSlots{1};
std::atomic<bool> g_threadUseLogged{false};

struct ThreadSlot {
    int index = -1;
    ~ThreadSlot()
    {
        if (index > 0)
            g_threadSlots.fetch_and(~(uint64_t(1) << index));
    }
};
thread_local ThreadSlot t_slot;

int slotForThisThread()
{
    if (emscripten_is_main_browser_thread())
        return kMainSlot;
    if (t_slot.index > 0)
        return t_slot.index;
    uint64_t used = g_threadSlots.load();
    for (;;) {
        if (~used == 0) {
            EM_ASM({ abort('qtclient-fs: more than 63 threads use the file system'); });
        }
        const int index = __builtin_ctzll(~used);
        if (g_threadSlots.compare_exchange_weak(used, used | (uint64_t(1) << index))) {
            t_slot.index = index;
            if (!g_threadUseLogged.exchange(true)) {
                // Worth knowing: a pthread holding a WasmFS lock across an
                // async op makes the main thread wait for that lock.
                emscripten_console_log("qtclient-fs: first file system use from a pthread");
            }
            return index;
        }
    }
}

EM_JS(void, qtclientFsRequireMainActivation, (int op), {
    var jspi = Module.qtclientJspi;
    if (!jspi)
        abort('qtclient-fs: browser/jspi-runtime.js post-js is not linked');
    if (jspi.parked > 0) {
        var msg = 'qtclient-fs op ' + op +
                  ' called from a JS→wasm re-entry while main is suspended. ' +
                  'Route that callback through the Qt event loop (browser/jspi-runtime.js).';
        err(msg + '\n' + new Error().stack);
        abort(msg);
    }
});

// Ring the Worker's bell. The Worker sleeps in Atomics.waitAsync on it, but
// Safari leaves a waitAsync resolution queued until some other task wakes the
// Worker's event loop (seconds when /play is otherwise idle). So the main
// thread also posts a `ring` message — a task, served at once. A pthread
// cannot reach the Worker object; Atomics.notify is its ring.
EM_JS(void, qtclientFsRing, (int32_t* bell, int fromMain), {
    Atomics.add(HEAP32, bell >> 2, 1);
    Atomics.notify(HEAP32, bell >> 2);
    if (fromMain)
        Module.qtclientFs.ring();
});

// Main thread: wait without blocking the browser's event loop. The worker
// posts `main-done` for the main slot (kMainSlot); a message is a task, so
// Safari resumes at once — an Atomics.waitAsync resolution stays queued there
// until some unrelated task wakes the loop (seconds while the page is idle).
EM_ASYNC_JS(void, qtclientFsAwaitMain, (int32_t* state), {
    var i = state >> 2;
    while (Atomics.load(HEAP32, i) !== 3)
        await Module.qtclientFs.mainDone();
});

struct Result {
    double r0 = 0;
    double r1 = 0;
};

// Main-thread time spent in file system calls (JSPI suspends included), for
// /play diagnostics: what the app feels, next to the Worker's own counters.
double g_mainWaitMs;
uint32_t g_mainCalls;

Result call(Op op, std::initializer_list<double> args)
{
    const bool main = emscripten_is_main_browser_thread();
    if (main)
        qtclientFsRequireMainActivation(op);
    const double started = main ? emscripten_get_now() : 0;
    Slot& slot = g_control.slots[slotForThisThread()];
    slot.op = op;
    int i = 0;
    for (double a : args)
        slot.args[i++] = a;
    slot.state.store(Posted);
    qtclientFsRing(reinterpret_cast<int32_t*>(&g_control.bell), main);

    if (!main) {
        for (int32_t s; (s = slot.state.load()) != Done;)
            emscripten_futex_wait(&slot.state, static_cast<uint32_t>(s), INFINITY);
    } else if (isSyncOp(op)) {
        const double t0 = emscripten_get_now();
        while (slot.state.load() != Done) {
            if (emscripten_get_now() - t0 > kMainSpinMs) {
                qtclientFsAwaitMain(reinterpret_cast<int32_t*>(&slot.state));
                break;
            }
        }
    } else {
        qtclientFsAwaitMain(reinterpret_cast<int32_t*>(&slot.state));
    }

    Result r{slot.r0, slot.r1};
    slot.state.store(Idle);
    if (main) {
        g_mainWaitMs += emscripten_get_now() - started;
        ++g_mainCalls;
    }
    return r;
}

double ptrArg(const void* p)
{
    return static_cast<double>(reinterpret_cast<uintptr_t>(p));
}

int asErr(double v)
{
    return static_cast<int>(v);
}

class OpfsFile final : public DataFile {
public:
    /// `preopened`: access handle the Worker opened while creating the file
    /// (WasmFS opens right after creating), so that open needs no round trip.
    OpfsFile(mode_t mode, backend_t backend, int nodeId, int preopened = 0)
        : DataFile(mode, backend)
        , m_node(nodeId)
        , m_preopened(preopened)
    {
    }

    ~OpfsFile() override
    {
        if (m_preopened > 0)
            call(OpClose, {double(m_preopened)});
        call(OpFreeNode, {double(m_node)});
    }

    int nodeId() const { return m_node; }

private:
    off_t getSize() override
    {
        const Result r = m_access > 0 ? call(OpSizeAccess, {double(m_access)})
                                      : call(OpSizeFile, {double(m_node)});
        return static_cast<off_t>(r.r0);
    }

    int setSize(off_t size) override
    {
        const Result r = m_access > 0 ? call(OpTruncAccess, {double(m_access), double(size)})
                                      : call(OpTruncFile, {double(m_node), double(size)});
        return asErr(r.r0);
    }

    int open(oflags_t) override
    {
        if (m_opens == 0 && m_preopened > 0) {
            m_access = m_preopened;
            m_preopened = 0;
        } else if (m_opens == 0) {
            const Result r = call(OpOpen, {double(m_node)});
            if (r.r0 < 0)
                return asErr(r.r0);
            m_access = static_cast<int>(r.r0);
        }
        ++m_opens;
        return 0;
    }

    int close() override
    {
        if (--m_opens > 0)
            return 0;
        const Result r = call(OpClose, {double(m_access)});
        m_access = 0;
        return asErr(r.r0);
    }

    ssize_t read(uint8_t* buf, size_t len, off_t offset) override
    {
        const Result r = call(OpRead, {double(m_access), ptrArg(buf), double(len), double(offset)});
        return static_cast<ssize_t>(r.r0);
    }

    ssize_t write(const uint8_t* buf, size_t len, off_t offset) override
    {
        const Result r = call(OpWrite, {double(m_access), ptrArg(buf), double(len), double(offset)});
        return static_cast<ssize_t>(r.r0);
    }

    // fsync is not issued on web: SyncAccessHandle data is persisted when the
    // handle closes and survives tab / browser crashes; only an OS crash right
    // after a write could lose it, and the launcher hash-checks what it
    // installs. Skipping it saves ~1 ms per QSaveFile commit (8k+ per install).
    int flush() override { return 0; }

    const int m_node;
    int m_preopened = 0;
    int m_access = 0;
    size_t m_opens = 0;
};

class OpfsDirectory final : public Directory {
public:
    /// `createdHere`: this process made the directory, so WasmFS's dcache
    /// holds every child (all later changes go through WasmFS too). A lookup
    /// that misses the dcache is then a miss — answered without the Worker.
    /// (Another /play tab writing into it would not be seen; OPFS access
    /// handles already make two tabs on one profile fail loudly.)
    OpfsDirectory(mode_t mode, backend_t backend, int nodeId, bool createdHere = false)
        : Directory(mode, backend)
        , m_node(nodeId)
        , m_createdHere(createdHere)
    {
    }

    ~OpfsDirectory() override
    {
        if (m_node != kRootNode)
            call(OpFreeNode, {double(m_node)});
    }

    int nodeId() const { return m_node; }

    static constexpr int kRootNode = 1;

private:
    std::shared_ptr<File> getChild(const std::string& name) override
    {
        if (m_createdHere)
            return nullptr;
        const Result r = call(OpChild, {double(m_node), ptrArg(name.data()), double(name.size())});
        if (r.r0 < 0)
            return nullptr;
        const int id = static_cast<int>(r.r1);
        if (static_cast<int>(r.r0) == File::DataFileKind)
            return std::make_shared<OpfsFile>(0777, getBackend(), id);
        return std::make_shared<OpfsDirectory>(0777, getBackend(), id);
    }

    std::shared_ptr<DataFile> insertDataFile(const std::string& name, mode_t mode) override
    {
        const Result r = call(OpInsertFile, {double(m_node), ptrArg(name.data()), double(name.size())});
        if (r.r0 < 0)
            return nullptr;
        return std::make_shared<OpfsFile>(mode, getBackend(), static_cast<int>(r.r0), static_cast<int>(r.r1));
    }

    std::shared_ptr<Directory> insertDirectory(const std::string& name, mode_t mode) override
    {
        const Result r = call(OpInsertDir, {double(m_node), ptrArg(name.data()), double(name.size())});
        if (r.r0 < 0)
            return nullptr;
        return std::make_shared<OpfsDirectory>(mode, getBackend(), static_cast<int>(r.r0), true);
    }

    std::shared_ptr<Symlink> insertSymlink(const std::string&, const std::string&) override
    {
        return nullptr; // OPFS has no symlinks.
    }

    int insertMove(const std::string& name, std::shared_ptr<File> file) override
    {
        int moved;
        if (auto f = file->dynCast<DataFile>())
            moved = std::static_pointer_cast<OpfsFile>(f)->nodeId();
        else
            moved = std::static_pointer_cast<OpfsDirectory>(file->cast<Directory>())->nodeId();
        const Result r = call(OpMove, {double(moved), double(m_node), ptrArg(name.data()), double(name.size())});
        return asErr(r.r0);
    }

    int removeChild(const std::string& name) override
    {
        return asErr(call(OpRemove, {double(m_node), ptrArg(name.data()), double(name.size())}).r0);
    }

    ssize_t getNumEntries() override
    {
        auto entries = getEntries();
        if (int err = entries.getError())
            return err;
        return entries->size();
    }

    MaybeEntries getEntries() override
    {
        // Records: kind (u8), name length (u16 LE), UTF-8 name.
        std::vector<uint8_t> buf(16 * 1024);
        Result r = call(OpEntries, {double(m_node), ptrArg(buf.data()), double(buf.size())});
        if (r.r0 == -ERANGE) {
            buf.resize(static_cast<size_t>(r.r1));
            r = call(OpEntries, {double(m_node), ptrArg(buf.data()), double(buf.size())});
        }
        if (r.r0 < 0)
            return {asErr(r.r0)};
        std::vector<Entry> entries;
        entries.reserve(static_cast<size_t>(r.r0));
        size_t at = 0;
        const size_t end = static_cast<size_t>(r.r1);
        while (at + 3 <= end) {
            const auto kind = static_cast<File::FileKind>(buf[at]);
            const size_t len = buf[at + 1] | (size_t(buf[at + 2]) << 8);
            entries.push_back({std::string(reinterpret_cast<const char*>(&buf[at + 3]), len), kind, 0});
            at += 3 + len;
        }
        return {entries};
    }

    const int m_node;
    const bool m_createdHere;
};

class OpfsBackend final : public Backend {
public:
    std::shared_ptr<DataFile> createFile(mode_t) override { return nullptr; }
    std::shared_ptr<Directory> createDirectory(mode_t mode) override
    {
        return std::make_shared<OpfsDirectory>(mode, this, OpfsDirectory::kRootNode);
    }
    std::shared_ptr<Symlink> createSymlink(std::string) override { return nullptr; }
};

// Layout + errno table for the Worker, in this order (see attach below).
int32_t g_attachInfo[] = {
    // layout (byte offsets)
    0, 0, sizeof(Slot), kSlots,
    offsetof(Slot, state), offsetof(Slot, op), offsetof(Slot, args), offsetof(Slot, r0), offsetof(Slot, r1),
    // errno (Emscripten / WASI numbering)
    ENOENT, EIO, EBADF, EACCES, EBUSY, EEXIST, ENOTDIR, EISDIR, EINVAL, ENOSPC, ERANGE, ENOTEMPTY, ENOSYS,
};

// 0 = ok; otherwise a message has been logged.
EM_ASYNC_JS(int, qtclientFsAttach, (const int32_t* info), {
    var fs = Module.qtclientFs;
    if (!fs || typeof fs.attachWasm !== 'function') {
        err('qtclient-fs: Module.qtclientFs is missing — the portal must provide it before callMain');
        return 1;
    }
    if (fs.protocol !== 5) {
        err('qtclient-fs: portal protocol ' + fs.protocol + ' != wasm 5 — portal and engine tip are out of sync');
        return 1;
    }
    var v = function (k) { return HEAP32[(info >> 2) + k]; };
    var layout = {
        bell: v(0), slots: v(1), stride: v(2), count: v(3),
        state: v(4), op: v(5), args: v(6), r0: v(7), r1: v(8),
    };
    var names = ['NOENT', 'IO', 'BADF', 'ACCES', 'BUSY', 'EXIST', 'NOTDIR', 'ISDIR', 'INVAL', 'NOSPC', 'RANGE', 'NOTEMPTY', 'NOSYS'];
    var errno = {};
    names.forEach(function (n, k) { errno[n] = v(9 + k); });
    try {
        await fs.attachWasm(wasmMemory, layout, errno);
    } catch (e) {
        err('qtclient-fs: attach failed: ' + (e && e.message || e));
        return 1;
    }
    return 0;
});

bool mkdirIfMissing(const char* path)
{
    if (mkdir(path, 0777) == 0 || errno == EEXIST)
        return true;
    emscripten_console_errorf("qtclient-fs: mkdir %s failed: %s", path, strerror(errno));
    return false;
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void qtclient_fs_stats_publish()
{
    // One statement per value: commas inside EM_ASM code split macro arguments.
    EM_ASM({
        var stats = {};
        stats.calls = $0;
        stats.ms = $1;
        Module.qtclientFsMain = stats;
    }, g_mainCalls, g_mainWaitMs);
}

bool qtclientMountOpfs()
{
    static_assert(sizeof(Control) % 8 == 0);
    g_attachInfo[0] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&g_control.bell));
    g_attachInfo[1] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&g_control.slots[0]));
    // The Worker reads state/bell through an Int32Array view.
    static_assert(sizeof(std::atomic<int32_t>) == 4);
    if (qtclientFsAttach(g_attachInfo) != 0)
        return false;

    const backend_t backend = wasmFS.addBackend(std::make_unique<OpfsBackend>());
    if (!mkdirIfMissing("/home"))
        return false;
    if (wasmfs_create_directory("/home/web_user", 0777, backend) != 0) {
        emscripten_console_errorf("qtclient-fs: mounting /home/web_user failed: %s", strerror(errno));
        return false;
    }
    // Tip packs live at OPFS mods/ (next to the binary's dir "/" on desktop).
    if (!mkdirIfMissing("/home/web_user/mods"))
        return false;
    if (symlink("/home/web_user/mods", "/mods") != 0) {
        emscripten_console_errorf("qtclient-fs: linking /mods failed: %s", strerror(errno));
        return false;
    }
    EM_ASM({ Module.qtclientFsMounted = 1; });
    return true;
}

#endif // __EMSCRIPTEN__
