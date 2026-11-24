#include "gameservergamesession.h"

#include "authentication/authenticationandencryptioninfo.h"
#include "authentication/loginprotocolmessagehandler.h"
#include "chat/chatgameactionhandler.h"
#include "config/characterconfigurationmanager.h"
#include "container/containerciptypes.h"
#include "container/containergameactionhandler.h"
#include "creatures/creaturesgameactionhandler.h"
#include "creatures/creaturestorage.h"
#include "friends/friendsciptypes.h"
#include "game/gameciptypes.h"
#include "game/playergameactionhandler.h"
#include "game/tgamenotify.h"
#include "game/thingsession_forward.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/domainhandlers.h"
#include "input/playermovementintenthandler.h"
#include "network/dualconnectioninfo.h"
#include "network/gameserverconnection.h"
#include "network/gameserverdualconnection.h"
#include "network/gameservernetworkpacket.h"
#include "network/gameservernetworkpacketconnection.h"
#include "network/gameservernetworkpacketrawdataprocessor.h"
#include "network/gameservernetworkpacketsequenceflowprocessor.h"
#include "network/gameservertcpconnection.h"
#include "network/unencryptedrawmessagestream.h"
#include "protocol/extendedopcode.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"
#include "worldmap/worldmapgameactionhandler.h"

#include "clockservice.h"
#include "compressionhelper.h"
#include "crashhandler.h"
#include "sessiondump/clientsessiondumprecorder.h"

#include <protocol.pb.h>

#include <stdexcept>

#include <QUrl>
#include <QtGlobal>
#include <format>

#include "game/gamesessionstate.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/runtime/host.h>
#include <tibia/schedule/schedule.h>

