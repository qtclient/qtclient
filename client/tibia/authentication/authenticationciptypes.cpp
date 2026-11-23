#include "authentication/authenticationciptypes.h"

namespace tibia::authentication {

void ownProductSurface(QObject* parent)
{
    Q_UNUSED(parent);
    static bool owned = false;
    if (owned)
        return;
    owned = true;
}

} // namespace tibia::authentication
