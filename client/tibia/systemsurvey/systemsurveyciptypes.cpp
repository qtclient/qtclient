#include "systemsurvey/systemsurveyciptypes.h"

namespace tibia::systemsurvey {

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

} // namespace tibia::systemsurvey
