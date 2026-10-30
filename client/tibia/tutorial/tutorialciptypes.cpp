#include "tutorial/tutorialciptypes.h"

namespace tibia::tutorial {

namespace {
struct CipTypedKeep {
    TSessiondumpHintBase tSessiondumpHintBase;
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

} // namespace tibia::tutorial
