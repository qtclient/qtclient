#include "network/gameservertcpconnection.h"

#include <QVariant>

#include <utility>

namespace tibia::network {

TGameserverTCPConnection::TGameserverTCPConnection(std::shared_ptr<TGameserverSocket> Socket)
    : QObject(nullptr)
    , m_socket(std::move(Socket))
{
    connect(m_socket.get(), &TGameserverSocket::connected, this, &TGameserverTCPConnection::onConnected);
    connect(m_socket.get(), &TGameserverSocket::disconnected, this, &TGameserverTCPConnection::disconnected);
    connect(m_socket.get(), &TGameserverSocket::errorOccurred, this, &TGameserverTCPConnection::onError);
    connect(m_socket.get(), &TGameserverSocket::readyRead, this, &TGameserverTCPConnection::onReadyRead);
}

TGameserverTCPConnection::~TGameserverTCPConnection() = default;

void TGameserverTCPConnection::connectToGameserver(const QUrl& Url, const QString& WorldName)
{
    m_worldName = WorldName;
    m_url = Url;
    m_socket->connectToHost(m_url.host(), static_cast<quint16>(m_url.port()), QIODevice::ReadWrite,
                            QAbstractSocket::AnyIPProtocol);
}

void TGameserverTCPConnection::disconnectFromGameserver()
{
    m_socket->disconnectFromHost();
}

std::shared_ptr<TGameserverSocket> TGameserverTCPConnection::socket() const
{
    return m_socket;
}

QUrl TGameserverTCPConnection::url() const
{
    return m_url;
}

void TGameserverTCPConnection::onConnected()
{
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, QVariant(1));
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, QVariant(0));
    Q_EMIT connected();
    sendWorldName();
}

void TGameserverTCPConnection::onError(QAbstractSocket::SocketError)
{
    Q_EMIT error(m_socket->errorString());
}

void TGameserverTCPConnection::onReadyRead()
{
    Q_EMIT readyRead();
}

void TGameserverTCPConnection::sendWorldName()
{
    QByteArray line = m_worldName.toLocal8Bit();
    line.append('\n');
    m_socket->write(line);
}

}
