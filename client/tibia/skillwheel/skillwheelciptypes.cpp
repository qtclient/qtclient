#include "skillwheel/skillwheelciptypes.h"

namespace tibia::skillwheel {

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

} // namespace tibia::skillwheel