namespace tibia::game {

using SoftHook = tibia::game::SoftHook;
using Args = TGameNotify::Args;

namespace {

/// CIP's TClientSessiondumpRecorder keep time (a constant in initialize).
constexpr int64_t SessiondumpKeepSeconds = 10;

constexpr uint8_t LoginOpcode = 10;
constexpr uint8_t SecondaryLoginOpcode = 11;
constexpr uint8_t ConnectionPingBackOpcode = 28;

/// Host: the session's network tick (CipSoft runs it with the game client's tick).
constexpr int NetworkTickMillis = 10;

std::shared_ptr<network::TGameserverNetworkPacketConnection> newGameserverNetworkPacketConnection()
{
    auto socket = std::make_shared<network::TGameserverSocket>();
    auto tcpConnection = std::make_shared<network::TGameserverTCPConnection>(socket);
    return std::make_shared<network::TGameserverNetworkPacketConnection>(tcpConnection);
}

void wireProcessLoginErrorImpl(const std::string_view error)
{
    tibia::game::notify("onLoginError", error);
}

void wireProcessLoginAdviceImpl(const std::string_view message)
{
    tibia::game::notify("onLoginAdvice", message);
}

void wireProcessLoginWaitImpl(const std::string_view message, const uint8_t time)
{
    tibia::game::notify("onLoginWait", message, time);
}

} // namespace

void TGameserverGameSession::wireProcessLoginError(const std::string_view error)
{
    wireProcessLoginErrorImpl(error);
}

void TGameserverGameSession::wireProcessLoginAdvice(const std::string_view message)
{
    wireProcessLoginAdviceImpl(message);
}

void TGameserverGameSession::wireProcessLoginWait(const std::string_view message, const uint8_t time)
{
    wireProcessLoginWaitImpl(message, time);
}

void TGameserverGameSession::wireProcessWorldEntered()
{
    TGameSessionState& hub = gameSessionState();
    hub.processEnterGame();
    if (hub.m_gameConnection && !hub.m_gameConnection->gameInitialized()) {
        hub.processGameStart();
        hub.m_gameConnection->setGameInitialized(true);
    }
}

void TGameserverGameSession::wireProcessDeath()
{
    TGameSessionState& hub = gameSessionState();
    hub.m_dead = true;
    if (hub.m_playerPresent)
        tibia::game::localPlayerStopWalk();
    setSceneFrozen(true);
}

void TGameserverGameSession::wireProcessPing()
{
    tibia::game::notify("onPing");
    if (TGameSessionState& hub = gameSessionState(); hub.m_gameConnection)
        tibia::protocol::gameclient::sendPingBack();
}

void TGameserverGameSession::wireProcessPingBack()
{
    TGameSessionState& hub = gameSessionState();
    ++hub.m_pingReceived;

    if (hub.m_pingReceived == hub.m_pingSent) {
        const int64_t oldPing = hub.m_ping;
        hub.m_ping = hub.m_pingTimer.elapsed();
        if (oldPing != hub.m_ping)
            tibia::game::notify("onPingBack", hub.m_ping);
    }

    const int pingDelay = hub.m_pingDelay;
    hub.m_pingEvent = tibia::schedule::after([] { gameSessionState().ping(); }, pingDelay);
}

namespace {

template<typename F>
SoftHook gameHook(const char* field, F&& f)
{
    return SoftHook(field, [f = std::forward<F>(f)](const Args& args) {
        f(args);
        return std::any{};
    });
}

}

void hubDisconnectNow()
{
    if (tibia::game::isOnline())
        gameSessionState().forceLogout();
    else
        gameSessionState().cancelLogin();
}

TGameserverGameSession::TGameserverGameSession(QObject* parent)
    : TGameSessionBase(parent)
    , m_authenticationAndEncryptionInfo(std::make_shared<authentication::TAuthenticationAndEncryptionInfo>())
{
    m_networkTick.setInterval(NetworkTickMillis);
    connect(&m_networkTick, &QTimer::timeout, this, &TGameserverGameSession::processNetwork);
    // CIP typed lifecycle (TGameNotify) — prefer over string SoftHook for end/fail.
    // The base announces worldEntered for every WorldEntered message.
    connect(TGameNotify::instance(), &TGameNotify::gameEnded, this, [this] {
        // Same-socket alt switch ends the session in-process. The socket stays
        // open, so this must not look like a logout (that reopens CharacterSelection).
        if (isSwitchingCharacter())
            return;
        Q_EMIT disconnected();
    });
    connect(TGameNotify::instance(), &TGameNotify::connectionFailing, this, [this](bool failing) {
        if (failing)
            Q_EMIT pingTimeout();
        else
            Q_EMIT pingTimeoutRecovered();
    });
    m_hooks.push_back(gameHook("onUpdateNeeded", [this](const Args&) { Q_EMIT clientOutOfDate(); }));
}

TGameserverGameSession::~TGameserverGameSession()
{
    m_networkTick.stop();
    m_connectionManager.disconnectAll();
    m_clientMessageConnections.disconnectAll();
    if (const auto sessionNetwork = m_sessionNetwork.lock()) {
        sessionNetwork->setFrameWriter({});
        sessionNetwork->setDisconnector({});
    }
    if (m_dualConnection) {
        m_dualConnection->disconnect(this);
        m_dualConnection->disconnectFromGameserver();
    }
    // Released from the event loop: the session can end inside one of their signals.
    QTimer::singleShot(0, [dualConnection = std::move(m_dualConnection),
                           sequenceFlowProcessor = std::move(m_sequenceFlowProcessor),
                           loginProtocolMessageHandler = std::move(m_loginProtocolMessageHandler)] {});
}

void TGameserverGameSession::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    TGameSessionBase::initialize(metaInfo);
    const auto gameserverMetaInfo = std::dynamic_pointer_cast<const TGameserverGameSessionMetaInfo>(metaInfo);
    if (!gameserverMetaInfo)
        throw std::invalid_argument("wrong type of TGameSessionMetaInfo submitted");
    createStorages();
    createProtocolMessageHandlers();
    createRenderProviders();
    createSoundProviders();
    createGameActionProvidersAndHandlers();
    createNetworkStack();
    m_loginData = gameserverMetaInfo->loginData;
    // CIP: the session records its last server messages (kept 10 to 20 seconds)
    // and hands them to the crash handler as the report's session dump.
    m_sessiondumpRecorder = std::make_shared<sessiondump::TClientSessiondumpRecorder>(SessiondumpKeepSeconds);
    m_sessiondumpRecorder->start(m_loginData.characterName, m_loginData.worldName, m_loginData.sessionKey);
    // Host: CipSoft's provider captures the session; a weak pointer keeps a crash
    // after this session ended from reading a destroyed recorder.
    shared::TCrashHandler::instance().setSessiondumpProvider(
        [recorder = std::weak_ptr<sessiondump::TClientSessiondumpRecorder>(m_sessiondumpRecorder)] {
            const auto live = recorder.lock();
            return live ? live->sessiondump() : QByteArray();
        });
    connectProtocolMessageHandlers();
    connectGameActionHandlers();
}

