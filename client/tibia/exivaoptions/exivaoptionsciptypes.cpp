#include "exivaoptions/exivaoptionsciptypes.h"

namespace tibia::exivaoptions {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

} // namespace tibia::exivaoptions
