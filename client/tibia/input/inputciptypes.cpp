#include "input/inputciptypes.h"

namespace tibia::input {

namespace {
struct CipTypedKeep {
    TLocationDepotSearchGameActionContext tLocationDepotSearchGameActionContext;
    TLocationNPCChatKeywordGameActionContext tLocationNPCChatKeywordGameActionContext;
    TUpgradeableObjectApperanceTypeGameActionContext tUpgradeableObjectApperanceTypeGameActionContext;
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

} // namespace tibia::input
