#include "world/creaturewalkmotion.h"

#include "client/animator.h"
#include "client/gameconfig.h"
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include "framework/core/scheduledevent.h"
#include "tibia/assets/things.h"
#include "tibia/assets/thingtype.h"
#include "tibia/world/creature.h"
#include <tibia/game/gamesession.h>
#include "tibia/world/localplayer.h"
#include "tibia/world/map.h"
#include "worldmap/worldmapfield.h"

#include <algorithm>
#include <cmath>
#include <tibia/game/thingsession_forward.h>

namespace tibia::creatures {

ThingType* TCreature::outfitThingType() const
{
    return tibia::game::rawThingType(m_outfit.isCreature() ? m_outfit.getId() : m_outfit.getAuxId(),
                                    m_outfit.getCategory());
}

ThingType* TCreature::mountThingType() const
{
    return m_outfit.hasMount() ? tibia::game::rawThingType(m_outfit.getMount(), ThingCategoryCreature) : nullptr;
}

int TCreature::displacementXForWalk() const
{
    if (m_outfit.isEffect())
        return 8;
    if (m_outfit.isItem())
        return 0;
    if (ThingType* mountType = mountThingType())
        return mountType->getDisplacementX();
    if (ThingType* type = outfitThingType())
        return type->getDisplacementX();
    return m_displacement.x;
}

int TCreature::displacementYForWalk() const
{
    if (m_outfit.isEffect())
        return 8;
    if (m_outfit.isItem())
        return 0;
    if (ThingType* mountType = mountThingType())
        return mountType->getDisplacementY();
    if (ThingType* type = outfitThingType())
        return type->getDisplacementY();
    return m_displacement.y;
}

uint16_t TCreature::computeStepDuration(const bool ignoreDiagonal)
{
    // CIP TCreatureMovementSpeedCalculator always divides by a positive speed.
    // A transient 0 (parse gap / paralyze edge) must not freeze the tween.
    const uint16_t speed = std::max<uint16_t>(1, m_speed);

    const Position& tilePos = m_lastStepTo.isValid() ? m_lastStepTo : m_position;
    tibia::worldmap::TWorldMapField* tile = g_map.getTile(tilePos.isValid() ? tilePos : m_position);

    const int serverBeat = g_game.getServerBeat();

    int groundSpeed = 0;
    if (tile)
        groundSpeed = tile->getGroundSpeed();
    if (groundSpeed == 0)
        groundSpeed = 150;

    if (groundSpeed != m_stepCache.groundSpeed || speed != m_stepCache.speed) {
        m_stepCache.speed = speed;
        m_stepCache.groundSpeed = static_cast<uint16_t>(groundSpeed);

        uint32_t stepDuration = 1000u * static_cast<uint32_t>(groundSpeed);
        if (Creature::hasSpeedFormula()) {
            stepDuration /= std::max<uint16_t>(1, m_calculatedStepSpeed);
        } else {
            stepDuration /= speed;
        }

        if (g_gameConfig.isForcingNewWalkingFormula() || g_game.getClientVersion() >= 860) {
            stepDuration = ((stepDuration + static_cast<uint32_t>(serverBeat) - 1u)
                            / static_cast<uint32_t>(serverBeat))
                * static_cast<uint32_t>(serverBeat);
        }

        m_stepCache.duration = static_cast<uint16_t>(stepDuration);
        m_stepCache.walkDuration = static_cast<uint16_t>(
            std::min<int>(static_cast<int>(stepDuration) / g_gameConfig.getSpriteSize(), 16));

        const double diagonalFactor =
            (g_game.getClientVersion() > 810 || g_gameConfig.isForcingNewWalkingFormula())
            ? (m_isPlayer ? g_gameConfig.getPlayerDiagonalWalkSpeed()
                          : g_gameConfig.getCreatureDiagonalWalkSpeed())
            : 2.0;
        m_stepCache.diagonalDuration = static_cast<uint16_t>(stepDuration * diagonalFactor);
    }

    uint16_t duration = ignoreDiagonal ? m_stepCache.duration
                                       : m_stepCache.getDuration(m_lastStepDirection);

    if (m_isLocalPlayer) {
        duration = static_cast<uint16_t>(duration + 10 * std::max<int>(1, static_cast<int>(m_preWalks.size())));
    }

    m_stepDuration = duration;
    return duration;
}

void TCreature::updateWalkOffset(const uint8_t totalPixelsWalked)
{
    m_walkOffset = {};
    const auto direction = static_cast<tibia::client::Direction>(m_direction);
    if (direction == tibia::client::North || direction == tibia::client::NorthEast
        || direction == tibia::client::NorthWest)
        m_walkOffset.y = g_gameConfig.getSpriteSize() - totalPixelsWalked;
    else if (direction == tibia::client::South || direction == tibia::client::SouthEast
             || direction == tibia::client::SouthWest)
        m_walkOffset.y = totalPixelsWalked - g_gameConfig.getSpriteSize();

    if (direction == tibia::client::East || direction == tibia::client::NorthEast
        || direction == tibia::client::SouthEast)
        m_walkOffset.x = totalPixelsWalked - g_gameConfig.getSpriteSize();
    else if (direction == tibia::client::West || direction == tibia::client::NorthWest
             || direction == tibia::client::SouthWest)
        m_walkOffset.x = g_gameConfig.getSpriteSize() - totalPixelsWalked;
}

void TCreature::clearWalkingTile()
{
    if (!m_hasWalkingTile)
        return;
    if (tibia::worldmap::TWorldMapField* field = g_map.getTile(m_walkingTilePos))
        field->removeWalkingCreature(m_creatureId);
    m_hasWalkingTile = false;
    m_walkingTilePos = {};
}

void TCreature::updateWalkingTile()
{
    tibia::worldmap::TWorldMapField* newWalkingTile = nullptr;

    const auto displacementX = g_game.getFeature(tibia::client::GameNegativeOffset) ? 0 : displacementXForWalk();
    const auto displacementY = g_game.getFeature(tibia::client::GameNegativeOffset) ? 0 : displacementYForWalk();

    const Rect virtualCreatureRect(g_gameConfig.getSpriteSize() + (m_walkOffset.x - displacementX),
                                   g_gameConfig.getSpriteSize() + (m_walkOffset.y - displacementY),
                                   g_gameConfig.getSpriteSize(), g_gameConfig.getSpriteSize());

    for (int xi = -1; xi <= 1 && !newWalkingTile; ++xi) {
        for (int yi = -1; yi <= 1 && !newWalkingTile; ++yi) {
            Rect virtualTileRect((xi + 1) * g_gameConfig.getSpriteSize(),
                                 (yi + 1) * g_gameConfig.getSpriteSize(),
                                 g_gameConfig.getSpriteSize(), g_gameConfig.getSpriteSize());
            if (virtualTileRect.contains(virtualCreatureRect.bottomRight()))
                newWalkingTile = g_map.getOrCreateTile(m_position.translated(xi, yi, 0));
        }
    }

    const auto direction = static_cast<tibia::client::Direction>(m_direction);
    if (m_walkedPixels < g_gameConfig.getSpriteSize() / 2) {
        if (direction == tibia::client::Direction::NorthWest) {
            newWalkingTile = m_hasWalkingTile ? g_map.getTile(m_walkingTilePos)
                                              : g_map.getTile(m_position);
        } else if (direction == tibia::client::Direction::SouthEast) {
            newWalkingTile = g_map.getTile(m_position.translated(-1, -1, 0));
        }
    }

    const Position newPos = newWalkingTile ? newWalkingTile->getPosition() : Position{};
    if (m_hasWalkingTile && newWalkingTile && newPos == m_walkingTilePos)
        return;
    if (!m_hasWalkingTile && !newWalkingTile)
        return;

    clearWalkingTile();

    if (newWalkingTile) {
        newWalkingTile->addWalkingCreature(m_creatureId);
        m_walkingTilePos = newWalkingTile->getPosition();
        m_hasWalkingTile = true;
        m_drawElevation = newWalkingTile->getDrawElevation();
        if (m_isLocalPlayer) {
            if (const CreaturePtr mapCreature = g_map.getCreatureById(m_creatureId))
                g_map.notificateTileUpdate(m_walkingTilePos, mapCreature, tibia::client::OPERATION_CLEAN);
            else
                g_map.notificateTileUpdate(m_walkingTilePos, nullptr, tibia::client::OPERATION_CLEAN);
        }
    }
}

void TCreature::updateWalkAnimation()
{
    if (!m_outfit.isCreature())
        return;

    ThingType* mountType = mountThingType();
    int footAnimPhases = mountType ? mountType->getAnimationPhases()
                                   : (outfitThingType() ? outfitThingType()->getAnimationPhases() : 0);
    if (!g_game.getFeature(tibia::client::GameEnhancedAnimations) && footAnimPhases > 2)
        --footAnimPhases;

    if (footAnimPhases == 0)
        return;

    if (m_walkTimer.ticksElapsed() < computeStepDuration()
        && m_walkedPixels == g_gameConfig.getSpriteSize()) {
        m_walkAnimationPhase = 0;
        return;
    }

    int minFootDelay = 20;
    const int maxFootDelay = footAnimPhases > 2 ? 80 : 205;
    int footAnimDelay = footAnimPhases;

    if (g_game.getFeature(tibia::client::GameEnhancedAnimations) && footAnimPhases > 2) {
        minFootDelay += 10;
        if (footAnimDelay > 1)
            footAnimDelay = static_cast<int>(footAnimDelay / 1.5);
    }

    const auto walkSpeed = m_walkingAnimationSpeed > 0
        ? m_walkingAnimationSpeed
        : m_stepCache.getDuration(m_lastStepDirection);
    const int footDelay = std::clamp<int>(walkSpeed / std::max(1, footAnimDelay), minFootDelay, maxFootDelay);

    if (m_footTimer.ticksElapsed() >= footDelay) {
        if (m_walkAnimationPhase == footAnimPhases)
            m_walkAnimationPhase = 1;
        else
            ++m_walkAnimationPhase;
        m_footTimer.restart();
    }
}

uint16_t TCreature::computeAnimationPhase(const bool mount) const
{
    ThingType* thingType = mount ? mountThingType() : outfitThingType();
    if (!thingType)
        return 0;

    if (const auto idleAnimator = thingType->getIdleAnimator()) {
        if (m_walkAnimationPhase == 0)
            return static_cast<uint16_t>(idleAnimator->getPhase());
        return static_cast<uint16_t>(m_walkAnimationPhase + idleAnimator->getAnimationPhases() - 1);
    }

    if (thingType->isAnimateAlways()) {
        if (const auto animator = thingType->getAnimator())
            return static_cast<uint16_t>(thingType->getIdleAnimationPhases() + animator->getPhase());

        const int animationPhases = thingType->getAnimationPhases();
        if (animationPhases <= 0)
            return 0;

        const int ticksPerFrame = std::max<int>(1, static_cast<int>(std::round(1000.0 / animationPhases)));
        const long long animationPeriod = static_cast<long long>(ticksPerFrame) * animationPhases;
        if (animationPeriod <= 0)
            return 0;

        const int idlePhases = thingType->getIdleAnimationPhases();
        return static_cast<uint16_t>(idlePhases + (g_clock.millis() % animationPeriod) / ticksPerFrame);
    }

    return m_walkAnimationPhase;
}

void TCreature::refreshAnimationPhases()
{
    m_animationPhase = computeAnimationPhase(false);
    m_mountAnimationPhase = computeAnimationPhase(true);
}

void TCreature::beginWalk(const Position& from, const Position& to)
{
    if (from == to)
        return;

    m_lastStepDirection = from.getDirectionFromPosition(to);
    m_lastStepFrom = from;
    m_lastStepTo = to;
    m_position = to;
    setDirection(static_cast<uint8_t>(m_lastStepDirection));

    m_walking = true;
    m_walkTimer.restart();
    m_walkedPixels = 0;
    m_walkTurnDirection = tibia::client::InvalidDirection;

    if (m_walkFinishAnimEvent) {
        m_walkFinishAnimEvent->cancel();
        m_walkFinishAnimEvent = nullptr;
    }

    computeStepDuration(true);
}

void TCreature::advanceWalkTween()
{
    if (!m_walking)
        return;

    const auto stepDuration = computeStepDuration(true);
    if (stepDuration == 0)
        return;

    const float walkTicksPerPixel = stepDuration / static_cast<float>(g_gameConfig.getSpriteSize());
    if (walkTicksPerPixel <= 0.f)
        return;

    const int totalPixelsWalked = std::min<int>(m_walkTimer.ticksElapsed() / walkTicksPerPixel,
                                                g_gameConfig.getSpriteSize());
    m_walkedPixels = static_cast<uint8_t>(std::max<int>(m_walkedPixels, totalPixelsWalked));

    const Point oldWalkOffset = m_walkOffset;

    updateWalkAnimation();
    updateWalkOffset(m_walkedPixels);
    updateWalkingTile();
    refreshAnimationPhases();

    if (m_isLocalPlayer && oldWalkOffset != m_walkOffset)
        g_map.notificateCameraMove(m_walkOffset);

    if (m_walkedPixels == g_gameConfig.getSpriteSize())
        terminateWalk();
}

void TCreature::terminateWalk()
{
    if (m_terminatingWalk)
        return;
    m_terminatingWalk = true;

    if (m_walkTurnDirection != tibia::client::InvalidDirection) {
        setDirection(static_cast<uint8_t>(m_walkTurnDirection));
        m_walkTurnDirection = tibia::client::InvalidDirection;
    }

    clearWalkingTile();

    m_walkedPixels = 0;
    m_walkOffset = {};
    m_walking = false;
    m_stepDuration = computeStepDuration();

    if (m_walkFinishAnimEvent) {
        m_walkFinishAnimEvent->cancel();
        m_walkFinishAnimEvent = nullptr;
    }

    const TCreatureID id = m_creatureId;
    m_walkFinishAnimEvent = tibia::schedule::after([id] {
        if (TCreatureStorage* storage = TCreatureStorage::instance()) {
            if (const std::shared_ptr<TCreature> row = storage->creatureAt(id)) {
                row->m_walkAnimationPhase = 0;
                row->m_walkFinishAnimEvent = nullptr;
                row->refreshAnimationPhases();
            }
        }
    }, g_game.getServerBeat());

    // Clear LocalPlayer session Thing walk flags (no Map registry).
    if (m_isLocalPlayer) {
        if (const CreaturePtr mapCreature = g_map.getCreatureById(m_creatureId))
            mapCreature->applyStorageWalkTerminated();
    }

    m_terminatingWalk = false;
}

void TCreature::stopWalk()
{
    if (!m_walking)
        return;
    terminateWalk();
}

} // namespace tibia::creatures
