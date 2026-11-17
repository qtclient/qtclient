#include "sprites/spriteinspritesheet.h"

namespace tibia::sprites {

TSpriteInSpritesheet::TSpriteInSpritesheet(const double scale, TSpritesheetPointer spritesheet, const QRect& cell, const QRect& area,
                                           const bool opaque)
    : TSprite(area.size(), scale)
    , m_spritesheet(std::move(spritesheet))
    , m_cell(cell)
    , m_area(area)
    , m_opaque(opaque)
{
}

QImage TSpriteInSpritesheet::getImage() const
{
    QImage image = m_spritesheet->copy(QRect(m_cell.topLeft() + m_area.topLeft(), m_area.size()));
    if (m_opaque)
        image = image.convertToFormat(QImage::Format_RGB32);
    return image;
}

}
