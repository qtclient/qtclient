#include "gameservergamesession.h"

#include "protocol/clientmessages.h"
#include "game/gameciptypes.h"
#include "game/tgamenotify.h"
#include "network/gameserverpacketprocessor.h"
#include "protocol/protocolmessagequeue.h"

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

void wireProcessLoginImpl()
{
    tibia::game::notify("onLogin");
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

void wireProcessSessionEndImpl(const uint8_t reason)
{
    tibia::game::notify("onSessionEnd", reason);
}

} // namespace

void TGameserverGameSession::wireProcessLogin()
{
    wireProcessLoginImpl();
}

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

void TGameserverGameSession::wireProcessSessionEnd(const uint8_t reason)
{
    wireProcessSessionEndImpl(reason);
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

void TGameserverGameSession::wireProcessDeath(const uint8_t deathType, const uint8_t penality)
{
    TGameSessionState& hub = gameSessionState();
    hub.m_dead = true;
    if (hub.m_playerPresent)
        tibia::game::localPlayerStopWalk();
    setSceneFrozen(true);

    tibia::game::notify("onDeath", deathType, penality);
    tibia::game::TGameNotify::instance()->emitDeath(deathType, penality);
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
    m_hooks.push_back(gameHook("onLogin", [this](const Args&) { Q_EMIT gameLoginSuccessful(); }));
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
    m_hooks.push_back(gameHook("onSessionEnd", [this](const Args& a) { Q_EMIT sessionEndInformation(static_cast<int>(argInt(a, 0))); }));
    connect(TGameNotify::instance(), &TGameNotify::death, this, [this](int, int) { Q_EMIT deadMessage(); });
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
    // The session's network stack: its encoded client messages go to the game connection.
    connect(protocolMessageQueue().get(), &protocol::TProtocolMessageQueue::clientMessageEnqueued, this,
            [](int, const QByteArray& frame) {
                if (frame.isEmpty())
                    return;
                if (auto* processor = network::TGameserverNetworkPacketProcessor::live())
                    processor->writeClientFrame(frame);
            });
    m_loginData = gameserverMetaInfo->loginData;
    connectProtocolMessageHandlers();
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

}
