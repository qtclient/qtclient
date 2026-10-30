#include "qmlcomponents/qmlcomponentsciptypes.h"

namespace tibia::qmlcomponents {

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

} // namespace tibia::qmlcomponents
