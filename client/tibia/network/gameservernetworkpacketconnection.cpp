#include "network/gameservernetworkpacketconnection.h"

#include "network/gameserverpacketprocessor.h"
#include "network/gameservernetworkpacketreader.h"
#include "network/gameservernetworkpacketwriter.h"
#include "network/gameservertcpconnection.h"
#include "shared/iodevicereader.h"
#include "shared/iodevicewriter.h"

#include <QDataStream>

#include <utility>

namespace tibia::network {

TGameserverNetworkPacketConnection::TGameserverNetworkPacketConnection(std::shared_ptr<TGameserverTCPConnection> TcpConnection)
    : QObject(nullptr)
    , m_tcpConnection(std::move(TcpConnection))
{
    if (!m_tcpConnection)
        return;
    connect(m_tcpConnection.get(), &TGameserverTCPConnection::connected, this, &TGameserverNetworkPacketConnection::onConnected);
    connect(m_tcpConnection.get(), &TGameserverTCPConnection::disconnected, this,
            &TGameserverNetworkPacketConnection::onDisconnected);
    connect(m_tcpConnection.get(), &TGameserverTCPConnection::error, this, &TGameserverNetworkPacketConnection::onError);

    auto deviceReader = std::make_shared<shared::TIODeviceReader>(m_tcpConnection->socket(), QDataStream::LittleEndian);
    deviceReader->open(QIODevice::ReadOnly);
    auto reader = std::make_shared<TGameserverNetworkPacketReader>(deviceReader);
    auto deviceWriter = std::make_shared<shared::TIODeviceWriter>(m_tcpConnection->socket(), QDataStream::LittleEndian);
    auto writer = std::make_shared<TGameserverNetworkPacketWriter>(deviceWriter);
    m_packetProcessor = std::make_shared<TGameserverNetworkPacketProcessor>(reader, writer);

    connect(m_tcpConnection.get(), &TGameserverTCPConnection::readyRead, m_packetProcessor.get(),
            &TGameserverNetworkPacketProcessor::onNewPacketDataAvailable);
    connect(m_packetProcessor.get(), &TGameserverNetworkPacketProcessor::packetReceived, this,
            &TGameserverNetworkPacketConnection::onPacketReceived);
}

TGameserverNetworkPacketConnection::~TGameserverNetworkPacketConnection() = default;

void TGameserverNetworkPacketConnection::connectToGameserver(const QUrl& Url, const QString& WorldName)
{
    m_state = Connecting;
    m_tcpConnection->connectToGameserver(Url, WorldName);
}

void TGameserverNetworkPacketConnection::disconnectFromGameserver()
{
    m_tcpConnection->disconnectFromGameserver();
}

void TGameserverNetworkPacketConnection::processAvailablePackets()
{
    m_packetProcessor->processAvailablePackets();
}

TGameserverNetworkPacketConnection::EState TGameserverNetworkPacketConnection::state() const
{
    return m_state;
}

bool TGameserverNetworkPacketConnection::isConnected() const
{
    return m_tcpConnection->socket()->state() == QAbstractSocket::ConnectedState;
}

bool TGameserverNetworkPacketConnection::isUnconnected() const
{
    return m_tcpConnection->socket()->state() == QAbstractSocket::UnconnectedState;
}

QUrl TGameserverNetworkPacketConnection::url() const
{
    return m_tcpConnection->url();
}

void TGameserverNetworkPacketConnection::sendPacket(const TGameserverNetworkPacket& Packet)
{
    m_packetProcessor->sendPacket(Packet);
    if (Packet.encryptionType() == TGameserverNetworkPacket::LoginRsa)
        m_state = LoginSent;
}

void TGameserverNetworkPacketConnection::onConnected()
{
    Q_EMIT connected();
}

void TGameserverNetworkPacketConnection::onDisconnected()
{
    m_state = Disconnected;
    Q_EMIT disconnected();
}

void TGameserverNetworkPacketConnection::onError(QString ErrorDescription)
{
    m_state = Disconnected;
    Q_EMIT error(ErrorDescription);
}

void TGameserverNetworkPacketConnection::onPacketReceived(tibia::network::TGameserverNetworkPacket NetworkPacket)
{
    if (state() == LoginSent)
        m_state = LoggedIn;
    if (state() == LoggedIn)
        NetworkPacket.setEncryptionType(TGameserverNetworkPacket::Xtea);
    Q_EMIT packetReceived(NetworkPacket);
}

}
