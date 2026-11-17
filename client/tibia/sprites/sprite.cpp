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

    SpriteSheet* sheet = sheetForSprite(spriteId, load);
    if (!sheet) {
        // Non-sheet / missing id — CIP TSingleSprite.
        (void)TSingleSprite(spriteId);
        return TSpriteInSpritesheet(spriteId, nullptr);
    }
    if (!sheet->data && load) {
        (void)sync.decompress(sheet);
    }
    return TSpriteInSpritesheet(spriteId, sheet);
}

bool TSpritesheetDecompressorSynchronous::decompress(SpriteSheet* sheet) const
{
    if (!sheet)
        return false;
    bool loading = false;
    const SpriteSheetPtr held = tibia::assets::sprites().sheetBySpriteId(sheet->firstId, loading, true);
    return held && held->data != nullptr;
}

}
