#include "appearances/spriteinfo.h"
#include "sprites/spritesystem.h"

#include <client/outfit.h>

#include <QPainter>

#include <typeinfo>

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

TSpritesheetAppearanceInstanceSpriteInfo::TSpritesheetAppearanceInstanceSpriteInfo(
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> sprite, const QRect& boundingBox)
    : m_sprite(std::move(sprite))
    , m_boundingBox(boundingBox)
{
}

TSpritesheetAppearanceInstanceSpriteInfo::TSpritesheetAppearanceInstanceSpriteInfo(const uint32_t spriteId)
    : m_sprite(std::make_shared<const sprites::TSpriteFromSpritesheetInfo>(sprites::TSpriteSystem::instance().spriteInfo(spriteId)))
    , m_boundingBox(0, 0, 32, 32)
{
}

std::shared_ptr<TAppearanceInstanceSpriteInfo> TSpritesheetAppearanceInstanceSpriteInfo::clone() const
{
    return std::make_shared<TSpritesheetAppearanceInstanceSpriteInfo>(m_sprite, m_boundingBox);
}

bool TSpritesheetAppearanceInstanceSpriteInfo::equals(const TAppearanceInstanceSpriteInfo& other) const
{
    return hasSameSprite(static_cast<const TSpritesheetAppearanceInstanceSpriteInfo&>(other));
}

bool TSpritesheetAppearanceInstanceSpriteInfo::hasSameSprite(const TSpritesheetAppearanceInstanceSpriteInfo& other) const
{
    return spriteId() == other.spriteId();
}

SpriteKey TSpritesheetAppearanceInstanceSpriteInfo::key() const
{
    return SpriteKey{ .spriteId = spriteId() };
}

QImage TSpritesheetAppearanceInstanceSpriteInfo::raster() const
{
    // Drawn via TSpriteTextureCache sheet decompress using key; no eager raster.
    return {};
}

namespace {

/// boost::hash_combine (64 bit): mixes `value` into `seed`.
size_t hashCombine(const size_t seed, const size_t value)
{
    uint64_t x = seed + 0x9e3779b9 + value;
    x = (x >> 32 ^ x) * 0xe9846af9b1a615dULL;
    x = (x >> 32 ^ x) * 0xe9846af9b1a615dULL;
    return x >> 28 ^ x;
}

uint32_t spriteIdOf(const std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>& sprite)
{
    return sprite ? sprite->spriteId : 0;
}

} // namespace

bool sameSpriteInfo(const TAppearanceInstanceSpriteInfoPtr& a, const TAppearanceInstanceSpriteInfoPtr& b)
{
    if (!a || !b)
        return !a && !b;
    return typeid(*a) == typeid(*b) && a->equals(*b);
}

TOutfitColors::TOutfitColors(const uint8_t head, const uint8_t body, const uint8_t legs, const uint8_t feet)
    : colors{ Outfit::getColor(head), Outfit::getColor(body), Outfit::getColor(legs), Outfit::getColor(feet) }
    , indices(head | body << 8 | legs << 16 | static_cast<uint32_t>(feet) << 24)
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

std::shared_ptr<TAppearanceInstanceSpriteInfo> TMaskedAppearanceInstanceSpriteInfo::clone() const
{
    return std::make_shared<TMaskedAppearanceInstanceSpriteInfo>(m_sprite, m_mask, m_colors, m_boundingBox);
}

bool TMaskedAppearanceInstanceSpriteInfo::equals(const TAppearanceInstanceSpriteInfo& other) const
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

SpriteKey TMaskedAppearanceInstanceSpriteInfo::key() const
{
    return SpriteKey{ .spriteId = spriteIdOf(m_sprite), .maskId = spriteIdOf(m_mask), .colors = m_colors.indices };
}

QImage TMaskedAppearanceInstanceSpriteInfo::raster() const
{
    return {};
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

bool TLayeredAppearanceInstanceSpriteInfo::equals(const TAppearanceInstanceSpriteInfo& other) const
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

SpriteKey TLayeredAppearanceInstanceSpriteInfo::key() const
{
    return m_layers.empty() || !m_layers.front() ? SpriteKey{} : m_layers.front()->key();
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
