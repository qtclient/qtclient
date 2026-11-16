#pragma once

#include <QAbstractSocket>
#include <QByteArray>
#include <QJsonDocument>
#include <QObject>
#include <QTcpSocket>

namespace tibia {
namespace shared {

/// CIP `shared::TRPCServiceConnection`: one end of the local RPC channel.
/// Messages are JSON documents, each sent as a QDataStream byte array (a
/// 32-bit length, then the compact JSON).
class TRPCServiceConnection : public QObject
{
    Q_OBJECT

public:
    /// Takes over `socket`.
    TRPCServiceConnection(QTcpSocket* socket, QObject* parent);
    ~TRPCServiceConnection() override;

    void send(const QJsonDocument& message);
    /// Drops the socket (deleted later).
    void close();
    /// 0 reading a length, 1 reading a body, 2 holding a whole message.
    int readState() const;

Q_SIGNALS:
    void receivedMessage(TRPCServiceConnection* pConnection, QJsonDocument Message);
    void disconnected(TRPCServiceConnection* pConnection);
    void connectionError(TRPCServiceConnection* pConnection, QAbstractSocket::SocketError SocketError, QString ErrorMessage);
    void newDataReceived();

private:
    void onDataReady();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketStateChanged();

    QTcpSocket* m_socket = nullptr;
    int m_readState = 0;
    quint32 m_messageSize = 0;
    QByteArray m_message;
};

}
}
