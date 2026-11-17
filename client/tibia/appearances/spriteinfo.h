#pragma once

#include "shared/cip_typed.h"

#include "sprites/spritefromspritesheetinfo.h"

#include <QColor>
#include <QRect>

#include <QImage>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace tibia {
namespace appearances {

/// What an instance draws for one animation phase (CIP
/// appearances::IAppearanceInstanceSpriteInfo). Texture providers are asked
/// for the sprite an info describes and cache it by hash() and equals().
class TAppearanceInstanceSpriteInfo : public ::tibia::CipTyped
{
public:
    /// CIP v6: what kind of sprite info this is.
    enum class EKind
    {
        Spritesheet = 0,
        Masked = 1,
        Layered = 2,
        CreatureMark = 3,
        HighlightBaseplate = 5,
        Dummy = 7,
        Other = 8,
    };

    virtual ~TAppearanceInstanceSpriteInfo() = default;
    /// CIP v2: the sprite identifier kind of every sprite info (2; resource
    /// identifiers are 1, the others 3 to 5).
    int identifierKind() const { return 2; }
    /// CIP v3.
    virtual std::shared_ptr<TAppearanceInstanceSpriteInfo> clone() const { return nullptr; }
    /// CIP v4: callers compare only infos of the same dynamic type.
    virtual bool equals(const TAppearanceInstanceSpriteInfo& other) const { return this == &other; }
    /// CIP v5.
    virtual size_t hash() const { return 0; }
    /// CIP v6.
    virtual EKind kind() const { return EKind::Other; }
    /// CIP v7: the drawn part of the sprite.
    virtual QRect boundingBox() const { return QRect(0, 0, 32, 32); }
    /// CIP v8: the sprite's size in pixels.
    virtual QSize spriteSize() const { return QSize(32, 32); }
};

/// CIP interface name used by some call sites / units.
using IAppearanceInstanceSpriteInfo = TAppearanceInstanceSpriteInfo;

using TAppearanceInstanceSpriteInfoPtr = std::shared_ptr<TAppearanceInstanceSpriteInfo>;

/// Two sprite infos draw the same: both null, or the same type and equal.
bool sameSpriteInfo(const TAppearanceInstanceSpriteInfoPtr& a, const TAppearanceInstanceSpriteInfoPtr& b);

/// The four colours of an outfit (head, body, legs, feet) and their hash (CIP:
/// four QColors and a hash built from them).
struct TOutfitColors
{
    std::array<QColor, 4> colors;
    uint32_t hash = 0;

    TOutfitColors() = default;
    TOutfitColors(uint8_t head, uint8_t body, uint8_t legs, uint8_t feet);
    /// Host: from head | body << 8 | legs << 16 | feet << 24.
    static TOutfitColors fromPacked(uint32_t packed)
    {
        return TOutfitColors(static_cast<uint8_t>(packed), static_cast<uint8_t>(packed >> 8),
                             static_cast<uint8_t>(packed >> 16), static_cast<uint8_t>(packed >> 24));
    }
    bool operator==(const TOutfitColors& other) const;
};

/// CIP TCreatureMarkAppearanceInstanceSpriteInfo — field mark frames from colours.
class TCreatureMarkAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    /// The frames' colours, outer to inner.
    explicit TCreatureMarkAppearanceInstanceSpriteInfo(std::vector<QColor> colors);

    std::shared_ptr<TAppearanceInstanceSpriteInfo> clone() const override;
    bool equals(const TAppearanceInstanceSpriteInfo& other) const override;
    size_t hash() const override;
    EKind kind() const override { return EKind::CreatureMark; }
    QRect boundingBox() const override;
    QSize spriteSize() const override;
    /// CIP v9.
    std::vector<QColor> colors() const { return m_colors; }

    /// A field-sized image with one 2-pixel frame per colour.
    static QImage imageForColors(const std::vector<QColor>& colors);

private:
    std::vector<QColor> m_colors;
};