void TGameserverGameSession::createProtocolMessageHandlers()
{
    TGameSessionBase::createProtocolMessageHandlers();
    // Host: qtclient has no Epic Games Store distribution.
    m_loginProtocolMessageHandler =
        std::make_shared<authentication::TLoginProtocolMessageHandler>(m_authenticationAndEncryptionInfo, false);
    using authentication::TLoginProtocolMessageHandler;
    TLoginProtocolMessageHandler* login = m_loginProtocolMessageHandler.get();
    // The login's outcome is the session's.
    connect(login, &TLoginProtocolMessageHandler::gameLoginErrorMessage, this, &TGameserverGameSession::gameLoginErrorMessage);
    connect(login, &TLoginProtocolMessageHandler::gameLoginAdviceMessage, this, &TGameserverGameSession::gameLoginAdviceMessage);
    connect(login, &TLoginProtocolMessageHandler::gameLoginWaitMessage, this, &TGameserverGameSession::gameLoginWaitMessage);
    connect(login, &TLoginProtocolMessageHandler::gameLoginChallengeMessage, this,
            &TGameserverGameSession::gameLoginChallengeMessage);
    connect(login, &TLoginProtocolMessageHandler::gameReadyForSecondaryConnection, this,
            &TGameserverGameSession::onGameReadyForSecondaryConnection);
}

