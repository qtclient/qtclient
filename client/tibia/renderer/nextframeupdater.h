#pragma once

#include "shared/cip_typed.h"

#include <QtGlobal>

namespace tibia::chat {
class TTextStorage;
}
namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}
namespace tibia::worldmap {
class TNumericalEffectStorage;
}

namespace tibia::renderer {

/// Advances everything that moves to the time the next frame is shown (CIP:
/// renderer::TNextFrameUpdater::v2 at 0x100f77a00).
///
/// CIP constructs twelve shared_ptr members (FUN_100f778b0) and advances them
/// in fixed order. product collapses that inventory into:
///   1–N  creature / effect / missile / outfit time via gamecore::advanceTo
///        inside WalkCalculator::advance (motion);
///   next texts (TTextStorage) and numerical effects;
///   then TWorldMapRenderProvider::advance (sprite list + classic/raycast light);
///   finally motion().resume() returns the clock to real time.
/// Minimap / ambient-light timers stay event-driven (data change / 10 s), matching
/// CIP's non-hot-path refresh. See docs/reverse-engineering/tibia-world-rendering.md.
class TNextFrameUpdater : public ::tibia::CipTyped
{
public:
    TNextFrameUpdater(qmlcomponents::TWorldMapRenderProvider& provider, chat::TTextStorage& texts,
                      worldmap::TNumericalEffectStorage& numericalEffects);

    void advance(qint64 timestampMs);

private:
    qmlcomponents::TWorldMapRenderProvider& m_provider;
    chat::TTextStorage& m_texts;
    worldmap::TNumericalEffectStorage& m_numericalEffects;
};

}
