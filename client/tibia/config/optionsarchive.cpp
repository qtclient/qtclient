#include "optionsarchive.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QtEndian>

#include <optional>

#include <zlib.h>

namespace tibia::config::optionsarchive {

namespace {

constexpr quint32 LocalHeader = 0x04034b50;
constexpr quint32 CentralHeader = 0x02014b50;
constexpr quint32 EndOfCentralDirectory = 0x06054b50;
constexpr quint16 Version = 20;
constexpr quint16 Utf8Names = 0x0800;
constexpr quint16 Stored = 0;
constexpr quint16 Deflated = 8;

void put16(QByteArray& out, const quint16 value)
{
    char bytes[2];
    qToLittleEndian(value, bytes);
    out.append(bytes, 2);
}

void put32(QByteArray& out, const quint32 value)
{
    char bytes[4];
    qToLittleEndian(value, bytes);
    out.append(bytes, 4);
}

quint16 get16(const QByteArray& in, const qsizetype at) { return qFromLittleEndian<quint16>(in.constData() + at); }
quint32 get32(const QByteArray& in, const qsizetype at) { return qFromLittleEndian<quint32>(in.constData() + at); }

QByteArray deflateRaw(const QByteArray& data)
{
    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return {};
    QByteArray out(static_cast<qsizetype>(deflateBound(&stream, static_cast<uLong>(data.size()))), Qt::Uninitialized);
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());
    const int result = deflate(&stream, Z_FINISH);
    out.resize(static_cast<qsizetype>(stream.total_out));
    deflateEnd(&stream);
    return result == Z_STREAM_END ? out : QByteArray();
}

std::optional<QByteArray> inflateRaw(const QByteArray& data, const quint32 size)
{
    z_stream stream{};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return std::nullopt;
    QByteArray out(static_cast<qsizetype>(size), Qt::Uninitialized);
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());
    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (result != Z_STREAM_END || stream.total_out != size)
        return std::nullopt;
    return out;
}

}

QList<TArchiveEntry> collect(const QString& dataDirectory, const QStringList& directories)
{
    const QDir root(dataDirectory);
    QList<TArchiveEntry> entries;
    for (const QString& directory : directories) {
        QDirIterator it(root.filePath(directory), QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QFile file(path);
            if (file.open(QIODevice::ReadOnly))
                entries.append({ root.relativeFilePath(path), file.readAll() });
        }
    }
    return entries;
}

bool write(const QString& path, const QList<TArchiveEntry>& entries)
{
    QByteArray archive;
    QByteArray central;
    for (const TArchiveEntry& entry : entries) {
        const QByteArray name = entry.name.toUtf8();
        const QByteArray deflated = deflateRaw(entry.data);
        const bool compress = !deflated.isEmpty() && deflated.size() < entry.data.size();
        const QByteArray& payload = compress ? deflated : entry.data;
        const auto crc = static_cast<quint32>(crc32(0, reinterpret_cast<const Bytef*>(entry.data.constData()), static_cast<uInt>(entry.data.size())));
        const auto offset = static_cast<quint32>(archive.size());

        put32(archive, LocalHeader);
        put16(archive, Version);
        put16(archive, Utf8Names);
        put16(archive, compress ? Deflated : Stored);
        put16(archive, 0);
        put16(archive, 0x21);
        put32(archive, crc);
        put32(archive, static_cast<quint32>(payload.size()));
        put32(archive, static_cast<quint32>(entry.data.size()));
        put16(archive, static_cast<quint16>(name.size()));
        put16(archive, 0);
        archive.append(name);
        archive.append(payload);

        put32(central, CentralHeader);
        put16(central, Version);
        put16(central, Version);
        put16(central, Utf8Names);
        put16(central, compress ? Deflated : Stored);
        put16(central, 0);
        put16(central, 0x21);
        put32(central, crc);
        put32(central, static_cast<quint32>(payload.size()));
        put32(central, static_cast<quint32>(entry.data.size()));
        put16(central, static_cast<quint16>(name.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, offset);
        central.append(name);
    }
    const auto centralOffset = static_cast<quint32>(archive.size());
    archive.append(central);
    put32(archive, EndOfCentralDirectory);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, static_cast<quint16>(entries.size()));
    put16(archive, static_cast<quint16>(entries.size()));
    put32(archive, static_cast<quint32>(central.size()));
    put32(archive, centralOffset);
    put16(archive, 0);

    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(archive) == archive.size() && file.commit();
}

QList<TArchiveEntry> read(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray archive = file.readAll();
    qsizetype end = archive.size() - 22;
    while (end >= 0 && get32(archive, end) != EndOfCentralDirectory)
        --end;
    if (end < 0)
        return {};
    const quint16 count = get16(archive, end + 10);
    qsizetype at = get32(archive, end + 16);

    QList<TArchiveEntry> entries;
    for (quint16 i = 0; i < count; ++i) {
        if (at + 46 > archive.size() || get32(archive, at) != CentralHeader)
            return {};
        const quint16 method = get16(archive, at + 10);
        const quint32 compressedSize = get32(archive, at + 20);
        const quint32 size = get32(archive, at + 24);
        const quint16 nameLength = get16(archive, at + 28);
        const quint16 extraLength = get16(archive, at + 30);
        const quint16 commentLength = get16(archive, at + 32);
        const quint32 local = get32(archive, at + 42);
        const QString name = QString::fromUtf8(archive.mid(at + 46, nameLength));
        at += 46 + nameLength + extraLength + commentLength;

        if (local + 30 > archive.size() || get32(archive, local) != LocalHeader)
            return {};
        const qsizetype data = local + 30 + get16(archive, local + 26) + get16(archive, local + 28);
        if (data + compressedSize > archive.size())
            return {};
        const QByteArray payload = archive.mid(data, compressedSize);
        if (name.endsWith(QLatin1Char('/')))
            continue;
        if (method == Stored) {
            entries.append({ name, payload });
        } else if (method == Deflated) {
            const std::optional<QByteArray> inflated = inflateRaw(payload, size);
            if (!inflated)
                return {};
            entries.append({ name, *inflated });
        } else {
            return {};
        }
    }
    return entries;
}

}
