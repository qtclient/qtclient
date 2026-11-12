#pragma once

#include <client/position.h>

#include <cstdint>

class ThingType;

namespace tibia {
namespace creatures {

class TCreature;
class TCreatureMovementSpeedCalculator;

/// CipSoft `creatures::TCreatureMovementTween` (names.txt `v9` @ ``).
/// One live duration tween per creature — offset, walking tile, foot phase.
class TCreatureMovementTween
{
public:
    explicit TCreatureMovementTween(TCreature& owner);

    /// Returns false when the call is a no-op (same in-flight from→to).
    bool v9(const Position& from, const Position& to);
    void advance();
    void terminate();
    void stop();
    /// CipSoft floor change / map teleport: drop tween and idle feet immediately.
    void abortForTeleport();

    void refreshAnimationPhases();

private:
    void updateWalkOffset(double progress);
    void updateWalkingTile();
    void updateWalkAnimation();
    void clearWalkingTile();
    uint16_t computeAnimationPhase(bool mount) const;
    ThingType* outfitThingType() const;
    ThingType* mountThingType() const;
    int displacementXForWalk() const;
    int displacementYForWalk() const;
    static void endDeltaForDirection(uint8_t direction, int& dx, int& dy);

    TCreature& m_owner;
};

}
}
