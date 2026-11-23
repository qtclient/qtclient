#pragma once

#include <QAbstractSocket>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

#if defined(Q_OS_WASM)
#include "network/gameserverwebsocketdevice.h"
#else
#include <QTcpSocket>
#endif

namespace tibia {
namespace network {

/// The socket under a game server connection: TCP on the desktop; in the
/// browser (no TCP) a WebSocket to the game server's WebSocket port carrying
/// the same byte stream.
#if defined(Q_OS_WASM)
using TGameserverSocket = TGameserverWebSocketDevice;
#else
using TGameserverSocket = QTcpSocket;
#endif

/// CIP network::TGameserverTCPConnection: the socket of one game server
/// connection. Once connected it announces the game world (its name and a
/// line feed), which the server routes the connection by.
class TGameserverTCPConnection : public QObject
{
    Q_OBJECT
public:
    explicit TGameserverTCPConnection(std::shared_ptr<TGameserverSocket> Socket);
    ~TGameserverTCPConnection() override;

    /// Connects to `Url`'s host and port for game world `WorldName`.
    void connectToGameserver(const QUrl& Url, const QString& WorldName);
    void disconnectFromGameserver();
    std::shared_ptr<TGameserverSocket> socket() const;
    QUrl url() const;

Q_SIGNALS:
    void connected();
    void disconnected();
    void error(QString ErrorDescription);
    void readyRead();

public Q_SLOTS:
    void onConnected();
    void onError(QAbstractSocket::SocketError SocketError);
    void onReadyRead();

private:
    void sendWorldName();

    std::shared_ptr<TGameserverSocket> m_socket;
    QString m_worldName;
    QUrl m_url;
};

}
}
