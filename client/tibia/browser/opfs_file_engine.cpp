#if defined(__EMSCRIPTEN__)

#include "opfs_file_engine.h"

#include <emscripten.h>
#include <cstdint>
#include <cstdlib>

#include <QtCore/private/qabstractfileengine_p.h>

#include <QDateTime>
#include <QDir>
#include <QStringList>
#include <QUrl>

#include <memory>

namespace {

bool tipFsPathOwned(const QString& path)
{
    return path == QLatin1String("/home/web_user")
        || path.startsWith(QLatin1String("/home/web_user/"))
        || path == QLatin1String("/mods")
        || path.startsWith(QLatin1String("/mods/"))
        || path == QLatin1String("/data")
        || path.startsWith(QLatin1String("/data/"));
}

bool tipFsLive()
{
    return EM_ASM_INT({
        var tip = (typeof Module !== 'undefined' && Module.tipFs) ? Module.tipFs
                  : (typeof tipFs !== 'undefined' ? tipFs : null);
        return tip && tip.ready ? 1 : 0;
    }) != 0;
}

// Unified Chrome + Safari tip-fs: EM_ASYNC_JS + JSPI (-sJSPI, wasm EH).
// Safari needs a main-thread yield for createSyncAccessHandle; JSPI suspends
// without Asyncify's Emval corruption on Chromium.

EM_ASYNC_JS(int, tipFsOpenJs, (const char* path, int flags), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var p = UTF8ToString(path);
    // Size is NOT cached on Module — concurrent JSPI opens would race.
    // TipFsEngine::open queries tipFsStat (single double return) instead.
    return tip.openAsync ? await tip.openAsync(p, flags) : tip.open(p, flags);
});

EM_ASYNC_JS(int, tipFsCloseJs, (int fd), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    if (tip.closeAsync)
        return await tip.closeAsync(fd);
    return tip.close(fd);
});

EM_ASYNC_JS(int, tipFsSeekJs, (int fd, int pos), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    if (tip.seekAsync)
        return await tip.seekAsync(fd, pos);
    return tip.seek(fd, pos);
});

EM_ASYNC_JS(int, tipFsFlushJs, (int fd), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    if (tip.flushAsync)
        return await tip.flushAsync(fd);
    return tip.flush(fd);
});

EM_ASYNC_JS(int, tipFsTruncateJs, (int fd, int size), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    if (tip.truncateAsync)
        return await tip.truncateAsync(fd, size);
    return tip.truncate(fd, size);
});

EM_ASYNC_JS(int, tipFsReadJs, (int fd, uint8_t* data, int maxlen), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var dest = data;
    var buf = new Uint8Array(maxlen);
    var n = tip.readAsync ? await tip.readAsync(fd, buf) : tip.read(fd, buf);
    if (n < 0)
        return n;
    // After await, wasm Memory may have grown — refresh HEAPU8.
    var heap = (typeof HEAPU8 !== 'undefined' && HEAPU8) ? HEAPU8
               : (Module.HEAPU8 || (Module.heap && Module.heap.u8));
    heap.set(buf.subarray(0, n), dest);
    return n;
});

EM_ASYNC_JS(int, tipFsWriteJs, (int fd, const uint8_t* data, int len), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var heap = (typeof HEAPU8 !== 'undefined' && HEAPU8) ? HEAPU8
               : (Module.HEAPU8 || (Module.heap && Module.heap.u8));
    var buf = heap.slice(data, data + len);
    if (tip.writeAsync)
        return await tip.writeAsync(fd, buf);
    return tip.write(fd, buf);
});

// Single return value — no out-pointers (JSPI stack restore) and no Module
// scratch fields (concurrent tipFsStat would race). Encoding:
// -1 = missing, -2 = directory, >=0 = file size (double mantissa safe to 2^53).
EM_ASYNC_JS(double, tipFsStatJs, (const char* path), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var p = UTF8ToString(path);
    var s = tip.statAsync ? await tip.statAsync(p) : tip.stat(p);
    if (!s || !s.exists)
        return -1;
    if (s.isDir)
        return -2;
    return s.size;
});

EM_ASYNC_JS(int, tipFsMkdirJs, (const char* path, int parents), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var p = UTF8ToString(path);
    if (tip.mkdirAsync)
        return await tip.mkdirAsync(p, !!parents);
    return tip.mkdir(p, !!parents);
});

