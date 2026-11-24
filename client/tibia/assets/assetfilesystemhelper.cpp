#include "assets/assetfilesystemhelper.h"

#include "filesystemhelper.h"
#include "localfileinfo.h"
#include "readonlymemoryblob.h"
#include "shared/tibiafilesystemhelper.h"

#include <utility>

namespace tibia::assets {

TAssetFileSystemHelper::TAssetFileSystemHelper(std::shared_ptr<tibia::shared::TTibiaFileSystemHelper> tibiaFileSystemHelper)
    : m_pTibiaFileSystemHelper(std::move(tibiaFileSystemHelper))
    , m_assetsDirectory(QString())
    , m_soundsDirectory(QString())
{
    if (m_pTibiaFileSystemHelper) {
        m_assetsDirectory = m_pTibiaFileSystemHelper->getAssetsDirectory();
        m_soundsDirectory = m_pTibiaFileSystemHelper->getSoundsDirectory();
    }
}

QDir TAssetFileSystemHelper::assetsDirectory() const { return m_pTibiaFileSystemHelper->getAssetsDirectory(); }

QDir TAssetFileSystemHelper::soundsDirectory() const { return m_pTibiaFileSystemHelper->getSoundsDirectory(); }

void TAssetFileSystemHelper::removeAssetFile(const QString& fileName) const
{
    m_pTibiaFileSystemHelper->getFileSystemHelper()->removeFile(m_assetsDirectory.filePath(fileName));
}

QString TAssetFileSystemHelper::getFilePath(const TAsset& asset) const
{
    return getFilePath(asset.type(), asset.file());
}

QString TAssetFileSystemHelper::getFilePath(const TAsset::EType type, const QString& fileName) const
{
    const bool sound = type == TAsset::Sounds || type == TAsset::Sound || type == TAsset::SoundCatalog;
    const QDir directory = sound ? m_soundsDirectory : m_assetsDirectory;
    QString path = directory.filePath(fileName);
    if (!m_pTibiaFileSystemHelper->getFileSystemHelper()->fileExists(path)) {
        const QString packed = path + QStringLiteral(".lzma");
        if (m_pTibiaFileSystemHelper->getFileSystemHelper()->fileExists(packed))
            path = packed;
    }
    return path;
}

void TAssetFileSystemHelper::forEachLocalFileDecideWhetherItShouldBeRetained(
    std::function<bool(const QString&)> shouldBeRetained) const
{
    m_pTibiaFileSystemHelper->getFileSystemHelper()->forEachFileInDirectory(
        assetsDirectory().absolutePath(), [&shouldBeRetained, this](const tibia::shared::TLocalFileInfo& file) {
            if (!shouldBeRetained(file.getFileName()))
                removeAssetFile(file.getFileName());
        });
}

void TAssetFileSystemHelper::removeAsset(const TAsset& asset) const
{
    removeAssetFile(asset.file());
}

QByteArray TAssetFileSystemHelper::readAsset(const TAsset& asset) const
{
    return m_pTibiaFileSystemHelper->getFileSystemHelper()->readFile(getFilePath(asset));
}

QString TAssetFileSystemHelper::writeAsset(const TAsset& asset, const QByteArray& data) const
{
    QString path = getFilePath(asset);
    m_pTibiaFileSystemHelper->getFileSystemHelper()->writeFile(path, data);
    return path;
}

std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> TAssetFileSystemHelper::openAsset(const TAsset& asset) const
{
    return m_pTibiaFileSystemHelper->getFileSystemHelper()->openReadOnlyMemoryBlob(getFilePath(asset));
}

}
