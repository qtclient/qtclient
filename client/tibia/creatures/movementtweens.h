#pragma once

#include <client/const.h>
#include <framework/util/point.h>

#include <cstdint>

namespace tibia {
namespace creatures {

/// Frame path: advance every live TCreatureMovementTween (CipSoft via TNextFrameUpdater).
void advanceAllMovementTweens(int64_t timestampMs);
void resumeMovementClock();

/// Walk offset for world draw / camera (storage row; 0 if unknown).
Point walkOffsetFor(uint32_t creatureId);

}
}
