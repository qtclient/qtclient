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
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // CIP TAuthenticationProcessController keep-alive. Live login remains on
    // TGameClient's TLoginRequestUploader; bindLoginUploader is opt-in.
    if (parent)
        new TAuthenticationProcessController(parent);
}

} // namespace tibia::authentication