void TGameserverGameSession::createNetworkStack()
{
    using network::TGameserverDualConnection;
    using network::TGameserverNetworkPacketSequenceFlowProcessor;
    auto primaryConnection = newGameserverNetworkPacketConnection();
    auto secondaryConnection = newGameserverNetworkPacketConnection();
    auto dualConnectionInfo = std::make_shared<network::TDualConnectionInfo>(primaryConnection, secondaryConnection);
    m_dualConnection = std::make_shared<TGameserverDualConnection>(dualConnectionInfo, clockService());
    m_serverMessageStream = std::make_shared<network::TUnencryptedRawMessageStream>();
    m_compressionHelper = std::make_shared<shared::TCompressionHelper>();
    m_sequenceFlowProcessor = std::make_shared<TGameserverNetworkPacketSequenceFlowProcessor>();
    m_rawDataProcessor = std::make_shared<network::TGameserverNetworkPacketRawDataProcessor>(
        m_authenticationAndEncryptionInfo->xteaHelper(), m_compressionHelper);

    protocol::TProtocolMessageQueue* queue = protocolMessageQueue().get();
    TGameserverDualConnection* dualConnection = m_dualConnection.get();
    TGameserverNetworkPacketSequenceFlowProcessor* sequenceFlowProcessor = m_sequenceFlowProcessor.get();
    // Packets of both connections are put in order, then decoded and parsed.
    connect(dualConnection, &TGameserverDualConnection::packetReceived, sequenceFlowProcessor,
            &TGameserverNetworkPacketSequenceFlowProcessor::processPacket);
    connect(dualConnection, &TGameserverDualConnection::connectionsUsedChanged, sequenceFlowProcessor,
            &TGameserverNetworkPacketSequenceFlowProcessor::onConnectionsUsedChanged);
    connect(queue, &protocol::TProtocolMessageQueue::receivedMessage, sequenceFlowProcessor,
            &TGameserverNetworkPacketSequenceFlowProcessor::handleAnyMessage);
    m_connectionManager.add(connect(sequenceFlowProcessor, &TGameserverNetworkPacketSequenceFlowProcessor::packetReady, this,
                                    &TGameserverGameSession::onPacketReady));
    // The queue's encoded client messages become packets for the connections.
    // Host: they pass the classic session state's view of the connection, which
    // records them and drops them in a replay.
    m_connectionManager.add(connect(queue, &protocol::TProtocolMessageQueue::clientMessageEnqueued, this,
                                    [this](int, const QByteArray& frame) {
                                        if (const auto sessionNetwork = m_sessionNetwork.lock())
                                            sessionNetwork->writeClientFrame(frame);
                                    }));
    connect(dualConnection, &TGameserverDualConnection::error, this, [this](const QString& ErrorDescription) {
        Q_EMIT connectionError(ErrorDescription);
        onConnectionLost(ErrorDescription, false);
    });
    // Queued (host): the connection announces the game world right after its
    // connected signal, before anything the session does on connect can reach the wire.
    connect(dualConnection, &TGameserverDualConnection::connected, this, [this](const QUrl&) { Q_EMIT connected(); },
            Qt::QueuedConnection);
    // Host: the classic session state ends the game when the server closes the
    // connection (its game end is the session's disconnected).
    connect(dualConnection, &TGameserverDualConnection::disconnected, this, [this] { onConnectionLost({}, true); });
    connect(dualConnection, &TGameserverDualConnection::connectionsUsedChanged, this,
            [this](const network::EConnectionsUsed ConnectionsUsed) {
                if (const auto sessionNetwork = m_sessionNetwork.lock())
                    sessionNetwork->setConnected(ConnectionsUsed != network::EConnectionsUsed::None);
            });
}

