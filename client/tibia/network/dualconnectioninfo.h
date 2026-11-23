#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace tibia {
namespace network {

class TGameserverNetworkPacketConnection;

/// CIP network::TDualConnectionInfo: the game server connections of a dual
/// connection (the primary one, optionally a secondary one), in that order.
class TDualConnectionInfo
{
public:
    TDualConnectionInfo(const std::shared_ptr<TGameserverNetworkPacketConnection>& PrimaryConnection,
                        const std::shared_ptr<TGameserverNetworkPacketConnection>& SecondaryConnection);
    virtual ~TDualConnectionInfo();

    /// The first unconnected connection; the search stops at a connection that
    /// is still connecting or waiting for its login to be answered.
    virtual std::shared_ptr<TGameserverNetworkPacketConnection> connectionToConnect() const;
    virtual std::size_t connectedConnectionCount() const;
    virtual const std::vector<std::shared_ptr<TGameserverNetworkPacketConnection>>& connections() const;

private:
    std::vector<std::shared_ptr<TGameserverNetworkPacketConnection>> m_connections;
};

}
}
