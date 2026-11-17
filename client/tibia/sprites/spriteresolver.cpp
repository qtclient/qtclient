#include "sprites/spriteresolver.h"

#include "appearances/spriteinfo.h"
#include "appearances/worldmaphighlightbaseplate.h"

#include "sprites/singlesprite.h"
#include "sprites/spriteinspritesheet.h"
#include "sprites/spritesheetmanager.h"

#include <QPainter>

namespace tibia::sprites {

TSpriteResolver::TSpriteResolver(std::shared_ptr<TSpritesheetManager> spritesheets, QObject* parent)
    : QObject(parent)
    , m_spritesheets(std::move(spritesheets))
{
}

std::shared_ptr<TSprite> TSpriteResolver::resolveSpritesheetSprite(const TSpriteFromSpritesheetInfo& info) const
{
    const TSpritesheetPointer spritesheet = m_spritesheets->getSpritesheet(info.spritesheetId);
    if (!spritesheet)
        return nullptr;
    const QRect area(QPoint(0, 0), QPoint(info.cell.right() - info.cell.left(), info.cell.bottom() - info.cell.top()));
    return std::make_shared<TSpriteInSpritesheet>(0.0, spritesheet, info.cell, area, info.opaque);
}

std::shared_ptr<TSprite> TSpriteResolver::resolveAssetSpritesheet(const TSpritesheetID& spritesheetId,
                                                                   const float scale) const
{
    const TSpritesheetPointer spritesheet = m_spritesheets->getSpritesheet(spritesheetId);
    if (!spritesheet)
        return nullptr;
    auto sprite = std::make_shared<TSingleSprite>(scale, *spritesheet);
    sprite->setCacheable(false);
    return sprite;
}

namespace {

/// The colour each pixel of an outfit mask picks: head for yellow, body for
/// red, legs for green, feet for blue, white elsewhere.
QImage outfitColorMap(const QImage& mask, const appearances::TOutfitColors& colors)
{
    QImage map(mask.size(), QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            QRgb color = 0xffffffff;
            switch (mask.pixel(x, y)) {
            case 0xffffff00:
                color = colors.colors[0].rgb();
                break;
            case 0xffff0000:
                color = colors.colors[1].rgb();
                break;
            case 0xff00ff00:
                color = colors.colors[2].rgb();
                break;
            case 0xff0000ff:
                color = colors.colors[3].rgb();
                break;
            default:
                break;
            }
            map.setPixel(x, y, color);
        }
    }
    return map;
}

/// Each channel of `image` times the one of `colors` (out of 255), the alpha of `image`.
QImage multiplied(const QImage& image, const QImage& colors)
{
    QImage result(image.size(), QImage::Format_ARGB32_Premultiplied);
    const auto channel = [](const uint32_t a, const uint32_t b) { return (a * b * 0x8081u) >> 23; };
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            const QRgb s = image.pixel(x, y);
            const QRgb c = colors.pixel(x, y);
            const uint32_t red = channel(qRed(s), qRed(c));
            const uint32_t green = channel(qGreen(s), qGreen(c));
            const uint32_t blue = channel(qBlue(s), qBlue(c));
            result.setPixel(x, y, (s & 0xff000000u) | red << 16 | green << 8 | blue);
        }
    }
    return result;
}

} // namespace

QImage TSpriteResolver::colorizedOutfit(const QImage& sprite, const QImage& mask, const appearances::TOutfitColors& colors)
{
    return multiplied(sprite, outfitColorMap(mask, colors));
}

QImage TSpriteResolver::composed(const QImage& bottom, const QImage& top)
{
    QImage result = bottom.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage over = top.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter;
    painter.begin(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(QRectF(result.rect()), over, QRectF(0, 0, over.width(), over.height()));
    painter.end();
    return result;
}

std::shared_ptr<TSprite> TSpriteResolver::resolveSpriteInfo(const appearances::TAppearanceInstanceSpriteInfo& info) const
{
    using EKind = appearances::TAppearanceInstanceSpriteInfo::EKind;
    switch (info.kind()) {
    case EKind::Spritesheet: {
        const auto& sprite = static_cast<const appearances::TSpritesheetAppearanceInstanceSpriteInfo&>(info).sprite();
        return sprite ? resolveSpritesheetSprite(*sprite) : nullptr;
    }
    case EKind::Masked:
        return resolveMaskedSprite(static_cast<const appearances::TMaskedAppearanceInstanceSpriteInfo&>(info));
    case EKind::Layered:
        return resolveLayeredSprite(static_cast<const appearances::TLayeredAppearanceInstanceSpriteInfo&>(info));
    case EKind::CreatureMark:
        return resolveCreatureMark(static_cast<const appearances::TCreatureMarkAppearanceInstanceSpriteInfo&>(info));
    case EKind::HighlightBaseplate:
        return resolveHighlightBaseplate(static_cast<const appearances::TWorldmapHighlightBaseplateSpriteInfo&>(info));
    default:
        return nullptr;
    }
}

std::shared_ptr<TSprite> TSpriteResolver::resolveCreatureMark(const appearances::TCreatureMarkAppearanceInstanceSpriteInfo& info) const
{
    return std::make_shared<TSingleSprite>(0.0, appearances::TCreatureMarkAppearanceInstanceSpriteInfo::imageForColors(info.colors()));
}

std::shared_ptr<TSprite>
TSpriteResolver::resolveHighlightBaseplate(const appearances::TWorldmapHighlightBaseplateSpriteInfo& info) const
{
    return std::make_shared<TSingleSprite>(0.0, appearances::TWorldmapHighlightBaseplateSpriteInfo::imageForFrame(info.frame()));
}

std::shared_ptr<TSprite> TSpriteResolver::resolveMaskedSprite(const appearances::TMaskedAppearanceInstanceSpriteInfo& info) const
{
    if (!info.sprite() || !info.mask())
        return nullptr;
    const TSpritesheetPointer spritesheet = m_spritesheets->getSpritesheet(info.sprite()->spritesheetId);
    const TSpritesheetPointer maskSpritesheet = m_spritesheets->getSpritesheet(info.mask()->spritesheetId);
    if (!spritesheet || !maskSpritesheet)
        return nullptr;
    const auto areaOf = [](const QRect& cell) {
        return QRect(QPoint(0, 0), QPoint(cell.right() - cell.left(), cell.bottom() - cell.top()));
    };
    const TSpriteInSpritesheet sprite(0.0, spritesheet, info.sprite()->cell, areaOf(info.sprite()->cell), false);
    const TSpriteInSpritesheet mask(0.0, maskSpritesheet, info.mask()->cell, areaOf(info.mask()->cell), false);
    return std::make_shared<TSingleSprite>(0.0, colorizedOutfit(sprite.getImage(), mask.getImage(), info.colors()));
}

std::shared_ptr<TSprite> TSpriteResolver::resolveLayeredSprite(const appearances::TLayeredAppearanceInstanceSpriteInfo& info) const
{
    std::shared_ptr<TSprite> result;
    for (const auto& layer : info.layers()) {
        std::shared_ptr<TSprite> sprite = layer ? resolveSpriteInfo(*layer) : nullptr;
        if (!sprite)
            return nullptr;
        result = result ? std::make_shared<TSingleSprite>(0.0, composed(result->getImage(), sprite->getImage())) : sprite;
    }
    return result;
}

}
