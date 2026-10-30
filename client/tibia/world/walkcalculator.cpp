#include "walkcalculator.h"

#include "client/gamecore.h"

#include <algorithm>

namespace tibia::world {

namespace {

WalkCalculator g_motion;

}

WalkCalculator& motion() { return g_motion; }

void WalkCalculator::advance(const int64_t timestampMs)
{
    client::gamecore::advanceTo(timestampMs * 1000);
    for (const auto& [id, creature] : fields().getCreatures()) {
        if (creature)
            creature->advanceWalkTween();
    }
    if (const LocalPlayerPtr& local = player())
        local->advanceWalkTween();
}

void WalkCalculator::resume()
{
    client::gamecore::resumeRealTime();
}

bool WalkCalculator::canStep(const bool ignoreLock) const
{
    const LocalPlayerPtr local = player();
    if (!local || !session().isOnline() || local->isDead())
        return false;
    if (local->isWalkLocked() && !ignoreLock)
        return false;
    if (session().getWalkMaxSteps() > 0) {
        if (local->getPreWalkingSize() > session().getWalkMaxSteps())
            return false;
    } else if (local->getPosition() != local->getServerPosition()) {
        return false;
    }
    if (local->isWalking()) {
        if (local->isAutoWalking())
            return true;
        if (local->isPreWalking())
            return false;
    }
    return local->getWalkTicksElapsed() >= local->getStepDuration();
}

bool WalkCalculator::blocksStep(const Otc::Direction direction) const
{
    const LocalPlayerPtr local = player();
    if (!local)
        return false;
    const TilePtr& tile = fields().getTile(local->getPosition().translatedToDirection(direction));
    return tile && !tile->isWalkable(true);
}

Point WalkCalculator::offset(const Creature& creature) const
{
    return const_cast<Creature&>(creature).getWalkOffset();
}

}
