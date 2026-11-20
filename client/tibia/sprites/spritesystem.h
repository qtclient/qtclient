#pragma once

#include "sprites/spritefromspritesheetinfo.h"
#include "sprites/spritesheetsmetadatamanager.h"

#include <QImage>

#include <memory>

namespace tibia::assets {
class TAssetAsyncLoader;
class TContentCatalog;
}

namespace tibia {
namespace sprites {

class TSpriteResolver;
class TSpritesheetDecompressor;
class TSpritesheetManager;

/// Host: the client's sprite objects (CIP keeps them on the game client): the
/// catalog's spritesheet metadata, the spritesheet manager with its cache and
/// decompressor, the sprite resolver and the asset loader that reads the
/// spritesheet files in the background. Created on the GUI thread.
class TSpriteSystem
{
public:
    static TSpriteSystem& instance();

    /// Forgets every spritesheet and stops loading.
    void clear();
    /// Takes the catalog's spritesheets and starts loading their files from `assetsDirectory`.
    void start(const tibia::assets::TContentCatalog& catalog, const QString& assetsDirectory);

    /// Null before start.
    const std::shared_ptr<TSpritesheetsMetadataManager>& metadata() const { return m_metadata; }
    TSpritesheetManager& spritesheets() { return *m_spritesheets; }
    TSpriteResolver& resolver() { return *m_resolver; }
    /// Null before start.
    tibia::assets::TAssetAsyncLoader* assetLoader() { return m_assetLoader.get(); }
    /// The decompressor of asset images (store images decode through it too).
    const std::shared_ptr<TSpritesheetDecompressor>& decompressor() const { return m_decompressor; }

    /// Where a sprite id lives; invalid metadata for an unknown sprite. Thread safe.
    TSpriteFromSpritesheetInfo spriteInfo(uint32_t spriteId, bool opaque = false);
    /// The size of a sprite from its spritesheet's layout (empty when unknown).
    QSize spriteSize(uint32_t spriteId);
    /// A sprite's image; null while its spritesheet is not decompressed yet
    /// (`loading`) or for an unknown sprite. Thread safe.
    QImage spriteImage(uint32_t spriteId, bool& loading);
    /// Host: like spriteImage, but decodes a loaded, uncached sheet on the calling
    /// thread (TSpritesheetManager::decodeNow). For worker threads only.
    QImage spriteImageDecoded(uint32_t spriteId, bool& loading);
    /// The sprite's spritesheet is decompressed (no load is started).
    bool isSpriteReady(uint32_t spriteId);

    /// Spritesheets the cache holds: half the physical memory, at most 512 MB, at least 350.
    static uint32_t defaultCacheCapacity();

private:
    TSpriteSystem();

    std::shared_ptr<TSpritesheetsMetadataManager> m_metadata;
    std::shared_ptr<TSpritesheetDecompressor> m_decompressor;
    std::shared_ptr<TSpritesheetManager> m_spritesheets;
    std::unique_ptr<TSpriteResolver> m_resolver;
    std::unique_ptr<tibia::assets::TAssetAsyncLoader> m_assetLoader;
};

}
}
