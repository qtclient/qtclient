#pragma once

#include "shared/cip_typed.h"

#include "renderer/spritetexturecache.h"
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

using qmlcomponents::SpriteKey;

/// What an instance draws for one animation phase (CIP
/// appearances::IAppearanceInstanceSpriteInfo). The CIP part is the vtable from
/// v3 on; key() and raster() are the host's texture cache keys.
class TAppearanceInstanceSpriteInfo : public ::tibia::CipTyped
{
public:
    /// CIP v6: what kind of sprite info this is.
    enum class EKind
    {
        Spritesheet = 0,
        Masked = 1,
        Layered = 2,
        Other = 3,
    };

    virtual ~TAppearanceInstanceSpriteInfo() = default;
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

    virtual SpriteKey key() const = 0;
    virtual QImage raster() const = 0;
};

/// CIP interface name used by some call sites / units.
using IAppearanceInstanceSpriteInfo = TAppearanceInstanceSpriteInfo;

using TAppearanceInstanceSpriteInfoPtr = std::shared_ptr<TAppearanceInstanceSpriteInfo>;

/// Two sprite infos draw the same: both null, or the same type and equal.
bool sameSpriteInfo(const TAppearanceInstanceSpriteInfoPtr& a, const TAppearanceInstanceSpriteInfoPtr& b);

/// The four colours of an outfit (head, body, legs, feet) and their hash (CIP:
/// four QColors and a hash built from the outfit's colour indices). The host
/// keeps the indices for its texture cache key.
struct TOutfitColors
{
    std::array<QColor, 4> colors;
    uint32_t hash = 0;
    /// head | body << 8 | legs << 16 | feet << 24 (host).
    uint32_t indices = 0;

    TOutfitColors() = default;
    TOutfitColors(uint8_t head, uint8_t body, uint8_t legs, uint8_t feet);
    bool operator==(const TOutfitColors& other) const;
};

/// CIP TCreatureMarkAppearanceInstanceSpriteInfo — field mark frames from colours.
class TCreatureMarkAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    explicit TCreatureMarkAppearanceInstanceSpriteInfo(std::array<uint32_t, 3> colors);

    SpriteKey key() const override;
    QImage raster() const override;
    const std::array<uint32_t, 3>& colors() const { return m_colors; }

    /// / spritetexturecache creatureMarkImage.
    static QImage imageForColors(const std::array<uint32_t, 3>& colors);

private:
    std::array<uint32_t, 3> m_colors{};
};

/// One sprite of a spritesheet drawn for an instance, with the bounding box of
/// the instance's direction (CIP appearances::TSpritesheetAppearanceInstanceSpriteInfo).
class TSpritesheetAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    TSpritesheetAppearanceInstanceSpriteInfo(std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> sprite,
                                             const QRect& boundingBox);
    explicit TSpritesheetAppearanceInstanceSpriteInfo(uint32_t spriteId);
    SpriteKey key() const override;
    QImage raster() const override;

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

    SpriteKey key() const override;
    QImage raster() const override;

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

    /// The bottom layer's key (draw layers() for all of them).
    SpriteKey key() const override;
    QImage raster() const override;

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
    SpriteKey key() const override;
    QImage raster() const override;
};

}
}
