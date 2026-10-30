#include "container/containerciptypes.h"

namespace tibia::container {

namespace {
struct CipTypedKeep {
    TContainer tContainer;
    TContainerGameActionProvider tContainerGameActionProvider;
    TContainerRenderProvider tContainerRenderProvider;
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

} // namespace tibia::container
