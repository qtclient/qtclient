#include "network/dualconnectioninfo.h"

#include "network/gameservernetworkpacketconnection.h"

namespace tibia::network {

TDualConnectionInfo::TDualConnectionInfo(const std::shared_ptr<TGameserverNetworkPacketConnection>& PrimaryConnection,
                                         const std::shared_ptr<TGameserverNetworkPacketConnection>& SecondaryConnection)
{
    if (PrimaryConnection)
        m_connections.push_back(PrimaryConnection);
    if (SecondaryConnection)
        m_connections.push_back(SecondaryConnection);
}

TDualConnectionInfo::~TDualConnectionInfo() = default;

std::shared_ptr<TGameserverNetworkPacketConnection> TDualConnectionInfo::connectionToConnect() const
{
    std::shared_ptr<TGameserverNetworkPacketConnection> connection;
    for (const auto& candidate : m_connections) {
        if (candidate->state() == TGameserverNetworkPacketConnection::Connecting
            || candidate->state() == TGameserverNetworkPacketConnection::LoginSent)
            return connection;
        if (!connection && candidate->isUnconnected())
            connection = candidate;
    }
    return connection;
}

std::size_t TDualConnectionInfo::connectedConnectionCount() const
{
    std::size_t count = 0;
    for (const auto& connection : m_connections)
        count += connection->isConnected() ? 1 : 0;
    return count;
}

const std::vector<std::shared_ptr<TGameserverNetworkPacketConnection>>& TDualConnectionInfo::connections() const
{
    return m_connections;
}

}
