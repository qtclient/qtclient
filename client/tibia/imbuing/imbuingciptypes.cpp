#include "imbuing/imbuingciptypes.h"

namespace tibia::imbuing {

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

} // namespace tibia::imbuing
