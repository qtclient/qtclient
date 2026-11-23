#include "authentication/authenticationciptypes.h"

namespace tibia::authentication {

namespace {
struct CipTypedKeep {
    TAuthenticationAndEncryptionInfo tAuthenticationAndEncryptionInfo;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void ownProductSurface(QObject* parent)
{
    Q_UNUSED(parent);
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
}

} // namespace tibia::authentication
