#include "appearances/spriteinfo.h"
#include "colorhelper.h"
#include "sprites/spritesystem.h"

#include <client/outfit.h>

#include <QPainter>

#include <typeinfo>

namespace tibia::appearances {

namespace {

using sprites::hashCombine;

uint32_t spriteIdOf(const std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>& sprite)
{
    return sprite ? sprite->spriteId : 0;
}

} // namespace

QSize spriteSizeOfIndex(const unsigned index)
{
    static constexpr std::array<QSize, 4> Sizes{ QSize(32, 32), QSize(32, 64), QSize(64, 32), QSize(64, 64) };
    return index < Sizes.size() ? Sizes[index] : QSize(0, 0);
}

TCreatureMarkAppearanceInstanceSpriteInfo::TCreatureMarkAppearanceInstanceSpriteInfo(std::vector<QColor> colors)
    : m_colors(std::move(colors))
{
}

std::shared_ptr<TAppearanceInstanceSpriteInfo> TCreatureMarkAppearanceInstanceSpriteInfo::clone() const
{
    return std::make_shared<TCreatureMarkAppearanceInstanceSpriteInfo>(m_colors);
}

bool TCreatureMarkAppearanceInstanceSpriteInfo::equals(const sprites::ISpriteIdentifier& other) const
{
    return m_colors == static_cast<const TCreatureMarkAppearanceInstanceSpriteInfo&>(other).colors();
}

size_t TCreatureMarkAppearanceInstanceSpriteInfo::hash() const
{
    size_t seed = 0;
    for (const QColor& color : m_colors)
        seed = hashCombine(seed, color.rgb());
    return seed;
}

QRect TCreatureMarkAppearanceInstanceSpriteInfo::boundingBox() const
{
    return QRect(QPoint(0, 0), spriteSize());
}

QSize TCreatureMarkAppearanceInstanceSpriteInfo::spriteSize() const
{
    return spriteSizeOfIndex(0);
}

QImage TCreatureMarkAppearanceInstanceSpriteInfo::imageForColors(const std::vector<QColor>& colors)
{
    // CIP : a field-sized image with one 2-pixel frame per colour.
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
    for (const QColor& color : colors) {
        extent -= 2;
        pen.setColor(color);
        painter.setPen(pen);
        painter.drawRect(QRect(QPoint(inset, inset), QPoint(extent, extent)));
        inset += 2;
    }
    return image;
}

TSpritesheetAppearanceInstanceSpriteInfo::TSpritesheetAppearanceInstanceSpriteInfo(
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> sprite, const QRect& boundingBox)
    : m_sprite(std::move(sprite))
    , m_boundingBox(boundingBox)
{
}

namespace {

std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> spritesheetSprite(const uint32_t spriteId)
{
    return std::make_shared<const sprites::TSpriteFromSpritesheetInfo>(sprites::TSpriteSystem::instance().spriteInfo(spriteId));
}

} // namespace

TSpritesheetAppearanceInstanceSpriteInfo::TSpritesheetAppearanceInstanceSpriteInfo(const uint32_t spriteId)
    : m_sprite(spritesheetSprite(spriteId))
    , m_boundingBox(QPoint(0, 0), m_sprite->size)
{
}

std::shared_ptr<TAppearanceInstanceSpriteInfo> TSpritesheetAppearanceInstanceSpriteInfo::clone() const
{
    return std::make_shared<TSpritesheetAppearanceInstanceSpriteInfo>(m_sprite, m_boundingBox);
}

bool TSpritesheetAppearanceInstanceSpriteInfo::equals(const sprites::ISpriteIdentifier& other) const
{
    return hasSameSprite(static_cast<const TSpritesheetAppearanceInstanceSpriteInfo&>(other));
}

bool TSpritesheetAppearanceInstanceSpriteInfo::hasSameSprite(const TSpritesheetAppearanceInstanceSpriteInfo& other) const
{
    return spriteId() == other.spriteId();
}

bool sameSpriteInfo(const TAppearanceInstanceSpriteInfoPtr& a, const TAppearanceInstanceSpriteInfoPtr& b)
{
    if (!a || !b)
        return !a && !b;
    return sprites::sameSpriteIdentifier(*a, *b);
}

TOutfitColors::TOutfitColors(const uint8_t head, const uint8_t body, const uint8_t legs, const uint8_t feet)
    : colors{ tibia::shared::outfitColor(head), tibia::shared::outfitColor(body), tibia::shared::outfitColor(legs), tibia::shared::outfitColor(feet) }
{
    hash = colors[3].rgba() * 13 ^ colors[2].rgba() * 11 ^ colors[1].rgba() * 7 ^ colors[0].rgba();
}

bool TOutfitColors::operator==(const TOutfitColors& other) const
{
    return colors == other.colors;
}

TMaskedAppearanceInstanceSpriteInfo::TMaskedAppearanceInstanceSpriteInfo(
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> sprite,
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> mask, const TOutfitColors& colors, const QRect& boundingBox)
    : m_sprite(std::move(sprite))
    , m_mask(std::move(mask))
    , m_colors(colors)
    , m_boundingBox(boundingBox)
{
}

TMaskedAppearanceInstanceSpriteInfo::TMaskedAppearanceInstanceSpriteInfo(const uint32_t spriteId, const uint32_t maskId,
                                                                         const TOutfitColors& colors)
    : m_sprite(spritesheetSprite(spriteId))
    , m_mask(spritesheetSprite(maskId))
    , m_colors(colors)
    , m_boundingBox(QPoint(0, 0), m_sprite->size)
{
}

std::shared_ptr<TAppearanceInstanceSpriteInfo> TMaskedAppearanceInstanceSpriteInfo::clone() const
{
    return std::make_shared<TMaskedAppearanceInstanceSpriteInfo>(m_sprite, m_mask, m_colors, m_boundingBox);
}

bool TMaskedAppearanceInstanceSpriteInfo::equals(const sprites::ISpriteIdentifier& other) const
{
    const auto& masked = static_cast<const TMaskedAppearanceInstanceSpriteInfo&>(other);
    return m_colors == masked.m_colors && spriteIdOf(m_sprite) == spriteIdOf(masked.m_sprite)
        && spriteIdOf(m_mask) == spriteIdOf(masked.m_mask);
}

size_t TMaskedAppearanceInstanceSpriteInfo::hash() const
{
    size_t seed = hashCombine(0, spriteIdOf(m_sprite));
    seed = hashCombine(seed, spriteIdOf(m_mask));
    return hashCombine(seed, m_colors.hash);
}

TLayeredAppearanceInstanceSpriteInfo::TLayeredAppearanceInstanceSpriteInfo(std::vector<TAppearanceInstanceSpriteInfoPtr> layers,
                                                                           const QRect& boundingBox)
    : m_layers(std::move(layers))
    , m_boundingBox(boundingBox)
{
}

std::shared_ptr<TAppearanceInstanceSpriteInfo> TLayeredAppearanceInstanceSpriteInfo::clone() const
{
    return std::make_shared<TLayeredAppearanceInstanceSpriteInfo>(m_layers, m_boundingBox);
}

bool TLayeredAppearanceInstanceSpriteInfo::equals(const sprites::ISpriteIdentifier& other) const
{
    const auto& layered = static_cast<const TLayeredAppearanceInstanceSpriteInfo&>(other);
    if (layered.m_layers.size() != m_layers.size())
        return false;
    for (size_t i = 0; i < m_layers.size(); ++i) {
        if (!sameSpriteInfo(m_layers[i], layered.m_layers[i]))
            return false;
    }
    return true;
}

size_t TLayeredAppearanceInstanceSpriteInfo::hash() const
{
    size_t seed = 0;
    for (const TAppearanceInstanceSpriteInfoPtr& layer : m_layers)
        seed = hashCombine(seed, layer ? layer->hash() : 0);
    return seed;
}

QSize TLayeredAppearanceInstanceSpriteInfo::spriteSize() const
{
    return m_layers.front()->spriteSize();
}

std::shared_ptr<TAppearanceInstanceSpriteInfo> TDummyAppearanceInstanceSpriteInfo::clone() const
{
    return std::make_shared<TDummyAppearanceInstanceSpriteInfo>();
}

}
