#pragma once

#include "sprites/spritefromspritesheetinfo.h"
#include "sprites/spritesheetcache.h"

#include <QObject>

#include <memory>

namespace tibia {
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

private:
    std::shared_ptr<TSpritesheetManager> m_spritesheets;
};

}
}
