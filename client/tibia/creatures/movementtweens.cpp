#include "creatures/movementtweens.h"

#include "client/gamecore.h"
#include "creatures/creaturestorage.h"

namespace tibia::creatures {

void advanceAllMovementTweens(const int64_t timestampMs)
{
    // Nested afterAnimating / notifyCameraMove must not re-enter while the
    // predicted stamp is live (collapses DurationTween → paste-23 bodyElapsed 0).
    static bool s_inAdvance = false;
    if (s_inAdvance)
        return;
    s_inAdvance = true;

    client::gamecore::advanceTo(timestampMs * 1000);
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage) {
        s_inAdvance = false;
        return;
    }

    for (const TCreatureID id : storage->creatureIds()) {
        if (const std::shared_ptr<TCreature> row = storage->creatureAt(id))
            row->update(timestampMs);
    }

    s_inAdvance = false;
}

void resumeMovementClock()
{
    client::gamecore::resumeRealTime();
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
