#include "network/networkciptypes.h"

namespace tibia::network {

namespace {
struct CipTypedKeep {
    TDualConnectionInfo tDualConnectionInfo;
    TGameserverNetworkPacketRawDataProcessor tGameserverNetworkPacketRawDataProcessor;
    TGameserverNetworkPacketWriter tGameserverNetworkPacketWriter;
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

} // namespace tibia::network
