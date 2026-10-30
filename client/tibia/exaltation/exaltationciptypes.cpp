#include "exaltation/exaltationciptypes.h"

namespace tibia::exaltation {

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

} // namespace tibia::exaltation
