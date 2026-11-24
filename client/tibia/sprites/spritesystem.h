#pragma once

#include "sprites/spritefromspritesheetinfo.h"
#include "sprites/spritesheetsmetadatamanager.h"

#include <QImage>

#include <memory>

namespace tibia::assets {
class TAssetAsyncLoader;
}

namespace tibia {
namespace sprites {

class TSpriteResolver;
class TSpritesheetDecompressor;
class TSpritesheetManager;

/// Host: the client's sprite objects (CIP keeps them on the game client): the
/// catalog's spritesheet metadata, the game client's spritesheet manager and
/// sprite resolver, for the code that reaches them process-wide. The game
/// client hands its spritesheet manager and resolver in; its asset manager
/// loads the spritesheets and hands the metadata and its loader in. Until the
/// client does, the sprite system's own manager and resolver stand in.
/// Created on the GUI thread.
class TSpriteSystem
{
public:
    static TSpriteSystem& instance();

    /// Forgets every spritesheet, the metadata and the loader.
    void clear();
    /// The game client's spritesheet manager (CIP: the game client base owns
    /// it); null goes back to the sprite system's own.
    void setSpritesheetManager(std::shared_ptr<TSpritesheetManager> spritesheets);
    /// The content catalog's spritesheet metadata, once the asset manager has read it.
    void setMetadata(std::shared_ptr<TSpritesheetsMetadataManager> metadata);
    /// The game client's sprite resolver (CIP keeps it on the game client);
    /// null goes back to the sprite system's own.
    void setResolver(std::shared_ptr<TSpriteResolver> resolver);
    /// The game client's asset loader (the Cyclopedia map's own storage asks it for map images).
    void setAssetLoader(std::shared_ptr<tibia::assets::TAssetAsyncLoader> assetLoader);

    /// Null before the asset manager has read the content catalog.
    const std::shared_ptr<TSpritesheetsMetadataManager>& metadata() const { return m_metadata; }
    /// The game client's spritesheet manager, else the sprite system's own.
    std::shared_ptr<TSpritesheetManager> sharedSpritesheets() const;
    TSpriteResolver& resolver() { return *m_resolver; }
    /// Null before the game client made its loader.
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
    /// Stands in until the game client hands its manager in.
    std::shared_ptr<TSpritesheetManager> m_ownSpritesheets;
    std::shared_ptr<TSpritesheetManager> m_spritesheets;
    /// Resolves sprites until the game client hands its resolver in.
    std::shared_ptr<TSpriteResolver> m_ownResolver;
    std::shared_ptr<TSpriteResolver> m_resolver;
    std::shared_ptr<tibia::assets::TAssetAsyncLoader> m_assetLoader;
};

}
}
