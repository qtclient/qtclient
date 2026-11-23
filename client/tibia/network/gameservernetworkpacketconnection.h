#pragma once

#include "network/gameservernetworkpacket.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace tibia {
namespace network {

class TGameserverNetworkPacketProcessor;
class TGameserverTCPConnection;

/// CIP network::TGameserverNetworkPacketConnection: one game server connection
/// at the packet level, with its login state. Packets that arrive after this
/// connection sent its login message are XTEA packets.
class TGameserverNetworkPacketConnection : public QObject
{
    Q_OBJECT
public:
    enum EState : int
    {
        Disconnected = 0,
        Connecting = 1,
        /// The login (or secondary login) message was sent.
        LoginSent = 2,
        /// A packet arrived after the login: the session runs on this connection.
        LoggedIn = 3,
    };

    explicit TGameserverNetworkPacketConnection(std::shared_ptr<TGameserverTCPConnection> TcpConnection);
    ~TGameserverNetworkPacketConnection() override;

    virtual void connectToGameserver(const QUrl& Url, const QString& WorldName);
    virtual void disconnectFromGameserver();
    /// Reads the packets that are complete.
    virtual void processAvailablePackets();
    virtual EState state() const;
    virtual bool isConnected() const;
    virtual bool isUnconnected() const;
    virtual QUrl url() const;
    virtual void sendPacket(const TGameserverNetworkPacket& Packet);

Q_SIGNALS:
    void connected();
    void disconnected();
    void error(QString ErrorDescription);
    void packetReceived(tibia::network::TGameserverNetworkPacket NetworkPacket);

public Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onError(QString ErrorDescription);
    void onPacketReceived(tibia::network::TGameserverNetworkPacket NetworkPacket);

private:
    std::shared_ptr<TGameserverTCPConnection> m_tcpConnection;
    std::shared_ptr<TGameserverNetworkPacketProcessor> m_packetProcessor;
    EState m_state = Disconnected;
};

}
}
