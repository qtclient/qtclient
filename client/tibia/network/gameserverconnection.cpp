#include "network/gameserverconnection.h"

#include "authentication/authenticationandencryptioninfo.h"
#include "client/gamecore.h"
#include "game/thingsession_forward.h"
#include "network/dualconnectioninfo.h"
#include "network/dualconnectionurlinfo.h"
#include "network/gameserverdualconnection.h"
#include "network/gameservernetworkpacketconnection.h"
#include "network/gameservernetworkpacketrawdataprocessor.h"
#include "network/gameservernetworkpacketsequenceflowprocessor.h"
#include "network/gameservertcpconnection.h"
#include "network/unencryptedrawmessagestream.h"
#include "protocol/extendedopcode.h"
#include "protocol/protocolciptypes.h"

#include "clockservice.h"
#include "compressionhelper.h"
#include "rsahelper.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

#include <QUrl>

#include <filesystem>
#include <iomanip>
#include <utility>

namespace tibia::network {

namespace {

TGameserverSessionNetwork* g_live = nullptr;

constexpr uint8_t LoginOpcode = 10;
constexpr uint8_t SecondaryLoginOpcode = 11;
constexpr uint8_t QuitGameOpcode = 20;
constexpr uint8_t ConnectionPingBackOpcode = 28;

/// The session's network tick (CipSoft runs it with the game session's tick).
constexpr int NetworkTickMillis = 10;

std::shared_ptr<TGameserverNetworkPacketConnection> newGameserverNetworkPacketConnection()
{
    auto socket = std::make_shared<TGameserverSocket>();
    auto tcpConnection = std::make_shared<TGameserverTCPConnection>(socket);
    return std::make_shared<TGameserverNetworkPacketConnection>(tcpConnection);
}

}

TGameserverSessionNetwork::TGameserverSessionNetwork(QObject* parent)
    : QObject(parent)
{
    g_live = this;
    m_networkTick.setInterval(NetworkTickMillis);
    connect(&m_networkTick, &QTimer::timeout, this, [this] {
        if (!m_dualConnection)
            return;
        // The dual connection may end this network from inside the tick.
        const auto dualConnection = m_dualConnection;
        dualConnection->processAvailablePackets();
        dualConnection->checkForStateChange();
    });
    m_readyForSecondaryConnectionHook = tibia::game::SoftHook(
        "onReadyForSecondaryConnection", [this](const tibia::game::TGameNotify::Args& args) {
            onReadyForSecondaryConnection(client::gamecore::argString(args, 0));
            return std::any{};
        });
}

TGameserverSessionNetwork::~TGameserverSessionNetwork()
{
    releaseNetworkStack();
    if (g_live == this)
        g_live = nullptr;
}

TGameserverSessionNetwork* TGameserverSessionNetwork::live()
{
    return g_live;
}

void TGameserverSessionNetwork::buildNetworkStack()
{
    auto primaryConnection = newGameserverNetworkPacketConnection();
    auto secondaryConnection = newGameserverNetworkPacketConnection();
    auto dualConnectionInfo = std::make_shared<TDualConnectionInfo>(primaryConnection, secondaryConnection);
    m_dualConnection = std::make_shared<TGameserverDualConnection>(dualConnectionInfo, shared::TClockService::process());
    m_serverMessageStream = std::make_shared<TUnencryptedRawMessageStream>();
    m_compressionHelper = std::make_shared<shared::TCompressionHelper>();
    m_authenticationAndEncryptionInfo = std::make_shared<authentication::TAuthenticationAndEncryptionInfo>();
    m_sequenceFlowProcessor = std::make_shared<TGameserverNetworkPacketSequenceFlowProcessor>();
    m_rawDataProcessor = std::make_shared<TGameserverNetworkPacketRawDataProcessor>(
        m_authenticationAndEncryptionInfo->xteaHelper(), m_compressionHelper);

    connect(m_dualConnection.get(), &TGameserverDualConnection::packetReceived, m_sequenceFlowProcessor.get(),
            &TGameserverNetworkPacketSequenceFlowProcessor::processPacket);
    connect(m_sequenceFlowProcessor.get(), &TGameserverNetworkPacketSequenceFlowProcessor::packetReady, this,
            &TGameserverSessionNetwork::onPacketReady);
    // Queued: the connection announces the game world right after its connected
    // signal, before anything the session does on connect can reach the wire.
    connect(
        m_dualConnection.get(), &TGameserverDualConnection::connected, this,
        [this](const QUrl&) {
            if (m_disconnected)
                return;
            m_readTimer.start();
            tibia::game::notify("onConnect");
        },
        Qt::QueuedConnection);
    connect(m_dualConnection.get(), &TGameserverDualConnection::error, this,
            [this](const QString& ErrorDescription) { fail(ErrorDescription, false); });
    connect(m_dualConnection.get(), &TGameserverDualConnection::disconnected, this, [this] { fail({}, true); });
}

void TGameserverSessionNetwork::releaseNetworkStack()
{
    m_networkTick.stop();
    if (m_dualConnection) {
        m_dualConnection->disconnect(this);
        m_dualConnection->disconnectFromGameserver();
    }
    if (m_sequenceFlowProcessor)
        m_sequenceFlowProcessor->disconnect(this);
    // Released from the event loop: this can run inside one of their signals.
    if (m_dualConnection || m_sequenceFlowProcessor) {
        QTimer::singleShot(0, [dualConnection = std::move(m_dualConnection),
                               sequenceFlowProcessor = std::move(m_sequenceFlowProcessor)] {});
    }
    m_dualConnection = nullptr;
    m_sequenceFlowProcessor = nullptr;
    m_record.reset();
}

void TGameserverSessionNetwork::connectToGameserver(const TGameserverLoginData& login)
{
    m_login = login;
    m_disconnected = false;
    m_replay = false;
    releaseNetworkStack();
    buildNetworkStack();
    if (!login.recordTo.empty()) {
        std::error_code ec;
        std::filesystem::create_directory("records", ec);
        const auto path = std::filesystem::path("records") / login.recordTo;
        m_record = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::trunc);
        m_recordStart = shared::TClockService::frameMillis();
        m_skipFirstOutput = true;
    }
    m_authenticationAndEncryptionInfo->setSessionKey(QString::fromStdString(login.sessionKey));
    m_authenticationAndEncryptionInfo->setCharacterName(QString::fromStdString(login.characterName));
    m_authenticationAndEncryptionInfo->setAssetsIdentifier(QString::fromStdString(tibia::game::assetIdentifier()));

