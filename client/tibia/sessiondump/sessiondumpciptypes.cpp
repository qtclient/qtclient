#include "sessiondump/sessiondumpciptypes.h"

namespace tibia::sessiondump {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

} // namespace tibia::sessiondump
