#include "network/gameservernetworkpacket.h"

namespace tibia::network {

TGameserverNetworkPacket::TGameserverNetworkPacket(const EOrigin Origin, const QByteArray& Data)
    : m_origin(Origin)
    , m_data(Data)
{
}

}