    QUrl primaryUrl;
    primaryUrl.setScheme(QStringLiteral("tcp"));
    primaryUrl.setHost(QString::fromStdString(login.host));
    primaryUrl.setPort(login.port);
    // Crystal serves the secondary connection on the same game port; the
    // fragment keeps the two URLs apart.
    QUrl secondaryUrl = primaryUrl;
    secondaryUrl.setFragment(QStringLiteral("secondary"));
    auto urlInfo = std::make_shared<TDualConnectionUrlInfo>(primaryUrl, secondaryUrl);
    m_dualConnection->connectToGameserver(urlInfo, QString::fromStdString(tibia::game::getWorldName()));
    m_networkTick.start();
}

void TGameserverSessionNetwork::beginReplay()
{
    m_replay = true;
    m_disconnected = false;
    tibia::game::notify("onConnect");
}

void TGameserverSessionNetwork::disconnect()
{
    m_disconnected = true;
    releaseNetworkStack();
}

void TGameserverSessionNetwork::retire()
{
    disconnect();
    if (g_live == this)
        g_live = nullptr;
    deleteLater();
}

void TGameserverSessionNetwork::fail(const QString& message, const bool clean)
{
    if (m_disconnected)
        return;
    m_disconnected = true;
    releaseNetworkStack();
    tibia::game::processConnectionError(message.isEmpty() && !clean ? std::string("connection lost") : message.toStdString(),
                                        clean);
}

