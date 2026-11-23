#include "network/gameserverdualconnection.h"

#include "network/dualconnectioninfo.h"
#include "network/gameservernetworkpacketconnection.h"

#include "app/clientoptions.h"
#include "iclockservice.h"

#include <QDebug>

#include <utility>

namespace tibia::network {

namespace {

const QLatin1String OptimizeConnectionStabilityOption("connectionStabilityOptimizationEnabled");

}

TGameserverDualConnection::TGameserverDualConnection(std::shared_ptr<TDualConnectionInfo> DualConnectionInfo,
                                                     std::shared_ptr<shared::IClockService> ClockService)
    : QObject(nullptr)
    , m_clockService(std::move(ClockService))
    , m_dualConnectionInfo(std::move(DualConnectionInfo))
{
    onOptimizeConnectionStabilityOptionChanged();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& Name) {
        if (Name == OptimizeConnectionStabilityOption)
            onOptimizeConnectionStabilityOptionChanged();
    });
    if (!m_dualConnectionInfo)
        return;
    for (const auto& connection : m_dualConnectionInfo->connections()) {
        const std::weak_ptr<TGameserverNetworkPacketConnection> weak = connection;
        connect(connection.get(), &TGameserverNetworkPacketConnection::connected, this, [this, weak] {
            if (const auto locked = weak.lock())
                onGameserverNetworkPacketConnectionConnected(locked);
        });
        connect(connection.get(), &TGameserverNetworkPacketConnection::disconnected, this, [this, weak] {
            if (const auto locked = weak.lock())
                onGameserverNetworkPacketConnectionDisconnected(locked);
        });
        connect(connection.get(), &TGameserverNetworkPacketConnection::error, this, [this, weak](const QString& ErrorDescription) {
            if (const auto locked = weak.lock())
                onGameserverNetworkPacketConnectionError(locked, ErrorDescription);
        });
        connect(connection.get(), &TGameserverNetworkPacketConnection::packetReceived, this,
                [this, weak](const TGameserverNetworkPacket& NetworkPacket) {
                    if (const auto locked = weak.lock())
                        onGameserverNetworkPacketReceived(locked, NetworkPacket);
                });
    }
}

TGameserverDualConnection::~TGameserverDualConnection() = default;

void TGameserverDualConnection::connectToGameserver(std::shared_ptr<TDualConnectionUrlInfo> UrlInfo, const QString& WorldName)
{
    m_urlInfo = std::move(UrlInfo);
    m_worldName = WorldName;
    m_urlInfo->resetNextConnectTimes(m_clockService->nowMillis());
    connectNextConnection();
}

void TGameserverDualConnection::disconnectFromGameserver()
{
    m_disconnecting = true;
    for (const auto& connection : m_dualConnectionInfo->connections())
        connection->disconnectFromGameserver();
}

void TGameserverDualConnection::processAvailablePackets()
{
    for (const auto& connection : m_dualConnectionInfo->connections())
        connection->processAvailablePackets();
}

void TGameserverDualConnection::sendPacket(const TGameserverNetworkPacket& Packet)
{
    for (const auto& connection : m_dualConnectionInfo->connections()) {
        const bool primary = m_urlInfo->isPrimaryUrl(connection->url());
        bool readyForPacket = false;
        if (Packet.encryptionType() < TGameserverNetworkPacket::Xtea)
            readyForPacket = true;
        else if (Packet.encryptionType() == TGameserverNetworkPacket::Xtea)
            readyForPacket = connection->state() == TGameserverNetworkPacketConnection::LoggedIn;
        const bool connected = connection->isConnected();
        const EConnectionsUsed target = Packet.connectionsUsed();
        if (readyForPacket && connected
            && (target == EConnectionsUsed::PrimaryAndSecondary || (primary && target == EConnectionsUsed::Primary)
                || (!primary && target == EConnectionsUsed::Secondary)))
            connection->sendPacket(Packet);
    }
}

void TGameserverDualConnection::preparePacket(TGameserverNetworkPacket& Packet)
{
    if (Packet.encryptionType() == TGameserverNetworkPacket::Unencrypted
        || Packet.encryptionType() == TGameserverNetworkPacket::LoginRsa) {
        // A login message goes to the connection that is logging in.
        for (const auto& connection : m_dualConnectionInfo->connections()) {
            const bool primary = m_urlInfo->isPrimaryUrl(connection->url());
            if (connection->isConnected() && connection->state() == TGameserverNetworkPacketConnection::Connecting) {
                Packet.setConnectionsUsed(static_cast<EConnectionsUsed>(int(primary) + 1));
                break;
            }
        }
    }
    if (Packet.connectionsUsed() == EConnectionsUsed::PrimaryAndSecondary)
        Packet.setSequenceNumber(m_nextSequenceNumber++);
    else
        Packet.setSequenceNumber(0);
}

void TGameserverDualConnection::allowSecondaryConnection()
{
    m_secondaryConnectionAllowed = true;
    connectNextConnection();
}

