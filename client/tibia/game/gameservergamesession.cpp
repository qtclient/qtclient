#include "gameservergamesession.h"

#include "protocol/clientmessages.h"
#include "game/gameciptypes.h"
#include "game/tgamenotify.h"
#include "input/playermovementintenthandler.h"
#include "network/gameserverpacketprocessor.h"
#include "protocol/protocolmessagequeue.h"

#include "clockservice.h"
#include "crashhandler.h"
#include "sessiondump/clientsessiondumprecorder.h"

#include <stdexcept>

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
{
    // The game connection publishes onConnect once the socket is open.
    m_hooks.emplace_back("onConnect", [this](const Args&) {
        Q_EMIT connected();
        return std::any{};
    });
    // CIP typed lifecycle (TGameNotify) — prefer over string SoftHook for start/end/fail.
    connect(TGameNotify::instance(), &TGameNotify::gameStarted, this, [this] { Q_EMIT worldEntered(); });
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
    m_hooks.push_back(gameHook("onLoginError", [this](const Args& a) { Q_EMIT gameLoginErrorMessage(argString(a, 0), 0); }));
    m_hooks.push_back(gameHook("onLoginAdvice", [this](const Args& a) { Q_EMIT gameLoginAdviceMessage(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onLoginWait", [this](const Args& a) {
        Q_EMIT gameLoginWaitMessage(argString(a, 0), static_cast<int>(argInt(a, 1)));
    }));
    m_hooks.push_back(gameHook("onConnectionError", [this](const Args& a) { Q_EMIT connectionError(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onUpdateNeeded", [this](const Args&) { Q_EMIT clientOutOfDate(); }));
}

TGameserverGameSession::~TGameserverGameSession() = default;

void TGameserverGameSession::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    TGameSessionBase::initialize(metaInfo);
    const auto gameserverMetaInfo = std::dynamic_pointer_cast<const TGameserverGameSessionMetaInfo>(metaInfo);
    if (!gameserverMetaInfo)
        throw std::invalid_argument("wrong type of TGameSessionMetaInfo submitted");
    createStorages();
    createProtocolMessageHandlers();
    // The game handler's client messages go out through the session's queue.
    connect(gameProtocolMessageHandler().get(), &TGameProtocolMessageHandler::sendEnterWorldMessage,
            protocolMessageQueue().get(), &protocol::TProtocolMessageQueue::sendEnterWorld);
    connect(gameProtocolMessageHandler().get(), &TGameProtocolMessageHandler::sendClientDetails,
            protocolMessageQueue().get(), &protocol::TProtocolMessageQueue::sendClientDetails);
    createRenderProviders();
    createSoundProviders();
    createGameActionProvidersAndHandlers();
    createGameActionHandlers();
    // The session's network stack: its encoded client messages go to the game connection.
    connect(protocolMessageQueue().get(), &protocol::TProtocolMessageQueue::clientMessageEnqueued, this,
            [](int, const QByteArray& frame) {
                if (frame.isEmpty())
                    return;
                if (auto* processor = network::TGameserverNetworkPacketProcessor::live())
                    processor->writeClientFrame(frame);
            });
    // The server answered with its login challenge: the connection is up (the
    // disconnect reaction tells a lost connection from a failed connect by it).
    connect(protocolMessageQueue().get(), &protocol::TProtocolMessageQueue::receivedLoginChallengeMessage, this,
            [this] { Q_EMIT gameLoginChallengeMessage(); });
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
    connect(protocolMessageQueue().get(), &protocol::TProtocolMessageQueue::receivedMessage,
            m_sessiondumpRecorder.get(), &sessiondump::TClientSessiondumpRecorder::handleAnyMessage);
}

void TGameserverGameSession::startSession()
{
}

void TGameserverGameSession::connectToGameserver()
{
    const GameserverCredentials& c = m_loginData;
    // Remaining hub call: loginWorld.
    try {
        gameSessionState().loginWorld(c.account.toStdString(), c.password.toStdString(), c.worldName.toStdString(),
                          c.host.toStdString(), c.port, c.characterName.toStdString(),
                          c.authenticatorToken.toStdString(), c.sessionKey.toStdString(), c.recordTo.toStdString());
    } catch (const std::exception& e) {
        Q_EMIT connectionError(QString::fromUtf8(e.what()));
    }
}

void TGameserverGameSession::requestLogout()
{
    safeLogout();
}

void TGameserverGameSession::disconnectNow()
{
    hubDisconnectNow();
}

std::shared_ptr<shared::IClockService> TGameserverGameSession::clockService() const
{
    return shared::TClockService::process();
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
