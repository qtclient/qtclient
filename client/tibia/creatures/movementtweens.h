#pragma once

#include <client/const.h>
#include <framework/util/point.h>

#include <cstdint>

namespace tibia {
namespace creatures {

/// Frame path: advance every live TCreatureMovementTween (CipSoft via TNextFrameUpdater).
void advanceAllMovementTweens(int64_t timestampMs);
void resumeMovementClock();

/// Step gate for keyboard / GoPath prediction (storage-authoritative).
bool canBeginStep(bool ignoreLock = false);
bool blocksStep(tibia::client::Direction direction);

/// Walk offset for world draw / camera (storage row; 0 if unknown).
Point walkOffsetFor(uint32_t creatureId);

}
}
