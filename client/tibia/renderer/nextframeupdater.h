#pragma once

#include "shared/cip_typed.h"

#include <QtGlobal>

namespace tibia {
namespace chat {
class TTextStorage;
}
}
namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}
namespace tibia {
namespace worldmap {
class TNumericalEffectStorage;
}
}

namespace tibia {
namespace renderer {

/// Advances everything that moves to the time the next frame is shown (CipSoft
/// stamped `v2` @).
///
/// CipSoft constructs twelve shared_ptr members and advances them
/// in fixed order. product collapses that inventory into:
/// 1–N creature / effect / missile / outfit time via gamecore::advanceTo
/// inside creatures::advanceAllMovementTweens (TCreatureMovementTween);
/// next texts (TTextStorage) and numerical effects;
/// then TWorldMapRenderProvider::advance (sprite list + classic/raycast light);
/// finally resumeMovementClock returns the clock to real time.
/// Minimap / ambient-light timers stay event-driven (data change / 10 s), matching
/// CipSoft's non-hot-path refresh. See docs/reverse-engineering/tibia-world-rendering.md.
class TNextFrameUpdater : public ::tibia::CipTyped
{
public:
    TNextFrameUpdater(qmlcomponents::TWorldMapRenderProvider& provider, chat::TTextStorage& texts,
                      worldmap::TNumericalEffectStorage& numericalEffects);

    void v2(qint64 timestampMs);

private:
    qmlcomponents::TWorldMapRenderProvider& m_provider;
    chat::TTextStorage& m_texts;
    worldmap::TNumericalEffectStorage& m_numericalEffects;
};

}
}
