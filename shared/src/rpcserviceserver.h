#pragma once

#include <QJsonDocument>
#include <QObject>
#include <QTcpServer>

#include <set>

namespace tibia {
namespace shared {

class TRPCServiceConnection;

/// CIP `shared::TRPCServiceServer`: accepts local RPC connections on
/// 127.0.0.1:`port`; throws TRPCServiceError when the port is taken.
class TRPCServiceServer : public QObject
{
    Q_OBJECT

public:
    TRPCServiceServer(quint16 port, QObject* parent = nullptr);
    ~TRPCServiceServer() override;

Q_SIGNALS:
    void connectionEstablished(TRPCServiceConnection* pConnection);

protected:
    /// A message from one of the connections; CipSoft's base ignores it.
    virtual void onMessageReceived(TRPCServiceConnection* connection, QJsonDocument message);

private:
    void onNewConnection();
    void onConnectionDisconnected(TRPCServiceConnection* connection);

    QTcpServer m_server;
    std::set<TRPCServiceConnection*> m_connections;
};

}
}
