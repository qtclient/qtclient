#include "prey/preyciptypes.h"

namespace tibia::prey {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

}
