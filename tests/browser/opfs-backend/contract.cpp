// Contract test for the /play file system: client/tibia/browser/
// qtclient_opfs_backend.cpp (WasmFS backend) + browser/qtclient-fs.js (the
// engine's OPFS Worker), in a real browser with real OPFS and JSPI. Plain
// POSIX calls, as Qt's file APIs make them. Run with run.mjs.
//
// It guards the parts we do not own: WasmFS's backend interface (Emscripten)
// and the browser's OPFS / Worker behaviour.

#include "browser/qtclient_opfs_backend.h"

#include <emscripten.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;
std::string g_log;

void note(const std::string& line)
{
    g_log += line + "\n";
    std::printf("%s\n", line.c_str());
}

#define CHECK(cond, what)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            ++g_failures;                                                                          \
            note(std::string("FAIL ") + (what) + " (errno " + std::strerror(errno) + ")");          \
        }                                                                                          \
    } while (0)

std::string g_base;

std::string path(const std::string& rel)
{
    return g_base + "/" + rel;
}

std::vector<char> pattern(size_t size, int seed)
{
    std::vector<char> bytes(size);
    for (size_t i = 0; i < size; ++i)
        bytes[i] = char((i * 31 + seed) & 0xff);
    return bytes;
}

bool writeFile(const std::string& file, const std::vector<char>& bytes)
{
    const int fd = open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
        return false;
    const bool ok = write(fd, bytes.data(), bytes.size()) == ssize_t(bytes.size());
    return close(fd) == 0 && ok;
}

bool readFile(const std::string& file, std::vector<char>& out)
{
    const int fd = open(file.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    struct stat st {};
    if (fstat(fd, &st) != 0) {
        close(fd);
        return false;
    }
    out.resize(size_t(st.st_size));
    const bool ok = read(fd, out.data(), out.size()) == ssize_t(out.size());
    close(fd);
    return ok;
}

void removeTree(const std::string& dir)
{
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string name = e->d_name;
            if (name == "." || name == "..")
                continue;
            const std::string child = dir + "/" + name;
            struct stat st {};
            if (stat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                removeTree(child);
            else
                unlink(child.c_str());
        }
        closedir(d);
    }
    rmdir(dir.c_str());
}

