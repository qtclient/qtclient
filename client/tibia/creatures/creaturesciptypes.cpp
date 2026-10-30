#include "creatures/creaturesciptypes.h"

namespace tibia::creatures {

namespace {
struct CipTypedKeep {
    TBattleListGameActionProvider tBattleListGameActionProvider;
    TCreatureMovementSpeedCalculator tCreatureMovementSpeedCalculator;
    TInspectPlayerGameActionProvider tInspectPlayerGameActionProvider;
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

} // namespace tibia::creatures
