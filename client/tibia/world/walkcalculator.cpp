#include "walkcalculator.h"

#include "client/gamecore.h"
#include "creatures/creaturestorage.h"

#include <algorithm>

namespace tibia::world {

namespace {

WalkCalculator g_motion;

std::shared_ptr<tibia::creatures::TCreature> localStored()
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return nullptr;
    const LocalPlayerPtr local = player();
    if (!local)
        return nullptr;
    return storage->creatureAt(local->getId());
}

}

WalkCalculator& motion() { return g_motion; }

void WalkCalculator::advance(const int64_t timestampMs)
{
    client::gamecore::advanceTo(timestampMs * 1000);
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return;

    // Storage owns walk tween — no Map getCreatureById for offset animation.
    for (const tibia::creatures::TCreatureID id : storage->creatureIds()) {
        const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(id);
        if (!row)
            continue;
        if (row->isWalking())
            row->advanceWalkTween();
        row->refreshAnimationPhases();
    }

    // Pre-walk collision: storage-based check; LocalPlayer only mutates on cancel.
    if (const std::shared_ptr<tibia::creatures::TCreature> local = localStored()) {
        if (local->isPreWalking()) {
            const Position predicted = local->predictedPosition();
            if (tibia::worldmap::TWorldMapField* tile = fields().getTile(predicted)) {
                for (const uint32_t walkingId : tile->walkingCreatureIds()) {
                    if (walkingId == local->creatureId())
                        continue;
                    const std::shared_ptr<tibia::creatures::TCreature> other =
                        storage->creatureAt(walkingId);
                    if (other && other->position() == predicted && !other->isPassable()) {
                        if (const LocalPlayerPtr& mapLocal = player())
                            mapLocal->cancelPreWalkCollision();
                        break;
                    }
                }
            }
        }
    }
}

void WalkCalculator::resume()
{
    client::gamecore::resumeRealTime();
}

bool WalkCalculator::canStep(const bool ignoreLock) const
{
    const std::shared_ptr<tibia::creatures::TCreature> local = localStored();
    if (!local || !session().isOnline() || local->isDead())
        return false;
    if (local->isWalkLocked() && !ignoreLock)
        return false;
    if (session().getWalkMaxSteps() > 0) {
        if (local->preWalkingSize() > session().getWalkMaxSteps())
            return false;
    } else if (local->position() != local->serverPosition()) {
        return false;
    }
    if (local->isWalking()) {
        if (local->isAutoWalking())
            return true;
        if (local->isPreWalking())
            return false;
    }
    return local->walkTicksElapsed() >= local->stepDuration();
}

bool WalkCalculator::blocksStep(const tibia::client::Direction direction) const
{
    const std::shared_ptr<tibia::creatures::TCreature> local = localStored();
    if (!local)
        return false;
    tibia::worldmap::TWorldMapField* tile = fields().getTile(local->position().translatedToDirection(direction));
    return tile && !tile->isWalkable(true);
}

Point WalkCalculator::offset(const uint32_t creatureId) const
{
    if (!creatureId)
        return {};
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return {};
    if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(creatureId))
        return row->walkOffset();
    return {};
}

}