void TGameserverGameSession::connectProtocolMessageHandlers()
{
    using Queue = protocol::TProtocolMessageQueue;
    Queue* queue = protocolMessageQueue().get();
    // The game window relinks the queue: the links are made again.
    m_clientMessageConnections.disconnectAll();
    const auto link = [this](const QMetaObject::Connection& connection) { m_clientMessageConnections.add(connection); };

    link(connect(queue, &Queue::receivedMessage, m_sessiondumpRecorder.get(),
                 &sessiondump::TClientSessiondumpRecorder::handleAnyMessage));
    TGameSessionBase::connectProtocolMessageHandlers();

    using authentication::TLoginProtocolMessageHandler;
    TLoginProtocolMessageHandler* login = m_loginProtocolMessageHandler.get();
    link(connect(queue, &Queue::receivedMessage, login, &TLoginProtocolMessageHandler::handleAnyMessage));
    link(connect(queue, &Queue::receivedLoginErrorMessage, login, &TLoginProtocolMessageHandler::handleLoginErrorMessage));
    link(connect(queue, &Queue::receivedLoginWaitMessage, login, &TLoginProtocolMessageHandler::handleLoginWaitMessage));
    link(connect(queue, &Queue::receivedLoginAdviceMessage, login, &TLoginProtocolMessageHandler::handleLoginAdviceMessage));
    link(connect(queue, &Queue::receivedLoginSuccessMessage, login, &TLoginProtocolMessageHandler::handleLoginSuccessMessage));
    link(connect(queue, &Queue::receivedReadyForSecondaryConnectionMessage, login,
                 &TLoginProtocolMessageHandler::handleReadyForSecondaryConnectionMessage));
    link(connect(login, &TLoginProtocolMessageHandler::sendLoginMessage, queue, &Queue::sendLogin));
    link(connect(login, &TLoginProtocolMessageHandler::sendSecondaryLoginMessage, queue, &Queue::sendSecondaryLogin));

    TGameProtocolMessageHandler* game = gameProtocolMessageHandler().get();
    link(connect(game, &TGameProtocolMessageHandler::sendEnterWorldMessage, queue, &Queue::sendEnterWorld));
    link(connect(game, &TGameProtocolMessageHandler::sendClientDetails, queue, &Queue::sendClientDetails));

    using container::TContainerProtocolMessageHandler;
    TContainerProtocolMessageHandler* containers = containerProtocolMessageHandler().get();
    link(connect(containers, &TContainerProtocolMessageHandler::sendCloseContainerMessage, queue, &Queue::sendCloseContainer));
    link(connect(containers, &TContainerProtocolMessageHandler::sendUpContainerMessage, queue, &Queue::sendUpContainer));
    link(connect(containers, &TContainerProtocolMessageHandler::sendSeekInContainerMessage, queue, &Queue::sendSeekInContainer));
    link(connect(containers, &TContainerProtocolMessageHandler::sendGetObjectInfoMessage, queue, &Queue::sendGetObjectInfo));
    link(connect(containers, &TContainerProtocolMessageHandler::sendContainerActionMessage, queue,
                 &Queue::sendContainerActionMessage));

    using creatures::TCreaturesGameActionHandler;
    namespace pb = ::tibia::protobuf::protocol;
    TCreaturesGameActionHandler* creaturesHandler = creaturesGameActionHandler().get();
    // Host: the session state's attack and follow ids follow the creature
    // storage's targets (providers, the world map and packs read them), and
    // packs get onAttack for each attack request.
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendAttack, queue, [this, queue](const pb::GameclientMessageAttack& message) {
        gameSessionState().setAttackingCreatureId(message.creature_id());
        gameSessionState().setFollowingCreatureId(creatureStorage()->followID());
        queue->sendAttack(message);
        tibia::game::notify("onAttack", static_cast<uint32_t>(message.creature_id()));
    }));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendFollow, queue, [this, queue](const pb::GameclientMessageFollow& message) {
        gameSessionState().setFollowingCreatureId(message.creature_id());
        gameSessionState().setAttackingCreatureId(creatureStorage()->attackID());
        queue->sendFollow(message);
    }));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendLookAtCreature, queue, &Queue::sendLookAtCreature));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendInspectPlayer, queue, &Queue::sendInspectPlayer));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendInviteToParty, queue, &Queue::sendInviteToParty));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendJoinParty, queue, &Queue::sendJoinParty));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendRevokeInvitation, queue, &Queue::sendRevokeInvitation));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendPassLeadership, queue, &Queue::sendPassLeadership));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendLeaveParty, queue, &Queue::sendLeaveParty));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendShareExperience, queue, &Queue::sendShareExperience));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendJoinAggression, queue, &Queue::sendJoinAggression));
    link(connect(creaturesHandler, &TCreaturesGameActionHandler::sendGreet, queue, &Queue::sendGreet));

    TPlayerGameActionHandler* player = playerGameActionHandler().get();
    link(connect(player, &TPlayerGameActionHandler::sendMount, queue, &Queue::sendMount));
    link(connect(player, &TPlayerGameActionHandler::sendGetOutfit, queue, &Queue::sendGetOutfit));
    link(connect(player, &TPlayerGameActionHandler::sendSetOutfit, queue, &Queue::sendSetOutfit));
    link(connect(player, &TPlayerGameActionHandler::sendSetCreaturePodiumConfiguration, queue,
                 &Queue::sendSetCreaturePodiumConfiguration));
    link(connect(player, &TPlayerGameActionHandler::sendInspectPlayer, queue, &Queue::sendInspectPlayer));
    link(connect(player, &TPlayerGameActionHandler::sendSetHirelingName, queue, &Queue::sendSetHirelingName));

    using container::TContainerGameActionHandler;
    TContainerGameActionHandler* containerActions = containerGameActionHandler().get();
    link(connect(containerActions, &TContainerGameActionHandler::sendMoveObject, queue, &Queue::sendMoveObject));
    link(connect(containerActions, &TContainerGameActionHandler::sendEquipObject, queue, &Queue::sendEquipObject));
    link(connect(containerActions, &TContainerGameActionHandler::sendStashAction, queue, &Queue::sendStashAction));
    link(connect(containerActions, &TContainerGameActionHandler::sendOpenDepotSearch, queue, &Queue::sendOpenDepotSearch));
    link(connect(containerActions, &TContainerGameActionHandler::sendCloseDepotSearch, queue, &Queue::sendCloseDepotSearch));
    link(connect(containerActions, &TContainerGameActionHandler::sendDepotSearchType, queue, &Queue::sendDepotSearchType));
    link(connect(containerActions, &TContainerGameActionHandler::sendOpenParentContainer, queue, &Queue::sendOpenParentContainer));
    link(connect(containerActions, &TContainerGameActionHandler::sendDepotSearchRetrieve, queue, &Queue::sendDepotSearchRetrieve));

    link(connect(worldMapGameActionHandler().get(), &worldmap::TWorldMapGameActionHandler::sendMoveObject, queue,
                 &Queue::sendMoveObject));
    using input::TUseWithGameActionHandler;
    TUseWithGameActionHandler* useWith = useWithGameActionHandler().get();
    link(connect(useWith, &TUseWithGameActionHandler::sendUseTwoObjects, queue, &Queue::sendUseTwoObjects));
    link(connect(useWith, &TUseWithGameActionHandler::sendUseOnCreature, queue, &Queue::sendUseOnCreature));
    link(connect(player, &TPlayerGameActionHandler::sendSetTactics, queue, &Queue::sendSetTactics));

    using chat::TChatGameActionHandler;
    TChatGameActionHandler* chat = chatGameActionHandler().get();
    link(connect(chat, &TChatGameActionHandler::sendGetChanneList, queue, &Queue::sendGetChanneList));
    link(connect(chat, &TChatGameActionHandler::sendJoinChannel, queue, &Queue::sendJoinChannel));
    link(connect(chat, &TChatGameActionHandler::sendOpenChannel, queue, &Queue::sendOpenChannel));
    link(connect(chat, &TChatGameActionHandler::sendPrivateChannel, queue, &Queue::sendPrivateChannel));
    link(connect(chat, &TChatGameActionHandler::sendCloseNPCChannel, queue, &Queue::sendCloseNPCChannel));
    link(connect(chat, &TChatGameActionHandler::sendLeaveChannel, queue, &Queue::sendLeaveChannel));
    link(connect(chat, &TChatGameActionHandler::sendInviteToChannel, queue, &Queue::sendInviteToChannel));
    link(connect(chat, &TChatGameActionHandler::sendExcludeFromChannel, queue, &Queue::sendExcludeFromChannel));
    link(connect(chat, &TChatGameActionHandler::sendGuildMessage, queue, &Queue::sendGuildMessage));
    link(connect(chat, &TChatGameActionHandler::sendTalkMessage, queue, &Queue::sendTalkMessage));
    link(connect(crossHairSpellGameActionHandler().get(), &input::TCrossHairSpellGameActionHandler::sendTalkMessage, queue,
                 &Queue::sendTalkMessage));

    link(connect(friendsProtocolMessageHandler().get(), &friends::TFriendsProtocolMessageHandler::sendFriendSystemAction, queue,
                 &Queue::sendFriendSystemAction));
    link(connect(gatherAllProtocolMessageHandler().get(), &input::TGatherAllProtocolMessageHandler::sendGatherAll, queue,
                 &Queue::sendGatherAll));
    link(connect(bossDifficultySelectionProtocolMessageHandler(),
                 &TBossDifficultySelectionProtocolMessageHandler::sendDifficultySelectionDataChanged, queue,
                 &Queue::sendBossDifficultySelectionChangedMessage));
}

