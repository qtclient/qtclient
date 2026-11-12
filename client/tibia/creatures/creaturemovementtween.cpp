#include "creatures/creaturemovementtween.h"

#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/creaturestorage.h"
#include "animation/animator.h"
#include "animation/spriteanimator.h"
#include <tibia/config/runtimegameconfig.h>
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include "framework/core/scheduledevent.h"
#include "tibia/assets/things.h"
#include "tibia/assets/thingtype.h"
#include <tibia/game/gamesession.h>
#include "game/localplayersessionthing.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapfield.h"

#include <algorithm>
#include <cmath>
#include <tibia/game/thingsession_forward.h>

namespace tibia::creatures {

TCreatureMovementTween::TCreatureMovementTween(TCreature& owner)
    : m_owner(owner)
{}

ThingType* TCreatureMovementTween::outfitThingType() const
{
    return tibia::game::rawThingType(m_owner.m_outfit.isCreature() ? m_owner.m_outfit.getId()
                                                                   : m_owner.m_outfit.getAuxId(),
                                    m_owner.m_outfit.getCategory());
}

ThingType* TCreatureMovementTween::mountThingType() const
{
    return m_owner.m_outfit.hasMount()
        ? tibia::game::rawThingType(m_owner.m_outfit.getMount(), ThingCategoryCreature)
        : nullptr;
}

int TCreatureMovementTween::displacementXForWalk() const
{
    if (m_owner.m_outfit.isEffect())
        return 8;
    if (m_owner.m_outfit.isItem())
        return 0;
    if (ThingType* mountType = mountThingType())
        return mountType->getDisplacementX();
    if (ThingType* type = outfitThingType())
        return type->getDisplacementX();
    return m_owner.m_displacement.x;
}

int TCreatureMovementTween::displacementYForWalk() const
{
    if (m_owner.m_outfit.isEffect())
        return 8;
    if (m_owner.m_outfit.isItem())
        return 0;
    if (ThingType* mountType = mountThingType())
        return mountType->getDisplacementY();
    if (ThingType* type = outfitThingType())
        return type->getDisplacementY();
    return m_owner.m_displacement.y;
}

void TCreatureMovementTween::endDeltaForDirection(const uint8_t direction, int& dx, int& dy)
{
    // CipSoft — end-delta (offset unit at progress 0 while already
    // sitting on the destination tile).
    switch (static_cast<tibia::client::Direction>(direction)) {
        case tibia::client::North:
            dx = 0;
            dy = 1;
            break;
        case tibia::client::NorthEast:
            dx = -1;
            dy = 1;
            break;
        case tibia::client::East:
            dx = -1;
            dy = 0;
            break;
        case tibia::client::SouthEast:
            dx = -1;
            dy = -1;
            break;
        case tibia::client::South:
            dx = 0;
            dy = -1;
            break;
        case tibia::client::SouthWest:
            dx = 1;
            dy = -1;
            break;
        case tibia::client::West:
            dx = 1;
            dy = 0;
            break;
        case tibia::client::NorthWest:
            dx = 1;
            dy = 1;
            break;
        default:
            dx = 0;
            dy = 0;
            break;
    }
}

void TCreatureMovementTween::updateWalkOffset(const double progress)
{
    // CipSoft TCreatureMovementTween::v17 @ :
    // offset = (int)(endDelta * (1 - progress) * continueScale * 32).
    int dx = 0;
    int dy = 0;
    endDeltaForDirection(m_owner.m_direction, dx, dy);
    const double remaining =
        (1.0 - std::clamp(progress, 0.0, 1.0)) * m_owner.m_walkContinueScale;
    const double sprite = static_cast<double>(tibia::config::spriteSize());
    m_owner.m_walkOffset.x = static_cast<int>(dx * remaining * sprite);
    m_owner.m_walkOffset.y = static_cast<int>(dy * remaining * sprite);
}

void TCreatureMovementTween::clearWalkingTile()
{
    if (!m_owner.m_hasWalkingTile)
        return;
    if (tibia::worldmap::TWorldMapField* field = tibia::worldmap::fieldAt(m_owner.m_walkingTilePos))
        field->removeWalkingCreature(m_owner.m_creatureId);
    m_owner.m_hasWalkingTile = false;
    m_owner.m_walkingTilePos = {};
}

void TCreatureMovementTween::updateWalkingTile()
{
    tibia::worldmap::TWorldMapField* newWalkingTile = nullptr;

    const auto displacementX =
        tibia::game::cipFeature(tibia::client::GameNegativeOffset) ? 0 : displacementXForWalk();
    const auto displacementY =
        tibia::game::cipFeature(tibia::client::GameNegativeOffset) ? 0 : displacementYForWalk();

    const Rect virtualCreatureRect(
        tibia::config::spriteSize() + (m_owner.m_walkOffset.x - displacementX),
        tibia::config::spriteSize() + (m_owner.m_walkOffset.y - displacementY),
        tibia::config::spriteSize(), tibia::config::spriteSize());

    for (int xi = -1; xi <= 1 && !newWalkingTile; ++xi) {
        for (int yi = -1; yi <= 1 && !newWalkingTile; ++yi) {
            Rect virtualTileRect((xi + 1) * tibia::config::spriteSize(),
                                 (yi + 1) * tibia::config::spriteSize(),
                                 tibia::config::spriteSize(), tibia::config::spriteSize());
            if (virtualTileRect.contains(virtualCreatureRect.bottomRight()))
                newWalkingTile = tibia::worldmap::getOrCreateField(m_owner.m_position.translated(xi, yi, 0));
        }
    }

    const auto direction = static_cast<tibia::client::Direction>(m_owner.m_direction);
    if (m_owner.m_walkedPixels < tibia::config::spriteSize() / 2) {
        if (direction == tibia::client::Direction::NorthWest) {
            newWalkingTile = m_owner.m_hasWalkingTile ? tibia::worldmap::fieldAt(m_owner.m_walkingTilePos)
                                                      : tibia::worldmap::fieldAt(m_owner.m_position);
        } else if (direction == tibia::client::Direction::SouthEast) {
            newWalkingTile = tibia::worldmap::fieldAt(m_owner.m_position.translated(-1, -1, 0));
        }
    }

    const Position newPos = newWalkingTile ? newWalkingTile->getPosition() : Position{};
    if (m_owner.m_hasWalkingTile && newWalkingTile && newPos == m_owner.m_walkingTilePos)
        return;
    if (!m_owner.m_hasWalkingTile && !newWalkingTile)
        return;

    clearWalkingTile();

    if (newWalkingTile) {
        newWalkingTile->addWalkingCreature(m_owner.m_creatureId);
        m_owner.m_walkingTilePos = newWalkingTile->getPosition();
        m_owner.m_hasWalkingTile = true;
        m_owner.m_drawElevation = newWalkingTile->getDrawElevation();
        if (m_owner.m_isLocalPlayer) {
            tibia::worldmap::notifyTileUpdate(m_owner.m_walkingTilePos, nullptr,
                                                tibia::client::OPERATION_CLEAN);
        }
    }
}

void TCreatureMovementTween::updateWalkAnimation()
{
    if (!m_owner.m_outfit.isCreature())
        return;

    ThingType* type = mountThingType() ? mountThingType() : outfitThingType();
    if (!type)
        return;

    // CipSoft appearances::TAppearanceInstanceSpriteSequence drives
    // animation::TMovingOutfitAnimator (ctor 280..720 ms cycle / phase count).
    // Prior qtclient ports kept OTC footDelay clamps (20..205) — that is why feet
    // never matched CipSoft even after the lag-skip absolute-elapsed change.
    const animation::TMovingOutfitAnimator movingAnimator(type->getAnimator(), type->getIdleAnimator());
    if (!movingAnimator.movingAnimator() && type->getAnimationPhases() <= 1) {
        m_owner.m_walkAnimationPhase = 1;
        return;
    }

    const int phaseIndex = movingAnimator.movingAnimator()
        ? movingAnimator.phaseAt(m_owner.m_walkAnimTimer.ticksElapsed())
        : static_cast<int>(
              m_owner.m_walkAnimTimer.ticksElapsed()
              / std::max(1, animation::TMovingOutfitAnimator::kMaxWalkCycleMs
                                / std::max(1, type->getAnimationPhases())))
            % std::max(1, type->getAnimationPhases());
    // Tip draw path keeps walk phases 1-based (0 = idle in computeAnimationPhase).
    m_owner.m_walkAnimationPhase = static_cast<uint8_t>(phaseIndex + 1);
}

uint16_t TCreatureMovementTween::computeAnimationPhase(const bool mount) const
{
    ThingType* thingType = mount ? mountThingType() : outfitThingType();
    if (!thingType)
        return 0;

    if (const auto idleAnimator = thingType->getIdleAnimator()) {
        if (m_owner.m_walkAnimationPhase == 0)
            return static_cast<uint16_t>(idleAnimator->getPhase());
        return static_cast<uint16_t>(m_owner.m_walkAnimationPhase + idleAnimator->getAnimationPhases()
                                    - 1);
    }

    if (thingType->isAnimateAlways()) {
        if (const auto animator = thingType->getAnimator())
            return static_cast<uint16_t>(thingType->getIdleAnimationPhases() + animator->getPhase());

        const int animationPhases = thingType->getAnimationPhases();
        if (animationPhases <= 0)
            return 0;

        const int ticksPerFrame =
            std::max<int>(1, static_cast<int>(std::round(1000.0 / animationPhases)));
        const long long animationPeriod = static_cast<long long>(ticksPerFrame) * animationPhases;
        if (animationPeriod <= 0)
            return 0;

        const int idlePhases = thingType->getIdleAnimationPhases();
        return static_cast<uint16_t>(idlePhases
                                    + (g_clock.millis() % animationPeriod) / ticksPerFrame);
    }

    return m_owner.m_walkAnimationPhase;
}

void TCreatureMovementTween::refreshAnimationPhases()
{
    m_owner.m_animationPhase = computeAnimationPhase(false);
    m_owner.m_mountAnimationPhase = computeAnimationPhase(true);
}

// b1/6a40e4aff7682e7e — CipSoft body @ (names.txt; stamp VA may lag).
bool TCreatureMovementTween::v9(const Position& from, const Position& to)
{
    if (from == to)
        return false;

    // Same from→to while already walking is a no-op (confirm must not restart).
    if (m_owner.m_walking && m_owner.m_lastStepFrom == from && m_owner.m_lastStepTo == to)
        return false;

    // Always drop a pending idle-foot event — terminate arms one after each step,
    // and GoPath/key-repeat v9 must not let it zero feet mid next tween.
    if (m_owner.m_walkFinishAnimEvent) {
        m_owner.m_walkFinishAnimEvent->cancel();
        m_owner.m_walkFinishAnimEvent = nullptr;
    }

    const bool wasWalking = m_owner.m_walking;
    if (wasWalking) {
        clearWalkingTile();
        // CipSoft v9 continue scale: 1 + min(0.5, 1 - progress). Keeps the sprite
        // from snapping to a full-tile offset when the next MoveCreature arrives
        // before the prior duration tween finishes (monsters / other players).
        const double stepDuration = static_cast<double>(std::max<uint16_t>(1, m_owner.m_stepDuration));
        const double progress =
            std::clamp(static_cast<double>(m_owner.m_walkTimer.ticksElapsed()) / stepDuration, 0.0, 1.0);
        const double remaining = 1.0 - progress;
        m_owner.m_walkContinueScale = 1.0 + std::min(0.5, remaining);
    } else {
        m_owner.m_walkContinueScale = 1.0;
        // Fresh walk chain — start the foot-phase clock (CipSoft MovingOutfit v5).
        m_owner.m_walkAnimTimer.restart();
    }

    m_owner.m_lastStepDirection = from.getDirectionFromPosition(to);
    m_owner.m_lastStepFrom = from;
    m_owner.m_lastStepTo = to;
    m_owner.m_position = to;
    m_owner.setDirection(static_cast<uint8_t>(m_owner.m_lastStepDirection));

    m_owner.m_walking = true;
    m_owner.m_walkTimer.restart();
    m_owner.m_walkedPixels = 0;
    m_owner.m_walkTurnDirection = tibia::client::InvalidDirection;

    // Freeze duration for this tween (CIP installs once in v9; include diagonal ×3).
    m_owner.m_speedCalculator.v9(m_owner, false);
    return true;
}

void TCreatureMovementTween::advance()
{
    if (!m_owner.m_walking)
        return;

    const auto stepDuration = m_owner.m_stepDuration;
    if (stepDuration == 0)
        return;

    const double progress = std::clamp(
        static_cast<double>(m_owner.m_walkTimer.ticksElapsed()) / static_cast<double>(stepDuration),
        0.0, 1.0);
    const int sprite = static_cast<int>(tibia::config::spriteSize());
    const int totalPixelsWalked =
        std::min(sprite, static_cast<int>(progress * sprite + 0.5));
    m_owner.m_walkedPixels =
        static_cast<uint8_t>(std::max<int>(m_owner.m_walkedPixels, totalPixelsWalked));

    const Point oldWalkOffset = m_owner.m_walkOffset;

    updateWalkAnimation();
    updateWalkOffset(progress);
    updateWalkingTile();
    refreshAnimationPhases();

    if (m_owner.m_isLocalPlayer && oldWalkOffset != m_owner.m_walkOffset)
        tibia::game::notifyCameraMove({ m_owner.m_walkOffset.x, m_owner.m_walkOffset.y });

    if (progress >= 1.0 || m_owner.m_walkedPixels == tibia::config::spriteSize())
        terminate();
}

void TCreatureMovementTween::terminate()
{
    if (m_owner.m_terminatingWalk)
        return;
    m_owner.m_terminatingWalk = true;

    if (m_owner.m_walkTurnDirection != tibia::client::InvalidDirection) {
        m_owner.setDirection(static_cast<uint8_t>(m_owner.m_walkTurnDirection));
        m_owner.m_walkTurnDirection = tibia::client::InvalidDirection;
    }

    clearWalkingTile();

    m_owner.m_walkedPixels = 0;
    m_owner.m_walkOffset = {};
    m_owner.m_walkContinueScale = 1.0;
    m_owner.m_walking = false;
    m_owner.m_stepDuration = m_owner.m_speedCalculator.v9(m_owner);

    if (m_owner.m_walkFinishAnimEvent) {
        m_owner.m_walkFinishAnimEvent->cancel();
        m_owner.m_walkFinishAnimEvent = nullptr;
    }

    const TCreatureID id = m_owner.m_creatureId;
    m_owner.m_walkFinishAnimEvent = tibia::schedule::after([id] {
        if (TCreatureStorage* storage = TCreatureStorage::instance()) {
            if (const std::shared_ptr<TCreature> row = storage->creatureAt(id)) {
                row->m_walkFinishAnimEvent = nullptr;
                // Next GoPath/key step may already be live — never snap its feet idle.
                if (!row->isWalking()) {
                    row->m_walkAnimationPhase = 0;
                    row->movementTween().refreshAnimationPhases();
                }
            }
        }
    }, tibia::game::getServerBeat());

    // CipSoft: chain GoPath prediction from tween end (no Map Creature walk clock).
    if (m_owner.m_isLocalPlayer)
        tibia::game::onLocalPlayerMovementTweenTerminated();

    m_owner.m_terminatingWalk = false;
}

void TCreatureMovementTween::stop()
{
    if (!m_owner.m_walking)
        return;
    terminate();
}

void TCreatureMovementTween::abortForTeleport()
{
    // Floor change / full map teleport: CipSoft replaces TCreatureMovementTween
    // rather than finishing the old step.
    m_owner.clearPreWalks();

    if (m_owner.m_walkFinishAnimEvent) {
        m_owner.m_walkFinishAnimEvent->cancel();
        m_owner.m_walkFinishAnimEvent = nullptr;
    }

    if (m_owner.m_walkTurnDirection != tibia::client::InvalidDirection) {
        m_owner.setDirection(static_cast<uint8_t>(m_owner.m_walkTurnDirection));
        m_owner.m_walkTurnDirection = tibia::client::InvalidDirection;
    }

    clearWalkingTile();
    m_owner.m_walkedPixels = 0;
    m_owner.m_walkOffset = {};
    m_owner.m_walkContinueScale = 1.0;
    m_owner.m_walking = false;
    m_owner.m_walkAnimationPhase = 0;
    m_owner.m_walkAnimTimer.restart();
    m_owner.m_stepDuration = m_owner.m_speedCalculator.v9(m_owner);
    refreshAnimationPhases();

    // Teleport replaces the tween — no GoPath chain (session clears separately).
}

} // namespace tibia::creatures
