#include "assets/assetfilesystemhelper.h"

#include "filesystemhelper.h"
#include "readonlymemoryblob.h"

namespace tibia::assets {

namespace {

const tibia::shared::TFileSystemHelper& fileSystem()
{
    static const tibia::shared::TFileSystemHelper helper;
    return helper;
}

}

TAssetFileSystemHelper::TAssetFileSystemHelper(const QDir& assetsDirectory, const QDir& soundsDirectory)
    : m_assetsDirectory(assetsDirectory)
    , m_soundsDirectory(soundsDirectory)
{
}

QString TAssetFileSystemHelper::getFilePath(const TAsset& asset) const
{
    return getFilePath(asset.type(), asset.file());
}

QString TAssetFileSystemHelper::getFilePath(const TAsset::EType type, const QString& fileName) const
{
    const bool sound = type == TAsset::Sounds || type == TAsset::Sound || type == 4;
    const QDir directory = sound ? m_soundsDirectory : m_assetsDirectory;
    QString path = directory.filePath(fileName);
    if (!fileSystem().fileExists(path)) {
        const QString packed = path + QStringLiteral(".lzma");
        if (fileSystem().fileExists(packed))
            path = packed;
    }
    return path;
}

std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> TAssetFileSystemHelper::openAsset(const TAsset& asset) const
{
    return fileSystem().openReadOnlyMemoryBlob(getFilePath(asset));
}

}
