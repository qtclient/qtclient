#include "dailyreward/dailyrewardciptypes.h"

namespace tibia::dailyreward {

namespace {
struct CipTypedKeep {
    TDailyRewardFixedItems tDailyRewardFixedItems;
    TDailyRewardPickItems tDailyRewardPickItems;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

} // namespace tibia::dailyreward
