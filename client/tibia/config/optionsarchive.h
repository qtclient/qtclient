#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace tibia {
namespace config {

/// One file of an options / minimap export, its name relative to the
/// client's data directory ("conf/clientoptions.json", "minimap...").
struct TArchiveEntry
{
    QString name;
    QByteArray data;
};

/// The zip archives of the options page's export / import buttons (CIP:
/// writes entries {name, data} through , import
/// reads them back with).
namespace optionsarchive {

/// The files under `directories` of the data directory, recursively (CIP
/// names them relative to it).
QList<TArchiveEntry> collect(const QString& dataDirectory, const QStringList& directories);
/// Writes the archive of entries to path through the file system helper,
/// which throws std::runtime_error when the file cannot be written.
void write(const QString& path, const QList<TArchiveEntry>& entries);
/// Every entry of a zip archive, directory entries ("name/") included;
/// nullopt when the archive cannot be unpacked.
std::optional<QList<TArchiveEntry>> unpack(const QByteArray& archive);
/// The file entries; empty on a missing or broken archive.
QList<TArchiveEntry> read(const QString& path);

}

}
}
