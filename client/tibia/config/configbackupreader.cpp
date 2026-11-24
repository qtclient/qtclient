#include "configbackupreader.h"

#include "filesystemhelper.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <utility>

namespace tibia::config {

TConfigBackupReader::TConfigBackupException::TConfigBackupException(const QString& message)
    : std::runtime_error(message.toStdString())
{
}

TConfigBackupReader::TConfigBackupReader(std::shared_ptr<shared::TFileSystemHelper> fileSystemHelper,
                                         const QString& backupFilePath, std::vector<QString> allowedDirectories)
    : m_fileSystemHelper(std::move(fileSystemHelper))
    , m_allowedDirectories(std::move(allowedDirectories))
{
    std::optional<QList<TArchiveEntry>> entries = optionsarchive::unpack(m_fileSystemHelper->readFile(backupFilePath));
    if (!entries)
        throw TConfigBackupException(qtTrId("configbackup_unpack_failed"));
    m_entries = std::move(*entries);
    for (TArchiveEntry& entry : m_entries) {
        if (entry.name.endsWith(QLatin1Char('/')) || entry.name.endsWith(QLatin1Char('\\')))
            throw TConfigBackupException(qtTrId("configbackup_unsupported_format"));
        entry.name = QDir::cleanPath(entry.name);
    }
}

void TConfigBackupReader::extractTo(const QDir& directory) const
{
    for (const TArchiveEntry& entry : m_entries) {
        if (!isAllowed(entry.name))
            continue;
        const QString path = m_fileSystemHelper->getPathInsideDirectory(directory, entry.name);
        m_fileSystemHelper->createDirectory(QFileInfo(path).dir().path());
        m_fileSystemHelper->writeFile(path, entry.data);
    }
}

bool TConfigBackupReader::containsMinimap() const
{
    for (const TArchiveEntry& entry : m_entries) {
        if (entry.name.contains(QLatin1String("minimap")))
            return true;
    }
    return false;
}

bool TConfigBackupReader::containsOptions() const
{
    for (const TArchiveEntry& entry : m_entries) {
        if (entry.name.contains(QLatin1String("conf")) || entry.name.contains(QLatin1String("characterdata")))
            return true;
    }
    return false;
}

bool TConfigBackupReader::isAllowed(const QString& name) const
{
    for (const QString& directory : m_allowedDirectories) {
        if (name.startsWith(directory + QLatin1Char('/')))
            return true;
    }
    return false;
}

}
