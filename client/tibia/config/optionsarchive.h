#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace tibia::config {

/// One file of an options / minimap export, its name relative to the
/// client's data directory ("conf/clientoptions.json", "minimap/...").
struct TArchiveEntry
{
    QString name;
    QByteArray data;
};

/// The zip archives of the options page's export / import buttons (CIP:
/// FUN_100d30400 writes entries {name, data} through FUN_1015e59e0, import
/// reads them back with FUN_100d31f00).
namespace optionsarchive {

/// The files under `directories` of the data directory, recursively (CIP
/// FUN_100d30180 names them relative to it).
QList<TArchiveEntry> collect(const QString& dataDirectory, const QStringList& directories);
bool write(const QString& path, const QList<TArchiveEntry>& entries);
/// Empty on a missing or broken archive.
QList<TArchiveEntry> read(const QString& path);

}

}
