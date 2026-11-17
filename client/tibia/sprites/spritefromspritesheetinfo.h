#pragma once

#include "sprites/spritesheetsmetadatamanager.h"

#include <QRect>

namespace tibia {
namespace sprites {

/// Where an appearance sprite lives: its spritesheet and its cell there
/// (CIP sprites::TSpriteFromSpritesheetInfo).
struct TSpriteFromSpritesheetInfo
{
    uint32_t spriteId = 0;
    TSpritesheetID spritesheetId;
    QRect cell;
    QSize size;
    bool opaque = false;

    TSpriteFromSpritesheetInfo() = default;
    TSpriteFromSpritesheetInfo(uint32_t id, TSpritesheetID sheet, const QRect& rect, bool isOpaque)
        : spriteId(id), spritesheetId(std::move(sheet)), cell(rect), size(rect.size()), opaque(isOpaque)
    {
    }

    /// The cell of a sprite in its spritesheet: sprites fill the 384 pixel wide
    /// spritesheet row by row in their layout's size.
    static QRect cellInSpritesheet(const TSpritesheetMetadata& metadata, uint32_t spriteId);
};

}
}