void TGameserverGameSession::startSession()
{
}

void TGameserverGameSession::connectToGameserver()
{
    // CIP hands the login data to its game server login process controller.
    // Host: the controller's steps run here: the classic session state starts
    // the login, the authentication info gets the login's keys, and the dual
    // connection connects to the world's game server.
    const GameserverCredentials& c = m_loginData;
    const auto sessionNetwork = network::TGameserverSessionNetwork::create();
    sessionNetwork->setFrameWriter([this](const QByteArray& frame) { sendClientFrame(frame); });
    sessionNetwork->setDisconnector([this] { closeConnection(); });
    try {
        gameSessionState().loginWorld(c.worldName.toStdString(), c.characterName.toStdString(), sessionNetwork);
    } catch (const std::exception& e) {
        sessionNetwork->setFrameWriter({});
        sessionNetwork->setDisconnector({});
        Q_EMIT connectionError(QString::fromUtf8(e.what()));
        return;
    }
    m_sessionNetwork = sessionNetwork;
    sessionNetwork->startRecording(c.recordTo.toStdString());
    m_authenticationAndEncryptionInfo->setSessionKey(c.sessionKey);
    m_authenticationAndEncryptionInfo->setCharacterName(c.characterName);
    m_authenticationAndEncryptionInfo->setAssetsIdentifier(QString::fromStdString(tibia::game::assetIdentifier()));
    // A pack that listens for extended opcodes asks for the product transport
    // in the login's RSA padding; the CIP client leaves the padding zero.
    if (tibia::protocol::ExtendedOpcodeReader::instance().hasListeners())
        m_loginProtocolMessageHandler->setProductTransport("ExtendedOpcodes");

    QUrl primaryUrl;
    primaryUrl.setScheme(QStringLiteral("tcp"));
    primaryUrl.setHost(c.host);
    primaryUrl.setPort(c.port);
    // Crystal serves the secondary connection on the same game port; the
    // fragment keeps the two URLs apart.
    QUrl secondaryUrl = primaryUrl;
    secondaryUrl.setFragment(QStringLiteral("secondary"));
    auto urlInfo = std::make_shared<network::TDualConnectionUrlInfo>(primaryUrl, secondaryUrl);
    m_dualConnection->connectToGameserver(urlInfo, c.worldName);
    m_networkTick.start();
}

