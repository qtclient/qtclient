#include "bossbestiary/bossbestiaryciptypes.h"

namespace tibia::bossbestiary {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    // Session-owned CIP bosstiary storage (live notify → moc signals).
    // TBossTrackerStorage is stack-owned in main beside sidebar trackers.
    new TBosstiaryStorage(parent);
    new TBossBestiaryProtocolMessageHandler(parent);
}

} // namespace tibia::bossbestiary