bool TGameserverSessionNetwork::isConnected() const
{
    if (m_replay)
        return !m_disconnected;
    return m_dualConnection && !m_disconnected && m_dualConnection->connectionsUsed() != EConnectionsUsed::None;
}

bool TGameserverSessionNetwork::isConnecting() const
{
    return !m_replay && m_dualConnection && !m_disconnected
        && m_dualConnection->connectionsUsed() == EConnectionsUsed::None;
}

qint64 TGameserverSessionNetwork::millisSinceRead() const
{
    if (!isConnected() || !m_readTimer.isValid())
        return -1;
    return m_readTimer.elapsed();
}

void TGameserverSessionNetwork::sendLogin(const uint32_t challengeTimestamp, const uint8_t challengeRandom)
{
    using namespace ::tibia::protobuf::protocol;
    if (!m_authenticationAndEncryptionInfo)
        return;
    auto& info = *m_authenticationAndEncryptionInfo;
    if (info.isSecondaryLogin()) {
        auto message = tibia::protocol::newGameclientMessage(SecondaryLoginOpcode);
        auto* secondaryLogin =
            message->MutableExtension(GameclientMessageSecondaryLogin::gameclient_message_secondary_login);
        secondaryLogin->set_challenge_connection(0);
        secondaryLogin->set_token(info.secondaryLoginToken().toStdString());
        tibia::protocol::sendGameclientMessage(std::move(message));
        return;
    }

    info.generateXteaKey();
    const QByteArray key = info.xteaKey();
    auto message = tibia::protocol::newGameclientMessage(LoginOpcode);
    auto* login = message->MutableExtension(GameclientMessageLogin::gameclient_message_login);
    login->set_client_os(tibia::game::clientOs());
    login->set_protocol_version(tibia::game::getProtocolVersion());
    login->set_client_version(tibia::game::clientRevision());
    login->set_client_version_string(std::to_string(tibia::game::clientRevision()));
    login->set_asset_identifier(info.assetsIdentifier().toStdString());
    login->set_preview_state(0);
    for (int i = 0; i + 3 < key.size(); i += 4) {
        login->add_xtea_key(uint32_t(uint8_t(key[i])) | (uint32_t(uint8_t(key[i + 1])) << 8)
                            | (uint32_t(uint8_t(key[i + 2])) << 16) | (uint32_t(uint8_t(key[i + 3])) << 24));
    }
    login->set_gamemaster(false);
    login->set_session_key(info.sessionKey().toStdString());
    login->set_character_name(info.characterName().toStdString());
    login->set_challenge_timestamp(challengeTimestamp);
    login->set_challenge_random(challengeRandom);
    // A pack that listens for extended opcodes asks for the product transport
    // in the RSA padding; the CIP client leaves the padding zero.
    if (tibia::protocol::ExtendedOpcodeReader::instance().hasListeners())
        login->set_product_transport("ExtendedOpcodes");
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void TGameserverSessionNetwork::writeClientFrame(const QByteArray& frame)
{
    sendFrame(frame);
}

void TGameserverSessionNetwork::writeProductFrame(const QByteArray& frame)
{
    sendFrame(frame);
}

void TGameserverSessionNetwork::sendFrame(const QByteArray& frame)
{
    if (frame.isEmpty())
        return;
    const auto opcode = static_cast<uint8_t>(frame.at(0));
    if (m_replay) {
        // A recorded session ends when the player logs out of it.
        if (opcode == QuitGameOpcode)
            fail({}, true);
        return;
    }
    if (!m_dualConnection || m_dualConnection->connectionsUsed() == EConnectionsUsed::None)
        return;
    recordOutput(frame);
    // CIP TProtocolClientMessageProcessor: the login messages are RSA packets,
    // every other message an XTEA packet for all connections; a connection
    // ping back answers one connection (qtclient's answers the primary one).
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameclient, frame);
    packet.setEncryptionType(opcode == LoginOpcode || opcode == SecondaryLoginOpcode ? TGameserverNetworkPacket::LoginRsa
                                                                                     : TGameserverNetworkPacket::Xtea);
    packet.setConnectionsUsed(opcode == ConnectionPingBackOpcode ? EConnectionsUsed::Primary
                                                                 : EConnectionsUsed::PrimaryAndSecondary);
    m_rawDataProcessor->encodePacket(packet);
    m_dualConnection->preparePacket(packet);
    m_dualConnection->sendPacket(packet);
}

