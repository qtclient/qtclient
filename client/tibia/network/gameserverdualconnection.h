#pragma once

#include "network/dualconnectionurlinfo.h"
#include "network/gameservernetworkpacket.h"
#include "network/gameservernetworkpacketconnection.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <map>
#include <memory>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace shared {
class IClockService;
}
namespace network {

class TDualConnectionInfo;

/// CIP network::TGameserverDualConnection: the session's connection to the
/// game server over up to two game server connections. The primary one
/// connects first; with the "optimize connection stability" option on, a
/// secondary one connects once the server allowed it. Packets for every
/// connection carry one shared sequence number and go out on all logged-in
/// connections; the server sends each packet on both, the client keeps the
/// first copy (TGameserverNetworkPacketSequenceFlowProcessor).
class TGameserverDualConnection : public QObject
{
    Q_OBJECT
public:
    /// Without `ClientOptions` the secondary connection stays off.
    TGameserverDualConnection(std::shared_ptr<TDualConnectionInfo> DualConnectionInfo,
                              std::shared_ptr<shared::IClockService> ClockService,
                              std::shared_ptr<config::TClientOptions> ClientOptions);
    ~TGameserverDualConnection() override;

    /// Starts connecting to the URLs of `UrlInfo` for game world `WorldName`.
    virtual void connectToGameserver(std::shared_ptr<TDualConnectionUrlInfo> UrlInfo, const QString& WorldName);
    /// Closes every connection; no connection is made any more.
    virtual void disconnectFromGameserver();
    /// Reads the packets that are complete on every connection.
    virtual void processAvailablePackets();
    /// Writes `Packet` to the connections it is meant for.
    virtual void sendPacket(const TGameserverNetworkPacket& Packet);
    /// Picks the connections for `Packet` and gives it its sequence number.
    virtual void preparePacket(TGameserverNetworkPacket& Packet);
    /// The server allows a secondary connection.
    virtual void allowSecondaryConnection();
    virtual EConnectionsUsed connectionsUsed() const;

Q_SIGNALS:
    void connected(QUrl Url);
    void secondaryConnected(QUrl Url);
    void disconnected();
    void connectionsUsedChanged(tibia::network::EConnectionsUsed ConnectionsUsed);
    void error(QString ErrorDescription);
    void packetReceived(tibia::network::TGameserverNetworkPacket NetworkPacket);

public Q_SLOTS:
    /// Handles the disconnects since the last call, then connects what may be connected.
    void checkForStateChange();
    void onGameserverNetworkPacketConnectionConnected(
        std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection);
    void onGameserverNetworkPacketConnectionDisconnected(
        std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection);
    void onGameserverNetworkPacketConnectionError(
        std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection,
        QString ErrorDescription);
    void onGameserverNetworkPacketReceived(
        std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection,
        tibia::network::TGameserverNetworkPacket NetworkPacket);
    void processDisconnectEvents();
    void onOptimizeConnectionStabilityOptionChanged();

private:
    /// Starts the next connection when one is due.
    void connectNextConnection();

    std::shared_ptr<shared::IClockService> m_clockService;
    std::shared_ptr<config::TClientOptions> m_clientOptions;
    QString m_worldName;
    bool m_optimizeConnectionStability = false;
    bool m_disconnecting = false;
    std::shared_ptr<TDualConnectionUrlInfo> m_urlInfo;
    std::shared_ptr<TDualConnectionInfo> m_dualConnectionInfo;
    bool m_secondaryConnectionAllowed = false;
    uint32_t m_nextSequenceNumber = 1;
    bool m_wasConnected = false;
    /// Connections lost since the last processDisconnectEvents, with their error.
    std::map<std::shared_ptr<TGameserverNetworkPacketConnection>, QString> m_disconnectEvents;
};

}
}