EConnectionsUsed TGameserverDualConnection::connectionsUsed() const
{
    if (!m_urlInfo)
        return EConnectionsUsed::None;
    const EConnectionsUsed used = m_urlInfo->connectionsUsed();
    const int value = static_cast<int>(used);
    return value >= 1 && value <= 3 ? used : EConnectionsUsed::None;
}

void TGameserverDualConnection::checkForStateChange()
{
    processDisconnectEvents();
    if (m_disconnecting)
        return;
    connectNextConnection();
}

void TGameserverDualConnection::onGameserverNetworkPacketConnectionConnected(
    std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection)
{
    m_wasConnected = true;
    const QUrl url = pGameserverNetworkPacketConnection->url();
    m_urlInfo->setConnected(url);
    const std::size_t connectedCount = m_dualConnectionInfo->connectedConnectionCount();
    if (connectedCount == 1)
        Q_EMIT connected(url);
    else if (connectedCount == m_dualConnectionInfo->connections().size())
        Q_EMIT secondaryConnected(url);
    Q_EMIT connectionsUsedChanged(connectionsUsed());
}

void TGameserverDualConnection::onGameserverNetworkPacketConnectionDisconnected(
    std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection)
{
    m_disconnectEvents.emplace(pGameserverNetworkPacketConnection, QString());
}

void TGameserverDualConnection::onGameserverNetworkPacketConnectionError(
    std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection,
    QString ErrorDescription)
{
    m_disconnectEvents[pGameserverNetworkPacketConnection] = ErrorDescription;
}

void TGameserverDualConnection::onGameserverNetworkPacketReceived(
    std::shared_ptr<tibia::network::TGameserverNetworkPacketConnection> pGameserverNetworkPacketConnection,
    tibia::network::TGameserverNetworkPacket NetworkPacket)
{
    const QUrl url = pGameserverNetworkPacketConnection->url();
    if (NetworkPacket.encryptionType() == TGameserverNetworkPacket::Xtea)
        m_urlInfo->resetFailedConnectAttempts(url, m_clockService->nowMillis());
    TGameserverNetworkPacket packet = NetworkPacket;
    packet.setConnectionIndex(m_urlInfo->isPrimaryUrl(url) ? 0 : 1);
    Q_EMIT packetReceived(packet);
}

void TGameserverDualConnection::processDisconnectEvents()
{
    if (m_disconnectEvents.empty())
        return;
    const int64_t now = m_clockService->nowMillis();
    QString errorDescription;
    for (const auto& [connection, eventError] : m_disconnectEvents) {
        const QUrl url = connection->url();
        if (!eventError.isEmpty())
            errorDescription = eventError;
        qInfo().noquote() << QStringLiteral("Connection to gameserver ") << url.url() << m_worldName
                          << QStringLiteral(" was disconnected (") << errorDescription << QStringLiteral(")");
        m_urlInfo->setDisconnected(url, now, true);
    }
    if (m_dualConnectionInfo->connectedConnectionCount() == 0 && (m_urlInfo->failedConnectAttempts() > 1 || m_wasConnected)) {
        m_disconnecting = true;
        if (!errorDescription.isEmpty())
            Q_EMIT error(errorDescription);
        Q_EMIT disconnected();
    }
    m_disconnectEvents.clear();
    Q_EMIT connectionsUsedChanged(connectionsUsed());
}

void TGameserverDualConnection::onOptimizeConnectionStabilityOptionChanged()
{
    m_optimizeConnectionStability = optimizeConnectionStabilityOption();
    if (m_optimizeConnectionStability || connectionsUsed() != EConnectionsUsed::PrimaryAndSecondary)
        return;
    const auto& connections = m_dualConnectionInfo->connections();
    auto secondary = connections.at(0);
    for (const auto& connection : connections) {
        if (!m_urlInfo->isPrimaryUrl(connection->url())) {
            secondary = connection;
            break;
        }
    }
    if (secondary->isConnected())
        secondary->disconnectFromGameserver();
}

void TGameserverDualConnection::connectNextConnection()
{
    if (m_disconnecting || !m_urlInfo)
        return;
    if ((!m_optimizeConnectionStability || !m_secondaryConnectionAllowed) && connectionsUsed() != EConnectionsUsed::None)
        return;
    if (m_dualConnectionInfo->connectedConnectionCount() == 1 && !m_optimizeConnectionStability)
        return;
    const QUrl url = m_urlInfo->nextUrlToConnect(m_clockService->nowMillis());
    if (url == QUrl())
        return;
    if (const auto connection = m_dualConnectionInfo->connectionToConnect()) {
        qInfo().noquote() << QStringLiteral("Request connection to gameserver ") << url.url() << m_worldName;
        connection->connectToGameserver(url, m_worldName);
    }
}

bool TGameserverDualConnection::optimizeConnectionStabilityOption()
{
    return app::ClientOptions::load().options().value(OptimizeConnectionStabilityOption).toBool(true);
}

}
