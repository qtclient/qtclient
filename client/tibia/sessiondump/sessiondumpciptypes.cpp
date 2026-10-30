#include "sessiondump/sessiondumpciptypes.h"

namespace tibia::sessiondump {

namespace {
struct CipTypedKeep {
    TLightweightSessiondumpMessageProcessor tLightweightSessiondumpMessageProcessor;
    TSessiondumpFactory tSessiondumpFactory;
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
    (void)parent;
}

} // namespace tibia::sessiondump