void TGameserverSessionNetwork::onPacketReady(TGameserverNetworkPacket packet)
{
    if (m_disconnected || !m_rawDataProcessor)
        return;
    m_rawDataProcessor->decodePacket(packet);
    m_readTimer.start();
    m_serverMessageStream->appendData(packet.data());
    const QByteArray data = m_serverMessageStream->readAllData();
    m_serverMessageStream->removeReadData();
    processServerData(data);
}

void TGameserverSessionNetwork::processServerData(const QByteArray& data)
{
    if (data.isEmpty() || data.size() > 0xffff)
        return;
    recordInput(data);
    deliverPacket(reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()));
}

void TGameserverSessionNetwork::deliverPacket(const uint8_t* data, const size_t size)
{
    if (m_disconnected || !data || size == 0)
        return;
    ++m_receivedPackets;
    m_receivedBytes += int(size);
    tibia::protocol::processServerPacket(data, size);
}

void TGameserverSessionNetwork::onReadyForSecondaryConnection(const QString& token)
{
    // CIP: TLoginProtocolMessageHandler stores the token and marks the next
    // login as a secondary one; the session then allows the secondary connection.
    if (!m_authenticationAndEncryptionInfo || !m_dualConnection)
        return;
    m_authenticationAndEncryptionInfo->setSecondaryLogin();
    m_authenticationAndEncryptionInfo->setSecondaryLoginToken(token);
    m_dualConnection->allowSecondaryConnection();
}

void TGameserverSessionNetwork::recordInput(const QByteArray& data)
{
    if (!m_record || !*m_record)
        return;
    *m_record << "< " << (shared::TClockService::frameMillis() - m_recordStart) << ' ';
    *m_record << std::hex << std::setfill('0');
    for (const char byte : data)
        *m_record << std::setw(2) << int(uint8_t(byte));
    *m_record << std::dec << '\n';
}

void TGameserverSessionNetwork::recordOutput(const QByteArray& data)
{
    if (!m_record || !*m_record)
        return;
    if (m_skipFirstOutput) {
        m_skipFirstOutput = false;
        return;
    }
    *m_record << "> " << (shared::TClockService::frameMillis() - m_recordStart) << ' ';
    *m_record << std::hex << std::setfill('0');
    for (const char byte : data)
        *m_record << std::setw(2) << int(uint8_t(byte));
    *m_record << std::dec << '\n';
}

shared::TRsaHelper& loginRsaHelper()
{
    static shared::TRsaHelper helper;
    return helper;
}

void installGameServerConnection()
{
    // Crystal's login RSA modulus, big-endian; the exponent is 65537.
    static const QByteArray LoginModulus = QByteArray::fromHex(
        "9b646903b45b07ac956568d87353bd7165139dd7940703b03e6dd079399661b4"
        "a837aa60561d7ccb9452fa0080594909882ab5bca58a1a1b35f8b1059b72b121"
        "2611c6152ad3dbb3cfbee7adc142a75d3d75971509c321c5c24a5bd51fd460f0"
        "1b4e15beb0de1930528a5d3f15c1e3cbf5c401d6777e10acaab33dbe8d5b7ff5");
    loginRsaHelper().setPublicKey(LoginModulus);
}

}
