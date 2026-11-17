#pragma once

#include "sprites/sprite.h"
#include "sprites/spritesheetcache.h"

#include <QRect>

namespace tibia {
namespace sprites {

/// A sprite cut from a decompressed spritesheet (CIP sprites::TSpriteInSpritesheet).
class TSpriteInSpritesheet final : public TSprite
{
public:
    /// `cell` is the sprite's rectangle in the spritesheet, `area` the part of the
    /// cell to draw (relative to the cell). An opaque sprite drops its alpha channel.
    TSpriteInSpritesheet(double scale, TSpritesheetPointer spritesheet, const QRect& cell, const QRect& area, bool opaque);

    QImage getImage() const override;

private:
    TSpritesheetPointer m_spritesheet;
    QRect m_cell;
    QRect m_area;
    bool m_opaque = false;
};

}
}
