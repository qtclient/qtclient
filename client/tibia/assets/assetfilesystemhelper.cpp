#include "assets/assetfilesystemhelper.h"

#include "filesystemhelper.h"
#include "localfileinfo.h"
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

void TAssetFileSystemHelper::removeAssetFile(const QString& fileName) const
{
    fileSystem().removeFile(m_assetsDirectory.filePath(fileName));
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

void TAssetFileSystemHelper::forEachLocalFileDecideWhetherItShouldBeRetained(
    std::function<bool(const QString&)> shouldBeRetained) const
{
    fileSystem().forEachFileInDirectory(assetsDirectory().absolutePath(),
                                        [&shouldBeRetained, this](const tibia::shared::TLocalFileInfo& file) {
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
    return fileSystem().readFile(getFilePath(asset));
}

QString TAssetFileSystemHelper::writeAsset(const TAsset& asset, const QByteArray& data) const
{
    QString path = getFilePath(asset);
    fileSystem().writeFile(path, data);
    return path;
}

std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> TAssetFileSystemHelper::openAsset(const TAsset& asset) const
{
    return fileSystem().openReadOnlyMemoryBlob(getFilePath(asset));
}

}