EM_ASYNC_JS(int, tipFsRmdirJs, (const char* path), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var p = UTF8ToString(path);
    if (tip.rmdirAsync)
        return await tip.rmdirAsync(p);
    return tip.rmdir(p);
});

EM_ASYNC_JS(int, tipFsUnlinkJs, (const char* path), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var p = UTF8ToString(path);
    if (tip.unlinkAsync)
        return await tip.unlinkAsync(p);
    return tip.unlink(p);
});

EM_ASYNC_JS(int, tipFsRenameJs, (const char* from, const char* to), {
    var tip = Module.tipFs;
    if (!tip)
        return -1;
    var a = UTF8ToString(from);
    var b = UTF8ToString(to);
    if (tip.renameAsync)
        return await tip.renameAsync(a, b);
    return tip.rename(a, b);
});

EM_ASYNC_JS(char*, tipFsReaddirJs, (const char* path), {
    var tip = Module.tipFs;
    if (!tip)
        return 0;
    var p = UTF8ToString(path);
    var names = tip.readdirAsync ? await tip.readdirAsync(p)
                                 : (tip.readdir(p) || []);
    var joined = names.join('\n');
    var len = lengthBytesUTF8(joined) + 1;
    var ptr = _malloc(len);
    stringToUTF8(joined, ptr, len);
    return ptr;
});

int tipFsOpen(const QByteArray& pathUtf8, int flags)
{
    return tipFsOpenJs(pathUtf8.constData(), flags);
}

int tipFsClose(int fd)
{
    return tipFsCloseJs(fd);
}

int tipFsSeek(int fd, int pos)
{
    return tipFsSeekJs(fd, pos);
}

int tipFsFlush(int fd)
{
    return tipFsFlushJs(fd);
}

int tipFsTruncate(int fd, int size)
{
    return tipFsTruncateJs(fd, size);
}

int tipFsRead(int fd, char* data, int maxlen)
{
    return tipFsReadJs(fd, reinterpret_cast<uint8_t*>(data), maxlen);
}

int tipFsWrite(int fd, const char* data, int len)
{
    return tipFsWriteJs(fd, reinterpret_cast<const uint8_t*>(data), len);
}

struct TipStat {
    bool exists = false;
    bool isDir = false;
    qint64 size = 0;
};

TipStat tipFsStat(const QByteArray& pathUtf8)
{
    const double code = tipFsStatJs(pathUtf8.constData());
    TipStat st;
    if (code < 0) {
        if (code == -2) {
            st.exists = true;
            st.isDir = true;
            st.size = 0;
        }
        return st;
    }
    st.exists = true;
    st.isDir = false;
    st.size = static_cast<qint64>(code);
    return st;
}

int tipFsMkdir(const QByteArray& pathUtf8, bool parents)
{
    return tipFsMkdirJs(pathUtf8.constData(), parents ? 1 : 0);
}

int tipFsRmdir(const QByteArray& pathUtf8)
{
    return tipFsRmdirJs(pathUtf8.constData());
}

int tipFsUnlink(const QByteArray& pathUtf8)
{
    return tipFsUnlinkJs(pathUtf8.constData());
}

int tipFsRename(const QByteArray& fromUtf8, const QByteArray& toUtf8)
{
    return tipFsRenameJs(fromUtf8.constData(), toUtf8.constData());
}

QStringList tipFsReaddir(const QByteArray& pathUtf8)
{
    char* joined = tipFsReaddirJs(pathUtf8.constData());
    QStringList out;
    if (!joined)
        return out;
    const QString all = QString::fromUtf8(joined);
    std::free(joined);
    const QStringList parts = all.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& name : parts)
        out.append(name);
    return out;
}

int openFlagsFrom(QIODevice::OpenMode mode)
{
    // Match tip-fs TipFsOpen bits (Create=16, Truncate=4, Append=8).
    int flags = 0;
    const bool rd = mode.testFlag(QIODevice::ReadOnly);
    const bool wr = mode.testFlag(QIODevice::WriteOnly);
    if (rd && wr)
        flags |= 3;
    else if (wr)
        flags |= 2;
    else
        flags |= 1;
    if (mode.testFlag(QIODevice::Truncate))
        flags |= 4;
    if (mode.testFlag(QIODevice::Append))
        flags |= 8;
    if (wr)
        flags |= 16; // Create
    return flags;
}

