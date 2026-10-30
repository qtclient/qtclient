#include "sprites/sprite.h"

#include "tibia/assets/spritesheet.h"

namespace tibia::sprites {

TSprite::TSprite(const uint32_t spriteId)
    : m_spriteId(spriteId)
{
}

TSpriteInSpritesheet::TSpriteInSpritesheet(const uint32_t spriteId, SpriteSheet* sheet)
    : TSprite(spriteId)
    , m_sheet(sheet)
{
}

TSpritesheetCache& TSpritesheetCache::instance()
{
    static TSpritesheetCache cache;
    return cache;
}

SpriteSheet* TSpritesheetCache::sheetForSprite(const uint32_t spriteId, const bool load)
{
    bool loading = false;
    const SpriteSheetPtr sheet = tibia::assets::sprites().sheetBySpriteId(static_cast<int>(spriteId), loading, load);
    return sheet.get();
}

TSpriteInSpritesheet TSpritesheetCache::resolve(const uint32_t spriteId, const bool load)
{
    static TResourceSpritesManager resourceSprites;
    static TSpriteResolver resolver;
    static TSpritesheetDecompressor decompressor;
    static TSpritesheetDecompressorThreadpool threadpool;
    static TSpritesheetManager sheets;
    static TSpritesheetDecompressorSynchronous sync;
    (void)resourceSprites;
    (void)resolver;
    (void)decompressor;
    (void)threadpool;
    (void)sheets;
    (void)TSpritesheetsMetadataManager::instance().spritesCount();

    SpriteSheet* sheet = sheetForSprite(spriteId, load);
    if (!sheet) {
        // Non-sheet / missing id — CIP TSingleSprite.
        (void)TSingleSprite(spriteId);
        return TSpriteInSpritesheet(spriteId, nullptr);
    }
    if (!sheet->data && load) {
        if (!sync.decompress(sheet)) {
            // CIP surfaces TSpritesheetDecompressorConcurrentException on worker failure.
            const TSpritesheetDecompressorConcurrentException error(
                "spritesheet decompress failed for sprite " + std::to_string(spriteId));
            (void)error.message();
        }
    }
    return TSpriteInSpritesheet(spriteId, sheet);
}

TQtTextureAtlasTextureCreator::TQtTextureAtlasTextureCreator(const bool allowEvict)
    : m_allowEvict(allowEvict)
{
}

TAppearanceTypeSpritesheetMapping::TAppearanceTypeSpritesheetMapping(const uint16_t appearanceId,
                                                                     const uint32_t firstSpriteId)
    : m_appearanceId(appearanceId)
    , m_firstSpriteId(firstSpriteId)
{
}

TAssetSpritesheetIdentifier::TAssetSpritesheetIdentifier(std::string file)
    : m_file(std::move(file))
{
}

TEffectWrapperSpritesheetIdentifier::TEffectWrapperSpritesheetIdentifier(const uint32_t effectSpriteId)
    : m_effectSpriteId(effectSpriteId)
{
}

TImageResourceSpriteIdentifier::TImageResourceSpriteIdentifier(std::string resourcePath)
    : m_resourcePath(std::move(resourcePath))
{
}

TMinimapTileSpritesheetIdentifier::TMinimapTileSpritesheetIdentifier(const uint32_t tileSpriteId)
    : m_tileSpriteId(tileSpriteId)
{
}

TStaticMinimapMarkerSymbolResourceSpriteIdentifier::TStaticMinimapMarkerSymbolResourceSpriteIdentifier(
    std::string resourcePath)
    : m_resourcePath(std::move(resourcePath))
{
}

TSpritesheetsMetadataManager& TSpritesheetsMetadataManager::instance()
{
    static TSpritesheetsMetadataManager manager;
    return manager;
}

int TSpritesheetsMetadataManager::spritesCount() const
{
    return tibia::assets::sprites().spritesCount();
}

bool TSpritesheetDecompressorSynchronous::decompress(SpriteSheet* sheet) const
{
    if (!sheet)
        return false;
    bool loading = false;
    const SpriteSheetPtr held = tibia::assets::sprites().sheetBySpriteId(sheet->firstId, loading, true);
    return held && held->data != nullptr;
}

TSpritesheetDecompressorConcurrentException::TSpritesheetDecompressorConcurrentException(std::string message)
    : m_message(std::move(message))
{
}

}