void TGameserverGameSession::requestLogout()
{
    safeLogout();
}

void TGameserverGameSession::disconnectNow()
{
    // Host: the classic session state logs out (when online) and lets go of the connection.
    hubDisconnectNow();
    if (const std::shared_ptr<config::TCharacterConfigurationManager>& manager = characterConfigurationManager())
        manager->saveConfiguration();
    closeConnection();
    m_connectionManager.disconnectAll();
}

std::shared_ptr<shared::IClockService> TGameserverGameSession::clockService() const
{
    return shared::TClockService::process();
}

void TGameserverGameSession::processNetwork()
{
    if (!m_dualConnection)
        return;
    // The dual connection may end the session's connection from inside the tick.
    const auto dualConnection = m_dualConnection;
    dualConnection->processAvailablePackets();
    dualConnection->checkForStateChange();
}

network::EConnectionsUsed TGameserverGameSession::connectionsUsed() const
{
    return m_dualConnection ? m_dualConnection->connectionsUsed() : network::EConnectionsUsed::None;
}

void TGameserverGameSession::onGameReadyForSecondaryConnection()
{
    m_dualConnection->allowSecondaryConnection();
}

void TGameserverGameSession::onPacketReady(network::TGameserverNetworkPacket packet)
{
    const auto sessionNetwork = m_sessionNetwork.lock();
    if (!sessionNetwork || !m_rawDataProcessor)
        return;
    m_rawDataProcessor->decodePacket(packet);
    sessionNetwork->notePacketRead();
    tibia::protocol::setServerPacketOrigin(packet.sequenceNumber(), packet.connectionIndex());
    m_serverMessageStream->appendData(packet.data());
    const QByteArray data = m_serverMessageStream->readAllData();
    m_serverMessageStream->removeReadData();
    sessionNetwork->processServerData(data);
}