class TipFsEngine final : public QAbstractFileEngine
{
public:
    explicit TipFsEngine(const QString& file)
        : m_file(file)
    {
    }

    void setFileName(const QString& file) override { m_file = file; }

    bool open(QIODevice::OpenMode openMode,
              std::optional<QFile::Permissions> = std::nullopt) override
    {
        if (m_fd >= 0)
            close();
        const int fd = tipFsOpen(m_file.toUtf8(), openFlagsFrom(openMode));
        if (fd < 0) {
            setError(QFile::OpenError, QStringLiteral("tip-fs open failed"));
            return false;
        }
        m_fd = fd;
        m_pos = 0;
        // tipFsStat returns size as a single double — JSPI-safe, no Module scratch.
        const TipStat st = tipFsStat(m_file.toUtf8());
        m_size = (st.exists && !st.isDir) ? st.size : 0;
        return true;
    }

    bool close() override
    {
        if (m_fd < 0)
            return true;
        const int rc = tipFsClose(m_fd);
        m_fd = -1;
        m_pos = 0;
        m_size = -1;
        if (rc != 0) {
            setError(QFile::UnspecifiedError, QStringLiteral("tip-fs close failed"));
            return false;
        }
        return true;
    }

    bool flush() override
    {
        if (m_fd < 0)
            return false;
        return tipFsFlush(m_fd) == 0;
    }

    bool syncToDisk() override { return flush(); }

    qint64 size() const override
    {
        // Prefer size captured at open — avoids a second tip-fs round-trip and
        // keeps QFile::readAll length correct for multi-MB appearances/RCC.
        if (m_fd >= 0 && m_size >= 0)
            return m_size;
        return tipFsStat(m_file.toUtf8()).size;
    }

    qint64 pos() const override { return m_pos; }

    bool seek(qint64 pos) override
    {
        if (m_fd < 0 || pos < 0)
            return false;
        const int rc = tipFsSeek(m_fd, static_cast<int>(pos));
        if (rc < 0)
            return false;
        m_pos = rc;
        return true;
    }

    bool isSequential() const override { return false; }

    bool remove() override
    {
        if (m_fd >= 0)
            close();
        return tipFsUnlink(m_file.toUtf8()) == 0;
    }

    bool rename(const QString& newName) override
    {
        if (m_fd >= 0)
            close();
        if (tipFsRename(m_file.toUtf8(), newName.toUtf8()) != 0)
            return false;
        m_file = newName;
        return true;
    }

    bool mkdir(const QString& dirName, bool createParentDirectories,
               std::optional<QFile::Permissions> = std::nullopt) const override
    {
        return tipFsMkdir(dirName.toUtf8(), createParentDirectories) == 0;
    }

    bool rmdir(const QString& dirName, bool) const override
    {
        return tipFsRmdir(dirName.toUtf8()) == 0;
    }

    bool setSize(qint64 size) override
    {
        if (m_fd < 0)
            return false;
        return tipFsTruncate(m_fd, static_cast<int>(size)) == 0;
    }

    bool caseSensitive() const override { return true; }

    bool isRelativePath() const override
    {
        return !m_file.startsWith(QLatin1Char('/'));
    }

    FileFlags fileFlags(FileFlags type = FileInfoAll) const override
    {
        FileFlags ret;
        const TipStat st = tipFsStat(m_file.toUtf8());
        if (!st.exists)
            return ret;
        ret |= ExistsFlag | LocalDiskFlag;
        if (type & TypesMask) {
            if (st.isDir)
                ret |= DirectoryType;
            else
                ret |= FileType;
        }
        if (type & PermsMask) {
            ret |= ReadUserPerm | WriteUserPerm | ReadOwnerPerm | WriteOwnerPerm;
            if (st.isDir)
                ret |= ExeUserPerm | ExeOwnerPerm;
        }
        return ret;
    }

    QString fileName(FileName file = DefaultName) const override
    {
        switch (file) {
        case BaseName: {
            const int slash = m_file.lastIndexOf(QLatin1Char('/'));
            return slash >= 0 ? m_file.mid(slash + 1) : m_file;
        }
        case PathName:
        case AbsolutePathName: {
            const int slash = m_file.lastIndexOf(QLatin1Char('/'));
            return slash > 0 ? m_file.left(slash) : QStringLiteral("/");
        }
        case AbsoluteName:
        case CanonicalName:
        case DefaultName:
        default:
            return m_file;
        }
    }