void testOffsets()
{
    const std::string file = path("offsets.bin");
    const int fd = open(file.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    CHECK(fd >= 0, "open offsets.bin");
    const auto big = pattern(1 << 20, 1);
    CHECK(pwrite(fd, big.data(), big.size(), 0) == ssize_t(big.size()), "pwrite 1 MiB at 0");
    const auto tail = pattern(100, 7);
    CHECK(pwrite(fd, tail.data(), tail.size(), 5'000'000) == 100, "pwrite past the end");
    struct stat st {};
    CHECK(fstat(fd, &st) == 0 && st.st_size == 5'000'100, "size after sparse write");
    std::vector<char> got(4096);
    CHECK(pread(fd, got.data(), got.size(), 12345) == 4096
              && std::memcmp(got.data(), big.data() + 12345, 4096) == 0,
          "pread inside the first write");
    got.assign(100, 0);
    CHECK(pread(fd, got.data(), 100, 5'000'000) == 100 && std::memcmp(got.data(), tail.data(), 100) == 0,
          "pread the tail");
    CHECK(ftruncate(fd, 1000) == 0, "ftruncate shrink");
    CHECK(fstat(fd, &st) == 0 && st.st_size == 1000, "size after shrink");
    CHECK(close(fd) == 0, "close offsets.bin");
    CHECK(stat(file.c_str(), &st) == 0 && st.st_size == 1000, "size of a closed file");
    CHECK(truncate(file.c_str(), 64) == 0, "truncate a closed file");
    CHECK(stat(file.c_str(), &st) == 0 && st.st_size == 64, "size after truncate");
}

void testSaveFile()
{
    // QSaveFile: write a temp file, rename it over the target.
    const std::string target = path("options.json");
    CHECK(writeFile(target, pattern(300, 2)), "write target v1");
    const auto v2 = pattern(500, 3);
    CHECK(writeFile(target + ".tmp", v2), "write temp v2");
    CHECK(rename((target + ".tmp").c_str(), target.c_str()) == 0, "rename temp over target");
    std::vector<char> got;
    CHECK(readFile(target, got) && got == v2, "target holds v2");
    struct stat st {};
    CHECK(stat((target + ".tmp").c_str(), &st) != 0 && errno == ENOENT, "temp is gone");
}

void testDirectories()
{
    CHECK(mkdir(path("dir").c_str(), 0755) == 0, "mkdir dir");
    CHECK(mkdir(path("dir/sub").c_str(), 0755) == 0, "mkdir dir/sub");
    for (const char* name : {"a", "b", "c"})
        CHECK(writeFile(path(std::string("dir/") + name), pattern(10, 4)), "write dir file");
    std::set<std::string> names;
    if (DIR* d = opendir(path("dir").c_str())) {
        while (dirent* e = readdir(d))
            names.insert(e->d_name);
        closedir(d);
    }
    CHECK((names == std::set<std::string>{".", "..", "a", "b", "c", "sub"}), "readdir lists dir");
    CHECK(rmdir(path("dir").c_str()) != 0 && errno == ENOTEMPTY, "rmdir non-empty fails");
    CHECK(unlink(path("dir/a").c_str()) == 0, "unlink");
    CHECK(rename(path("dir/b").c_str(), path("dir/sub/b").c_str()) == 0, "move into subdir");
    std::vector<char> got;
    CHECK(readFile(path("dir/sub/b"), got) && got == pattern(10, 4), "moved file content");
    CHECK(rename(path("dir/sub").c_str(), path("moved").c_str()) == 0, "rename a directory");
    CHECK(readFile(path("moved/b"), got), "file under renamed dir");
}

void testManyOpen()
{
    // Handles are capped like a process fd limit: past it, EMFILE (never a
    // wait — the caller may hold every handle itself).
    std::vector<int> fds;
    int emfile = 0;
    for (int i = 0; i < 160; ++i) {
        const int fd = open(path("many" + std::to_string(i)).c_str(), O_CREAT | O_RDWR, 0644);
        if (fd >= 0)
            fds.push_back(fd);
        else if (errno == EMFILE)
            ++emfile;
    }
    CHECK(fds.size() >= 100, "open at least 100 files at once");
    CHECK(fds.size() + emfile == 160, "every open either succeeds or fails with EMFILE");
    for (const int fd : fds)
        close(fd);
    const int again = open(path("many0").c_str(), O_RDONLY);
    CHECK(again >= 0, "open after releasing handles");
    if (again >= 0)
        close(again);
}

void testPthread()
{
    // Pthread requests ring the Worker over a BroadcastChannel.
    std::atomic<bool> done{false};
    std::atomic<int> threadFailures{0};
    std::thread worker([&] {
        for (int i = 0; i < 20; ++i) {
            const std::string file = path("thread" + std::to_string(i));
            const auto bytes = pattern(1000 + i, i);
            std::vector<char> got;
            if (!writeFile(file, bytes) || !readFile(file, got) || got != bytes)
                ++threadFailures;
            if (rename(file.c_str(), (file + ".done").c_str()) != 0)
                ++threadFailures;
        }
        done = true;
    });
    // Keep main's event loop free while the thread works (JSPI suspend).
    while (!done)
        emscripten_sleep(1);
    worker.join();
    CHECK(threadFailures == 0, "pthread write/read/rename");
}

// One statement per field: a comma inside EM_ASM code splits macro arguments.
void report(int failures, const std::string& log)
{
    EM_ASM({
        var result = {};
        result.failures = $0;
        result.log = UTF8ToString($1);
        globalThis.qtclientFsContract = result;
    }, failures, log.c_str());
}

} // namespace

int main()
{
    if (!qtclientMountOpfs()) {
        note("FAIL mount");
        report(1, "mount failed");
        return 1;
    }
    g_base = "/home/web_user/contract-" + std::to_string(emscripten_get_now());
    CHECK(mkdir(g_base.c_str(), 0755) == 0, "mkdir base");

    testOffsets();
    testSaveFile();
    testDirectories();
    testManyOpen();
    testPthread();

    removeTree(g_base);
    struct stat st {};
    CHECK(stat(g_base.c_str(), &st) != 0, "base removed");

    note(g_failures ? "contract: " + std::to_string(g_failures) + " failure(s)" : "contract: ok");
    report(g_failures, g_log);
    return 0;
}