/// One sprite of a spritesheet drawn for an instance, with the bounding box of
/// the instance's direction (CIP appearances::TSpritesheetAppearanceInstanceSpriteInfo).
class TSpritesheetAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    TSpritesheetAppearanceInstanceSpriteInfo(std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> sprite,
                                             const QRect& boundingBox);
    /// Host: sprite `spriteId` of its spritesheet, the whole sprite as bounding box
    /// (things drawn from a type's pattern without an instance).
    explicit TSpritesheetAppearanceInstanceSpriteInfo(uint32_t spriteId);

    std::shared_ptr<TAppearanceInstanceSpriteInfo> clone() const override;
    bool equals(const TAppearanceInstanceSpriteInfo& other) const override;
    size_t hash() const override { return spriteId(); }
    EKind kind() const override { return EKind::Spritesheet; }
    QRect boundingBox() const override { return m_boundingBox; }
    QSize spriteSize() const override { return m_sprite ? m_sprite->size : QSize(); }

    /// Draws the same sprite as `other`.
    bool hasSameSprite(const TSpritesheetAppearanceInstanceSpriteInfo& other) const;
    /// The sprite id (CIP asks it twice, v5 and v9).
    uint32_t spriteId() const { return m_sprite ? m_sprite->spriteId : 0; }
    const std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>& sprite() const { return m_sprite; }

private:
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> m_sprite;
    QRect m_boundingBox;
};

/// A sprite coloured through its mask sprite with an outfit's colours
/// (CIP appearances::TMaskedAppearanceInstanceSpriteInfo).
class TMaskedAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    TMaskedAppearanceInstanceSpriteInfo(std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> sprite,
                                        std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> mask,
                                        const TOutfitColors& colors, const QRect& boundingBox);
    /// Host: sprite `spriteId` coloured through sprite `maskId`, the whole sprite as bounding box.
    TMaskedAppearanceInstanceSpriteInfo(uint32_t spriteId, uint32_t maskId, const TOutfitColors& colors);

    std::shared_ptr<TAppearanceInstanceSpriteInfo> clone() const override;
    bool equals(const TAppearanceInstanceSpriteInfo& other) const override;
    size_t hash() const override;
    EKind kind() const override { return EKind::Masked; }
    QRect boundingBox() const override { return m_boundingBox; }
    QSize spriteSize() const override { return m_sprite ? m_sprite->size : QSize(); }

    const std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>& sprite() const { return m_sprite; }
    const std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>& mask() const { return m_mask; }
    const TOutfitColors& colors() const { return m_colors; }

private:
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> m_sprite;
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> m_mask;
    TOutfitColors m_colors;
    QRect m_boundingBox;
};

/// Several sprite infos drawn over each other: an outfit and its addons
/// (CIP appearances::TLayeredAppearanceInstanceSpriteInfo).
class TLayeredAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    TLayeredAppearanceInstanceSpriteInfo(std::vector<TAppearanceInstanceSpriteInfoPtr> layers, const QRect& boundingBox);

    std::shared_ptr<TAppearanceInstanceSpriteInfo> clone() const override;
    bool equals(const TAppearanceInstanceSpriteInfo& other) const override;
    size_t hash() const override;
    EKind kind() const override { return EKind::Layered; }
    QRect boundingBox() const override { return m_boundingBox; }
    /// The bottom layer's sprite size.
    QSize spriteSize() const override;

    const std::vector<TAppearanceInstanceSpriteInfoPtr>& layers() const { return m_layers; }

private:
    std::vector<TAppearanceInstanceSpriteInfoPtr> m_layers;
    QRect m_boundingBox;
};

/// CIP TDummyAppearanceInstanceSpriteInfo — empty / missing sprite placeholder.
class TDummyAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    std::shared_ptr<TAppearanceInstanceSpriteInfo> clone() const override;
    bool equals(const TAppearanceInstanceSpriteInfo&) const override { return true; }
    size_t hash() const override { return 0; }
    EKind kind() const override { return EKind::Dummy; }
    QRect boundingBox() const override { return QRect(); }
    QSize spriteSize() const override { return QSize(); }
};

/// CIP's sprite sizes by index (32x32, 32x64, 64x32, 64x64); empty past the end.
QSize spriteSizeOfIndex(unsigned index);

}
}
