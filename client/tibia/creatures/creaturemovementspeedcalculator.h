#pragma once

#include <client/const.h>
#include <client/position.h>

#include <cstdint>

namespace tibia {
namespace creatures {

class TCreature;

/// CipSoft `creatures::TCreatureMovementSpeedCalculator` (stamped `v9` @).
/// Computes ground / diagonal step duration for one live movement tween.
class TCreatureMovementSpeedCalculator
{
public:
    /// CipSoft always divides by a positive speed; transient 0 must not freeze.
    /// Diagonal dirs (bitmask 0xaa) multiply by 3 before beat rounding.
    uint16_t v9(TCreature& creature, bool ignoreDiagonal = false);
    void invalidateCache() { m_cache.speed = 0; }

private:
    struct CachedStep
    {
        uint16_t speed{ 0 };
        uint16_t groundSpeed{ 0 };
        uint16_t duration{ 0 };
        uint16_t walkDuration{ 0 };
        uint16_t diagonalDuration{ 0 };

        uint16_t getDuration(const tibia::client::Direction dir) const
        {
            return Position::isDiagonal(dir) ? diagonalDuration : duration;
        }
    };

    CachedStep m_cache;
};

}
}
