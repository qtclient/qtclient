#pragma once

#include "sprites/spritefromspritesheetinfo.h"
#include "sprites/spriteidentifier.h"
#include "sprites/spritesheetcache.h"

#include "input/gameactionptr.h"

#include <QImage>
#include <QObject>
#include <QTimer>

#include <list>
#include <memory>

namespace tibia {
namespace minimap {
class TMinimapTileManager;
}
namespace appearances {
class TAppearanceInstanceSpriteInfo;
class TCreatureMarkAppearanceInstanceSpriteInfo;
class TWorldmapHighlightBaseplateSpriteInfo;
class TLayeredAppearanceInstanceSpriteInfo;
class TMaskedAppearanceInstanceSpriteInfo;
struct TOutfitColors;
}
namespace sprites {

class TImageResourceSpriteIdentifier;
class TMinimapTileSpritesheetIdentifier;
class TSprite;
class TSpritesheetManager;

/// Turns sprite descriptions into drawable sprites (CIP sprites::TSpriteResolver).
class TSpriteResolver : public QObject
{
    Q_OBJECT
public:
    explicit TSpriteResolver(std::shared_ptr<TSpritesheetManager> spritesheets, QObject* parent = nullptr);

    /// The session's minimap tiles (CIP takes them at construction; qtclient's
    /// resolver outlives sessions, so the minimap installs and removes them).
    void setMinimapTileManager(std::shared_ptr<minimap::TMinimapTileManager> tileManager);

    /// The sprite of an appearance, cut from its spritesheet; null while the
    /// spritesheet is not decompressed yet.
    virtual std::shared_ptr<TSprite> resolveSpritesheetSprite(const TSpriteFromSpritesheetInfo& info) const;
    /// A whole asset spritesheet as one sprite drawn at `scale`, not cached; null
    /// while it is not decompressed yet.
    virtual std::shared_ptr<TSprite> resolveAssetSpritesheet(const TSpritesheetID& spritesheetId, float scale) const;
    /// CIP v12: the sprite of an identifier by its kind: image resources and
    /// sprite infos (both marked as sprites of a resource), asset spritesheets,
    /// minimap tiles and effect wrappers; other resources give null. Null while
    /// a spritesheet is not decompressed yet.
    std::shared_ptr<TSprite> resolve(const ISpriteIdentifier& identifier) const;
    /// CIP v21: the tile's colour image or unexplored mask at the identifier's
    /// scale; a black 256x256 image while the tile is not there yet.
    virtual std::shared_ptr<TSprite> resolveMinimapTile(const TMinimapTileSpritesheetIdentifier& identifier) const;
    /// CIP v19: the identifier's rectangle cut out of its image file, at scale 1
    /// (a missing file gives an empty sprite).
    virtual std::shared_ptr<TSprite> resolveImageResource(const TImageResourceSpriteIdentifier& identifier) const;
    /// CIP v22: the wrapped identifier's sprite with its effect applied, keeping
    /// its scale and flags.
    std::shared_ptr<TSprite> resolveEffectWrapper(const TEffectWrapperSpritesheetIdentifier& identifier) const;
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
    /// Each colour channel half way to `color`, alpha scaled by its alpha (pixels
    /// that end up fully transparent are cleared).
    static QImage blended(const QImage& image, const QColor& color);
    /// Every pixel `color`, its alpha the pixel's alpha scaled by the colour's.
    static QImage highlighted(const QImage& image, const QColor& color);

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);

public Q_SLOTS:
    /// RESOLVE_SPRITE: the identifier waits until its sprite can be made, then
    /// SPRITE_RESOLVED carries the sprite.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    /// Publishes SPRITE_RESOLVED for every waiting identifier whose sprite can be
    /// made now; the others are tried again on the next event loop pass.
    void resolvePendingSprites();

    /// What a minimap tile shows until it is loaded.
    QImage m_emptyMinimapTileImage;
    std::shared_ptr<TSpritesheetManager> m_spritesheets;
    std::shared_ptr<minimap::TMinimapTileManager> m_minimapTileManager;
    QTimer m_retryTimer;
    /// A list: publishing a resolved sprite may queue another identifier.
    std::list<ISpriteIdentifierPtr> m_pendingIdentifiers;
};

}
}
