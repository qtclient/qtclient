#include "gameWidnow/gameWidnowciptypes.h"

namespace tibia::gameWidnow {

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

} // namespace tibia::gameWidnow
