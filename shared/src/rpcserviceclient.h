#pragma once

#include <QAbstractSocket>
#include <QJsonDocument>
#include <QObject>
#include <QTimer>

namespace tibia {
namespace shared {

class TRPCServiceConnection;

/// CIP `shared::TRPCServiceClient`: connects to a local RPC server within a
/// timeout and reports the connection's life.
class TRPCServiceClient : public QObject
{
    Q_OBJECT

public:
    explicit TRPCServiceClient(QObject* parent = nullptr);
    ~TRPCServiceClient() override;

    void connectToServer(quint16 port, int timeoutMs);
    void send(const QJsonDocument& message);
    /// Drops the connection's socket.
    void closeConnection();

Q_SIGNALS:
    void connectionError(QAbstractSocket::SocketError SocketError, QString ErrorDescription);
    void connectionEstablished(TRPCServiceConnection* pConnection);
    void connectionDisconnected();

protected:
    /// A message from the server; CipSoft's base ignores it.
    virtual void onMessageReceived(TRPCServiceConnection* connection, QJsonDocument message);
    virtual void onConnected();
    virtual void onDisconnected();
    virtual void onConnectionError(TRPCServiceConnection* connection, QAbstractSocket::SocketError socketError,
        QString errorMessage);
    virtual void onConnectTimeout();

private:
    TRPCServiceConnection* m_connection = nullptr;
    QTimer m_connectTimer;
};

}
}
