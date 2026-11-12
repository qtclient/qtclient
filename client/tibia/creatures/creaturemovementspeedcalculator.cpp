#include "creatures/creaturemovementspeedcalculator.h"

#include "creatures/creaturespeedformula.h"
#include "creatures/creaturestorage.h"
#include <tibia/config/runtimegameconfig.h>
#include <tibia/game/gamesession.h>
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapfield.h"

#include <algorithm>

namespace tibia::creatures {

// b1/732ebf229d0725dc
uint16_t TCreatureMovementSpeedCalculator::v9(TCreature& creature,
                                              const bool ignoreDiagonal)
{
    // CipSoft always divides by a positive speed.
    // A transient 0 (parse gap / paralyze edge) must not freeze the tween.
    const uint16_t speed = std::max<uint16_t>(1, creature.m_speed);

    const Position& tilePos = creature.m_lastStepTo.isValid() ? creature.m_lastStepTo
                                                             : creature.m_position;
    tibia::worldmap::TWorldMapField* tile =
        tibia::worldmap::fieldAt(tilePos.isValid() ? tilePos : creature.m_position);

    const int serverBeat = tibia::game::getServerBeat();

    int groundSpeed = 0;
    if (tile)
        groundSpeed = tile->getGroundSpeed();
    if (groundSpeed == 0)
        groundSpeed = 150;

    if (groundSpeed != m_cache.groundSpeed || speed != m_cache.speed) {
        m_cache.speed = speed;
        m_cache.groundSpeed = static_cast<uint16_t>(groundSpeed);

        uint32_t stepDuration = 1000u * static_cast<uint32_t>(groundSpeed);
        if (hasWireSpeedFormula()) {
            stepDuration /= std::max<uint16_t>(1, creature.m_calculatedStepSpeed);
        } else {
            stepDuration /= speed;
        }

        if (tibia::config::forcingNewWalkingFormula() || tibia::game::clientRevision() >= 860) {
            stepDuration = ((stepDuration + static_cast<uint32_t>(serverBeat) - 1u)
                            / static_cast<uint32_t>(serverBeat))
                * static_cast<uint32_t>(serverBeat);
        }

        m_cache.duration = static_cast<uint16_t>(stepDuration);
        m_cache.walkDuration = static_cast<uint16_t>(
            std::min<int>(static_cast<int>(stepDuration) / tibia::config::spriteSize(), 16));

        // Diagonal dirs (bitmask 0xaa → NE/SE/SW/NW) multiply the ground duration
        // by 3 before beat rounding. Not the walk-tween continue scale.
        const double diagonalFactor =
            (tibia::game::clientRevision() > 810 || tibia::config::forcingNewWalkingFormula())
            ? (creature.m_isPlayer ? tibia::config::playerDiagonalWalkSpeed()
                                   : tibia::config::creatureDiagonalWalkSpeed())
            : 3.0;
        m_cache.diagonalDuration = static_cast<uint16_t>(stepDuration * diagonalFactor);
    }

    uint16_t duration = ignoreDiagonal ? m_cache.duration
                                       : m_cache.getDuration(creature.m_lastStepDirection);

    // Duration is fixed when TCreatureMovementTween::v9 installs the tween.
    creature.m_stepDuration = duration;
    return duration;
}

} // namespace tibia::creatures