void TGameserverGameSession::sendClientFrame(const QByteArray& frame)
{
    if (frame.isEmpty() || !m_dualConnection || m_dualConnection->connectionsUsed() == network::EConnectionsUsed::None)
        return;
    using network::TGameserverNetworkPacket;
    const auto opcode = static_cast<uint8_t>(frame.at(0));
    // CIP TProtocolClientMessageProcessor: the login messages are RSA packets,
    // every other message an XTEA packet for all connections; a connection
    // ping back answers one connection (qtclient's answers the primary one).
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameclient, frame);
    packet.setEncryptionType(opcode == LoginOpcode || opcode == SecondaryLoginOpcode ? TGameserverNetworkPacket::LoginRsa
                                                                                     : TGameserverNetworkPacket::Xtea);
    packet.setConnectionsUsed(opcode == ConnectionPingBackOpcode ? network::EConnectionsUsed::Primary
                                                                 : network::EConnectionsUsed::PrimaryAndSecondary);
    m_rawDataProcessor->encodePacket(packet);
    m_dualConnection->preparePacket(packet);
    m_dualConnection->sendPacket(packet);
}

void TGameserverGameSession::closeConnection()
{
    m_networkTick.stop();
    if (m_dualConnection)
        m_dualConnection->disconnectFromGameserver();
}

void TGameserverGameSession::onConnectionLost(const QString& message, const bool clean)
{
    // The classic session state's view fails (it closes the connection through
    // its disconnector); without it the connection just closes.
    if (const auto sessionNetwork = m_sessionNetwork.lock())
        sessionNetwork->fail(message, clean);
    else
        closeConnection();
}

bool TGameserverGameSession::isOnline() const
{
    return tibia::game::isOnline();
}

QString TGameserverGameSession::characterName() const
{
    return QString::fromStdString(tibia::game::characterName());
}

QString TGameserverGameSession::worldName() const
{
    return QString::fromStdString(getWorldName());
}

void TGameserverGameSession::connectPlayerMovementIntentHandler(input::TPlayerMovementIntentHandler* Handler)
{
    using H = TPlayerProtocolMessageHandler;
    using I = input::TPlayerMovementIntentHandler;
    H* player = playerProtocolMessageHandler().get();
    connect(Handler, &I::sendGoNorth, player, &H::goNorth);
    connect(Handler, &I::sendGoEast, player, &H::goEast);
    connect(Handler, &I::sendGoSouth, player, &H::goSouth);
    connect(Handler, &I::sendGoWest, player, &H::goWest);
    connect(Handler, &I::sendGoNorthEast, player, &H::goNorthEast);
    connect(Handler, &I::sendGoSouthEast, player, &H::goSouthEast);
    connect(Handler, &I::sendGoSouthWest, player, &H::goSouthWest);
    connect(Handler, &I::sendGoNorthWest, player, &H::goNorthWest);
    connect(Handler, &I::sendStop, player, &H::stop);
    connect(Handler, &I::sendCancel, player, &H::cancel);
    connect(Handler, &I::sendGoPath, player, &H::goPath);
    connect(Handler, &I::sendRotateNorth, player, &H::rotateNorth);
    connect(Handler, &I::sendRotateEast, player, &H::rotateEast);
    connect(Handler, &I::sendRotateSouth, player, &H::rotateSouth);
    connect(Handler, &I::sendRotateWest, player, &H::rotateWest);
    player->setPlayerMovementIntentHandler(Handler);
}

}
