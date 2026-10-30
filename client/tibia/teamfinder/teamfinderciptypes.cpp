#include "teamfinder/teamfinderciptypes.h"

namespace tibia::teamfinder {

namespace {
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

} // namespace tibia::teamfinder
