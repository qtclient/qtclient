#pragma once

#include "sprites/spritefromspritesheetinfo.h"
#include "sprites/spritesheetcache.h"

#include <QObject>

#include <memory>

namespace tibia {
namespace appearances {
class TAppearanceInstanceSpriteInfo;
class TCreatureMarkAppearanceInstanceSpriteInfo;
class TWorldmapHighlightBaseplateSpriteInfo;
class TLayeredAppearanceInstanceSpriteInfo;
class TMaskedAppearanceInstanceSpriteInfo;
struct TOutfitColors;
}
namespace sprites {

class TSprite;
class TSpritesheetManager;

/// Turns sprite descriptions into drawable sprites (CIP sprites::TSpriteResolver).
class TSpriteResolver : public QObject
{
    Q_OBJECT
public:
    explicit TSpriteResolver(std::shared_ptr<TSpritesheetManager> spritesheets, QObject* parent = nullptr);

    /// The sprite of an appearance, cut from its spritesheet; null while the
    /// spritesheet is not decompressed yet.
    virtual std::shared_ptr<TSprite> resolveSpritesheetSprite(const TSpriteFromSpritesheetInfo& info) const;
    /// A whole asset spritesheet as one sprite drawn at `scale`, not cached; null
    /// while it is not decompressed yet.
    virtual std::shared_ptr<TSprite> resolveAssetSpritesheet(const TSpritesheetID& spritesheetId, float scale) const;
    /// CIP v13: the sprite an instance's sprite info draws, by its kind
    /// (spritesheet, masked, layered); null for other kinds and while a
    /// spritesheet is not decompressed yet.
    virtual std::shared_ptr<TSprite> resolveSpriteInfo(const appearances::TAppearanceInstanceSpriteInfo& info) const;
    /// CIP v17: the frames of a creature mark.
    virtual std::shared_ptr<TSprite> resolveCreatureMark(const appearances::TCreatureMarkAppearanceInstanceSpriteInfo& info) const;
    /// CIP v18: the highlight baseplate frame.
    virtual std::shared_ptr<TSprite>
    resolveHighlightBaseplate(const appearances::TWorldmapHighlightBaseplateSpriteInfo& info) const;
    /// CIP v15: the sprite coloured through its mask with the outfit's colours.
    virtual std::shared_ptr<TSprite> resolveMaskedSprite(const appearances::TMaskedAppearanceInstanceSpriteInfo& info) const;
    /// CIP v16: the layers drawn over each other; null when a layer is missing.
    virtual std::shared_ptr<TSprite> resolveLayeredSprite(const appearances::TLayeredAppearanceInstanceSpriteInfo& info) const;

    /// `sprite` multiplied by the colours its mask's yellow, red, green and blue
    /// pixels pick (head, body, legs, feet).
    static QImage colorizedOutfit(const QImage& sprite, const QImage& mask, const appearances::TOutfitColors& colors);
    /// `top` drawn over `bottom`.
    static QImage composed(const QImage& bottom, const QImage& top);

private:
    std::shared_ptr<TSpritesheetManager> m_spritesheets;
};

}
}
