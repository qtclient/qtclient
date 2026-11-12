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

/// Advances everything that moves to the time the next frame is shown
/// (CIP `renderer::TNextFrameUpdater::v2`).
///
/// Pin unit (`tmp/re/builds/f7a12491b5d5/units/renderer/TNextFrameUpdater.c`):
/// CIP holds twelve `shared_ptr` slots at +0x08..+0xb8 (stride 0x10) and
/// advances them in order via vtable offsets 0x120, 0xd8, 0x60, 0x90, 0x60,
/// 0x78, 0x78, 0x80, 0x78, 0x78, 0x78, 0x60. Product collapses that inventory
/// into: creature/effect/missile/outfit time via `gamecore::advanceTo` inside
/// `creatures::advanceAllMovementTweens`; texts (`TTextStorage`) and numerical
/// effects; `TWorldMapRenderProvider::advance` (sprite list + light); then
/// `resumeMovementClock`. Minimap / ambient-light stay event-driven.
class TNextFrameUpdater : public ::tibia::CipTyped
{
public:
    TNextFrameUpdater(qmlcomponents::TWorldMapRenderProvider& provider, chat::TTextStorage& texts,
                      worldmap::TNumericalEffectStorage& numericalEffects);

    /// CIP v2 — advance all frame-dependent systems to `timestampMs`.
    void v2(qint64 timestampMs);

private:
    qmlcomponents::TWorldMapRenderProvider& m_provider;
    chat::TTextStorage& m_texts;
    worldmap::TNumericalEffectStorage& m_numericalEffects;
};

}
}
