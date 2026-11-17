#pragma once

#include "assets/asset.h"

#include <QDir>

#include <memory>

namespace tibia::shared {
class TReadOnlyMemoryBlob;
}

namespace tibia {
namespace assets {

/// Where the asset files live and how they are read (CIP assets::TAssetFileSystemHelper).
class TAssetFileSystemHelper
{
public:
    TAssetFileSystemHelper(const QDir& assetsDirectory, const QDir& soundsDirectory);
    virtual ~TAssetFileSystemHelper() = default;

    /// The path of an asset's file.
    virtual QString getFilePath(const TAsset& asset) const;
    /// The path of a file of an asset type: sounds live in the sounds directory, the
    /// rest in the assets directory; a missing file falls back to its ".lzma" packed copy.
    virtual QString getFilePath(TAsset::EType type, const QString& fileName) const;
    /// The asset's bytes (unpacked when the file is LZMA packed); throws when it is missing.
    virtual std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> openAsset(const TAsset& asset) const;

private:
    QDir m_assetsDirectory;
    QDir m_soundsDirectory;
};

}
}
