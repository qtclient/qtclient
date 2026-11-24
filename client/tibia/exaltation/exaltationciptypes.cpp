#include "exaltation/exaltationciptypes.h"

namespace tibia::exaltation {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

}
