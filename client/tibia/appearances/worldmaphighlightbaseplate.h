#pragma once

#include "appearances/spriteinfo.h"

#include <QtGlobal>

#include <cstdint>

namespace tibia::appearances {

/// CIP TWorldmapHighlightBaseplateSpriteInfo — field-sized animated square with
/// corner marks and a center crosshair under the mouse (Highlight Mouse Target).
/// Art: `:/cursors/highlight-baseplate.png` (256×32 strip, eight 32×32 frames).
class TWorldmapHighlightBaseplateSpriteInfo final : public TAppearanceInstanceSpriteInfo
{
public:
    static constexpr int FrameCount = 8;
    static constexpr int FrameSize = 32;

    explicit TWorldmapHighlightBaseplateSpriteInfo(uint8_t frame = 0)
        : m_frame(static_cast<uint8_t>(frame % FrameCount))
    {
    }

    uint8_t frame() const { return m_frame; }

    SpriteKey key() const override;
    QImage raster() const override;

    /// CIP FUN_100951690 / v21: frame from wall-clock ms (`(t * k) & 7`).
    static uint8_t frameAtMillis(qint64 millis);

    /// Crops one frame from the RCC cursor strip. Null if the resource is missing.
    static QImage imageForFrame(uint8_t frame);

private:
    uint8_t m_frame = 0;
};

}
