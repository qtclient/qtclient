#include "rpcserviceclient.h"

#include "rpcserviceconnection.h"

#include <QTcpSocket>

namespace tibia::shared {

TRPCServiceClient::TRPCServiceClient(QObject* parent)
    : QObject(parent)
{
    m_connectTimer.setSingleShot(true);
    connect(&m_connectTimer, &QTimer::timeout, this, &TRPCServiceClient::onConnectTimeout);
}

TRPCServiceClient::~TRPCServiceClient() = default;

void TRPCServiceClient::connectToServer(const quint16 port, const int timeoutMs)
{
    auto* socket = new QTcpSocket(this);
    connect(socket, &QAbstractSocket::connected, this, &TRPCServiceClient::onConnected);
    m_connection = new TRPCServiceConnection(socket, this);
    connect(m_connection, &TRPCServiceConnection::disconnected, this, &TRPCServiceClient::onDisconnected);
    connect(m_connection, &TRPCServiceConnection::connectionError, this, &TRPCServiceClient::onConnectionError);
    connect(m_connection, &TRPCServiceConnection::receivedMessage, this, &TRPCServiceClient::onMessageReceived);
    socket->connectToHost(QStringLiteral("localhost"), port, QIODevice::ReadWrite, QAbstractSocket::AnyIPProtocol);
    m_connectTimer.start(timeoutMs);
}

void TRPCServiceClient::send(const QJsonDocument& message)
{
    if (!m_connection)
        return;
    m_connection->send(message);
}

void TRPCServiceClient::closeConnection()
{
    if (m_connection)
        m_connection->close();
}

void TRPCServiceClient::onMessageReceived(TRPCServiceConnection* connection, QJsonDocument message)
{
    Q_UNUSED(connection);
    Q_UNUSED(message);
}

void TRPCServiceClient::onConnected()
{
    m_connectTimer.stop();
    if (!m_connection)
        return;
    Q_EMIT connectionEstablished(m_connection);
}

void TRPCServiceClient::onDisconnected()
{
    m_connectTimer.stop();
    Q_EMIT connectionDisconnected();
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TRPCServiceClient::onConnectionError(TRPCServiceConnection* connection,
    const QAbstractSocket::SocketError socketError, QString errorMessage)
{
    Q_UNUSED(connection);
    m_connectTimer.stop();
    Q_EMIT connectionError(socketError, errorMessage);
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TRPCServiceClient::onConnectTimeout()
{
    if (!m_connection)
        return;
    onConnectionError(m_connection, QAbstractSocket::SocketTimeoutError, QStringLiteral("Connection timed out"));
}

}
