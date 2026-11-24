#pragma once

#include "config/optionsarchive.h"

#include <QDir>
#include <QList>
#include <QString>

#include <memory>
#include <stdexcept>
#include <vector>

namespace tibia {
namespace shared {
class TFileSystemHelper;
}
namespace config {

/// CIP `config::TConfigBackupReader`: an options backup (zip) read for the
/// options menu's import. Only entries under the allowed top directories
/// ("conf", "characterdata", "minimap") are extracted.
class TConfigBackupReader
{
public:
    /// Thrown by the constructor; the message is the translated reason.
    class TConfigBackupException : public std::runtime_error
    {
    public:
        explicit TConfigBackupException(const QString& message);
    };

    /// Reads and unpacks `backupFilePath`. Throws TConfigBackupException when
    /// the archive cannot be unpacked or holds a directory entry; entry names
    /// are cleaned paths.
    TConfigBackupReader(std::shared_ptr<shared::TFileSystemHelper> fileSystemHelper, const QString& backupFilePath,
                        std::vector<QString> allowedDirectories);

    /// Writes every allowed entry below `directory`, creating its directory.
    void extractTo(const QDir& directory) const;
    /// Whether an entry's name contains "minimap".
    bool containsMinimap() const;
    /// Whether an entry's name contains "conf" or "characterdata".
    bool containsOptions() const;

private:
    /// Whether `name` starts with "<allowed directory>/".
    bool isAllowed(const QString& name) const;

    std::shared_ptr<shared::TFileSystemHelper> m_fileSystemHelper;
    QList<TArchiveEntry> m_entries;
    std::vector<QString> m_allowedDirectories;
};

}
}
