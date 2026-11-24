// The parts of shared::TTibiaFileSystemHelper the asset tests reach, without
// the client's data tree: the data directory is the given base directory and
// the assets and sounds directories are <base>/assets and <base>/sounds
// unless set.

#include "filesystemhelper.h"
#include "shared/tibiafilesystemhelper.h"

#include <utility>

namespace tibia::shared {

TTibiaFileSystemHelper::TTibiaFileSystemHelper(const QDir& baseDirectory, std::shared_ptr<TFileSystemHelper> fileSystemHelper)
    : m_dataDirectory(baseDirectory)
    , m_baseDirectory(baseDirectory)
    , m_fileSystemHelper(std::move(fileSystemHelper))
{
}

TTibiaFileSystemHelper::~TTibiaFileSystemHelper() = default;

QDir TTibiaFileSystemHelper::getAssetsDirectory() const
{
    return m_hostAssetsDirectory ? *m_hostAssetsDirectory : QDir(m_dataDirectory.filePath(QStringLiteral("assets")));
}

QDir TTibiaFileSystemHelper::getSoundsDirectory() const
{
    return m_hostSoundsDirectory ? *m_hostSoundsDirectory : QDir(m_dataDirectory.filePath(QStringLiteral("sounds")));
}

void TTibiaFileSystemHelper::setAssetsDirectories(const QDir& assetsDirectory, const QDir& soundsDirectory)
{
    m_hostAssetsDirectory = assetsDirectory;
    m_hostSoundsDirectory = soundsDirectory;
}

std::shared_ptr<TFileSystemHelper> TTibiaFileSystemHelper::getFileSystemHelper() const { return m_fileSystemHelper; }

}
