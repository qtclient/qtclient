#pragma once

#include "shared/cip_typed.h"

#include "renderer/spritetexturecache.h"
#include "sprites/spritefromspritesheetinfo.h"

#include <QRect>

#include <QImage>

#include <array>
#include <cstdint>
#include <memory>

namespace tibia {
namespace appearances {

using qmlcomponents::SpriteKey;

/// CIP appearances::*SpriteInfo common surface: something the world can raster
/// into a QImage / SpriteKey. Concrete types match RTTI names.
class TAppearanceInstanceSpriteInfo : public ::tibia::CipTyped
{
public:
    virtual ~TAppearanceInstanceSpriteInfo() = default;
    virtual SpriteKey key() const = 0;
    virtual QImage raster() const = 0;
};

/// CIP interface name used by some call sites / units.
using IAppearanceInstanceSpriteInfo = TAppearanceInstanceSpriteInfo;

using TAppearanceInstanceSpriteInfoPtr = std::shared_ptr<TAppearanceInstanceSpriteInfo>;

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

    std::shared_ptr<TSpritesheetAppearanceInstanceSpriteInfo> clone() const;
    /// Draws the same sprite as `other`.
    bool hasSameSprite(const TSpritesheetAppearanceInstanceSpriteInfo& other) const;
    /// The sprite id (CIP asks it twice, v5 and v9).
    uint32_t spriteId() const { return m_sprite ? m_sprite->spriteId : 0; }
    QRect boundingBox() const { return m_boundingBox; }
    QSize spriteSize() const { return m_sprite ? m_sprite->size : QSize(); }
    const std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo>& sprite() const { return m_sprite; }

private:
    std::shared_ptr<const sprites::TSpriteFromSpritesheetInfo> m_sprite;
    QRect m_boundingBox;
};

/// CIP TMaskedAppearanceInstanceSpriteInfo — sprite + mask + outfit colours.
class TMaskedAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    TMaskedAppearanceInstanceSpriteInfo(uint32_t spriteId, uint32_t maskId, uint32_t colors);

    SpriteKey key() const override;
    QImage raster() const override;

private:
    uint32_t m_spriteId = 0;
    uint32_t m_maskId = 0;
    uint32_t m_colors = 0;
};

/// CIP TLayeredAppearanceInstanceSpriteInfo — colourized layered outfit draw.
/// Same key shape as masked for our atlas (sprite + mask + colours).
class TLayeredAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    TLayeredAppearanceInstanceSpriteInfo(uint32_t spriteId, uint32_t maskId, uint32_t colors);

    SpriteKey key() const override;
    QImage raster() const override;

private:
    uint32_t m_spriteId = 0;
    uint32_t m_maskId = 0;
    uint32_t m_colors = 0;
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
