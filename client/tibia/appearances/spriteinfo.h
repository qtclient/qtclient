#pragma once

#include "shared/cip_typed.h"

#include "renderer/spritetexturecache.h"

#include <QImage>

#include <array>
#include <cstdint>
#include <memory>

namespace tibia::appearances {

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

using TAppearanceInstanceSpriteInfoPtr = std::shared_ptr<TAppearanceInstanceSpriteInfo>;

/// CIP TCreatureMarkAppearanceInstanceSpriteInfo — field mark frames from colours.
class TCreatureMarkAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    explicit TCreatureMarkAppearanceInstanceSpriteInfo(std::array<uint32_t, 3> colors);

    SpriteKey key() const override;
    QImage raster() const override;
    const std::array<uint32_t, 3>& colors() const { return m_colors; }

    /// FUN_100ff6dc0 / spritetexturecache creatureMarkImage.
    static QImage imageForColors(const std::array<uint32_t, 3>& colors);

private:
    std::array<uint32_t, 3> m_colors{};
};

/// CIP TSpritesheetAppearanceInstanceSpriteInfo — one spritesheet entry.
class TSpritesheetAppearanceInstanceSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    explicit TSpritesheetAppearanceInstanceSpriteInfo(uint32_t spriteId);

    SpriteKey key() const override;
    QImage raster() const override;
    uint32_t spriteId() const { return m_spriteId; }

private:
    uint32_t m_spriteId = 0;
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
