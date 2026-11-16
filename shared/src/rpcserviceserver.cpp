#include "rpcserviceserver.h"

#include "rpcserviceconnection.h"
#include "rpcserviceerror.h"

#include <QHostAddress>

namespace tibia::shared {

TRPCServiceServer::TRPCServiceServer(const quint16 port, QObject* parent)
    : QObject(parent)
{
    if (!m_server.listen(QHostAddress(QStringLiteral("127.0.0.1")), port))
        throw TRPCServiceError(QStringLiteral("Cannot bind to port %1").arg(port));
    connect(&m_server, &QTcpServer::newConnection, this, &TRPCServiceServer::onNewConnection);
}

TRPCServiceServer::~TRPCServiceServer() = default;

void TRPCServiceServer::onMessageReceived(TRPCServiceConnection* connection, QJsonDocument message)
{
    Q_UNUSED(connection);
    Q_UNUSED(message);
}

void TRPCServiceServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        auto* connection = new TRPCServiceConnection(m_server.nextPendingConnection(), this);
        connect(connection, &TRPCServiceConnection::disconnected, this, &TRPCServiceServer::onConnectionDisconnected);
        connect(connection, &TRPCServiceConnection::receivedMessage, this, &TRPCServiceServer::onMessageReceived);
        m_connections.insert(connection);
        Q_EMIT connectionEstablished(connection);
    }
}

void TRPCServiceServer::onConnectionDisconnected(TRPCServiceConnection* connection)
{
    m_connections.erase(connection);
    connection->close();
    connection->deleteLater();
}

}
