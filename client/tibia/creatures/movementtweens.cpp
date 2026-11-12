#include "creatures/movementtweens.h"

#include "client/gamecore.h"
#include "creatures/creaturestorage.h"
#include <tibia/game/gamesession.h>

namespace tibia::creatures {

namespace {

tibia::game::MapPos toMapPos(const Position& pos)
{
    return { pos.x, pos.y, pos.z };
}

std::shared_ptr<TCreature> localStored()
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return nullptr;
    const uint32_t id = tibia::game::localPlayerId();
    if (!id)
        return nullptr;
    return storage->creatureAt(id);
}

} // namespace

void advanceAllMovementTweens(const int64_t timestampMs)
{
    client::gamecore::advanceTo(timestampMs * 1000);
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;

    for (const TCreatureID id : storage->creatureIds()) {
        const std::shared_ptr<TCreature> row = storage->creatureAt(id);
        if (!row)
            continue;
        if (row->isWalking())
            row->advanceWalkTween();
        row->refreshAnimationPhases();
    }

    // Predicted-step collision: storage-based check; LocalPlayerSessionThing only mutates on cancel.
    if (const std::shared_ptr<TCreature> local = localStored()) {
        if (local->isPreWalking()) {
            const Position predicted = local->predictedPosition();
            for (const uint32_t walkingId : tibia::game::walkingCreatureIds(toMapPos(predicted))) {
                if (walkingId == local->creatureId())
                    continue;
                const std::shared_ptr<TCreature> other = storage->creatureAt(walkingId);
                if (other && other->position() == predicted && !other->isPassable()) {
                    tibia::game::localPlayerCancelPredictedStepCollision();
                    break;
                }
            }
        }
        // CipSoft: chain the next GoPath prediction at step boundaries.
        tibia::game::localPlayerTryBeginNextGoPathStep();
    }
}

void resumeMovementClock()
{
    client::gamecore::resumeRealTime();
}

bool canBeginStep(const bool ignoreLock)
{
    const std::shared_ptr<TCreature> local = localStored();
    if (!local || !tibia::game::isOnline() || local->isDead())
        return false;
    if (local->isWalkLocked() && !ignoreLock)
        return false;
    if (tibia::game::getWalkMaxSteps() > 0) {
        if (local->preWalkingSize() >= tibia::game::getWalkMaxSteps())
            return false;
    } else if (local->position() != local->serverPosition()) {
        return false;
    }
    // CipSoft: one live movement tween — chain the next step only after it ends.
    if (local->isWalking())
        return false;
    return local->walkTicksElapsed() >= local->stepDuration();
}

bool blocksStep(const tibia::client::Direction direction)
{
    const std::shared_ptr<TCreature> local = localStored();
    if (!local)
        return false;
    const Position next = local->position().translatedToDirection(direction);
    return tibia::game::blocksWalk(toMapPos(next), true);
}

Point walkOffsetFor(const uint32_t creatureId)
{
    if (!creatureId)
        return {};
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return {};
    if (const std::shared_ptr<TCreature> row = storage->creatureAt(creatureId))
        return row->walkOffset();
    return {};
}

} // namespace tibia::creatures
