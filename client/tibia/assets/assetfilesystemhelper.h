#pragma once

#include "assets/asset.h"

#include <QByteArray>
#include <QDir>

#include <functional>
#include <memory>

namespace tibia::shared {
class TReadOnlyMemoryBlob;
class TTibiaFileSystemHelper;
}

namespace tibia {
namespace assets {

/// Where the asset files live and how they are read (CIP assets::TAssetFileSystemHelper).
class TAssetFileSystemHelper
{
public:
    /// Keeps the helper and takes its assets and sounds directories (empty
    /// directories without a helper).
    explicit TAssetFileSystemHelper(std::shared_ptr<tibia::shared::TTibiaFileSystemHelper> tibiaFileSystemHelper);
    virtual ~TAssetFileSystemHelper() = default;

    /// Deletes the file `fileName` of the assets directory.
    virtual void removeAssetFile(const QString& fileName) const;

    /// The path of an asset's file.
    virtual QString getFilePath(const TAsset& asset) const;
    /// The path of a file of an asset type: sounds live in the sounds directory, the
    /// rest in the assets directory; a missing file falls back to its ".lzma" packed copy.
    virtual QString getFilePath(TAsset::EType type, const QString& fileName) const;
    /// The directory the assets live in (asked from the helper).
    virtual QDir assetsDirectory() const;
    /// The directory the sounds live in (asked from the helper).
    virtual QDir soundsDirectory() const;
    /// Asks `shouldBeRetained` about the name of every file of the assets
    /// directory and deletes the files it does not retain.
    virtual void forEachLocalFileDecideWhetherItShouldBeRetained(std::function<bool(const QString&)> shouldBeRetained) const;
    /// Deletes the asset's file.
    virtual void removeAsset(const TAsset& asset) const;
    /// The bytes of the asset's file.
    virtual QByteArray readAsset(const TAsset& asset) const;
    /// The asset's bytes (unpacked when the file is LZMA packed); throws when it is missing.
    virtual std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> openAsset(const TAsset& asset) const;
    /// Writes `data` to the asset's file; returns its path.
    virtual QString writeAsset(const TAsset& asset, const QByteArray& data) const;

private:
    std::shared_ptr<tibia::shared::TTibiaFileSystemHelper> m_pTibiaFileSystemHelper;
    QDir m_assetsDirectory;
    QDir m_soundsDirectory;
};

}
}
