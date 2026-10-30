#include "appearances/spriteinfo.h"

#include <QPainter>

namespace tibia::appearances {

TCreatureMarkAppearanceInstanceSpriteInfo::TCreatureMarkAppearanceInstanceSpriteInfo(
    const std::array<uint32_t, 3> colors)
    : m_colors(colors)
{
}

SpriteKey TCreatureMarkAppearanceInstanceSpriteInfo::key() const
{
    return SpriteKey{ .markColors = m_colors };
}

QImage TCreatureMarkAppearanceInstanceSpriteInfo::raster() const
{
    return imageForColors(m_colors);
}

QImage TCreatureMarkAppearanceInstanceSpriteInfo::imageForColors(const std::array<uint32_t, 3>& colors)
{
    // CIP FUN_100ff6dc0: a field-sized image with one 2-pixel frame per colour.
    constexpr int Size = 32;
    QImage image(Size, Size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QPen pen;
    pen.setWidth(2);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setBrush(Qt::NoBrush);
    int inset = 1;
    int extent = Size;
    for (const uint32_t color : colors) {
        if (!color)
            break;
        extent -= 2;
        pen.setColor(QColor::fromRgba(color));
        painter.setPen(pen);
        painter.drawRect(QRect(QPoint(inset, inset), QPoint(extent, extent)));
        inset += 2;
    }
    return image;
}

TSpritesheetAppearanceInstanceSpriteInfo::TSpritesheetAppearanceInstanceSpriteInfo(const uint32_t spriteId)
    : m_spriteId(spriteId)
{
}

SpriteKey TSpritesheetAppearanceInstanceSpriteInfo::key() const
{
    return SpriteKey{ .spriteId = m_spriteId };
}

QImage TSpritesheetAppearanceInstanceSpriteInfo::raster() const
{
    // Drawn via TSpriteTextureCache sheet decompress using key(); no eager raster.
    return {};
}

TMaskedAppearanceInstanceSpriteInfo::TMaskedAppearanceInstanceSpriteInfo(const uint32_t spriteId,
                                                                           const uint32_t maskId,
                                                                           const uint32_t colors)
    : m_spriteId(spriteId)
    , m_maskId(maskId)
    , m_colors(colors)
{
}

SpriteKey TMaskedAppearanceInstanceSpriteInfo::key() const
{
    return SpriteKey{ .spriteId = m_spriteId, .maskId = m_maskId, .colors = m_colors };
}

QImage TMaskedAppearanceInstanceSpriteInfo::raster() const
{
    return {};
}

TLayeredAppearanceInstanceSpriteInfo::TLayeredAppearanceInstanceSpriteInfo(const uint32_t spriteId,
                                                                             const uint32_t maskId,
                                                                             const uint32_t colors)
    : m_spriteId(spriteId)
    , m_maskId(maskId)
    , m_colors(colors)
{
}

SpriteKey TLayeredAppearanceInstanceSpriteInfo::key() const
{
    return SpriteKey{ .spriteId = m_spriteId, .maskId = m_maskId, .colors = m_colors };
}

QImage TLayeredAppearanceInstanceSpriteInfo::raster() const
{
    return {};
}

SpriteKey TDummyAppearanceInstanceSpriteInfo::key() const
{
    return {};
}

QImage TDummyAppearanceInstanceSpriteInfo::raster() const
{
    return {};
}

}