    QDateTime fileTime(QFile::FileTime) const override { return {}; }

    QStringList entryList(QDir::Filters filters,
                          const QStringList& filterNames) const override
    {
        QStringList names = tipFsReaddir(m_file.toUtf8());
        QStringList out;
        for (const QString& name : names) {
            if (!filterNames.isEmpty()) {
                bool match = false;
                for (const QString& pat : filterNames) {
                    if (QDir::match(pat, name)) {
                        match = true;
                        break;
                    }
                }
                if (!match)
                    continue;
            }
            Q_UNUSED(filters);
            out.append(name);
        }
        return out;
    }

    qint64 read(char* data, qint64 maxlen) override
    {
        if (m_fd < 0 || maxlen <= 0)
            return -1;
        const int n =
            tipFsRead(m_fd, data, static_cast<int>(qMin(maxlen, qint64(1 << 20))));
        if (n < 0)
            return -1;
        m_pos += n;
        return n;
    }

    qint64 write(const char* data, qint64 len) override
    {
        if (m_fd < 0 || len <= 0)
            return -1;
        qint64 written = 0;
        while (written < len) {
            const int chunk =
                static_cast<int>(qMin(len - written, qint64(1 << 20)));
            const int n = tipFsWrite(m_fd, data + written, chunk);
            if (n <= 0)
                return written > 0 ? written : -1;
            written += n;
            m_pos += n;
        }
        return written;
    }

private:
    QString m_file;
    int m_fd = -1;
    qint64 m_pos = 0;
    qint64 m_size = -1;
};

class TipFsHandler final : public QAbstractFileEngineHandler
{
public:
    std::unique_ptr<QAbstractFileEngine> create(const QString& fileName) const override
    {
        if (!tipFsLive())
            return {};
        QString path = fileName;
        if (path.startsWith(QLatin1String("file:")))
            path = QUrl(path).toLocalFile();
        if (!tipFsPathOwned(path))
            return {};
        return std::make_unique<TipFsEngine>(path);
    }
};

TipFsHandler* g_handler = nullptr;

} // namespace

bool tibiaRegisterTipFsFileEngine()
{
    if (!tipFsLive())
        return false;
    if (!g_handler)
        g_handler = new TipFsHandler();
    EM_ASM({ Module.qtclientFsEngine = 1; });
    return true;
}

// Assemble the whole file in JS (proven hash-correct), then one HEAP copy.
// Chunked tipFsReadJs across JSPI suspends left multi-MB appearances/RCC
// truncated or zeroed for QFile::readAll even when the worker bytes were fine.
//
// Layout: [uint32 LE length][payload…] — length travels with the pointer so
// concurrent JSPI readFileAll calls cannot race a Module.__tipFsReadFileLen
// scratch field (same class of bug tipFsStat had with Module out-fields).
EM_ASYNC_JS(char*, tipFsReadFileAllJs, (const char* path), {
    var tip = Module.tipFs;
    if (!tip || !tip.readFileAsync)
        return 0;
    var p = UTF8ToString(path);
    var u8 = await tip.readFileAsync(p);
    if (!u8)
        return 0;
    var len = u8.byteLength | 0;
    var ptr = _malloc(4 + (len ? len : 0));
    if (!ptr)
        return 0;
    if (typeof updateMemoryViews === 'function')
        updateMemoryViews();
    var heap = Module.HEAPU8 || HEAPU8;
    heap[ptr] = len & 255;
    heap[ptr + 1] = (len >> 8) & 255;
    heap[ptr + 2] = (len >> 16) & 255;
    heap[ptr + 3] = (len >> 24) & 255;
    if (len > 0)
        heap.set(u8, ptr + 4);
    return ptr;
});

QByteArray tibiaTipFsReadFile(const QString& path)
{
    if (!tipFsLive() || !tipFsPathOwned(path))
        return {};
    char* ptr = tipFsReadFileAllJs(path.toUtf8().constData());
    if (!ptr)
        return {};
    const auto* bytes = reinterpret_cast<const unsigned char*>(ptr);
    const int len = int(bytes[0]) | (int(bytes[1]) << 8) | (int(bytes[2]) << 16)
        | (int(bytes[3]) << 24);
    QByteArray out;
    if (len > 0)
        out = QByteArray(ptr + 4, len);
    std::free(ptr);
    return out;
}

#endif // __EMSCRIPTEN__
