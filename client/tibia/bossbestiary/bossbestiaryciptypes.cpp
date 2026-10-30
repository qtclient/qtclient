#include "bossbestiary/bossbestiaryciptypes.h"

namespace tibia::bossbestiary {

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

} // namespace tibia::bossbestiary
