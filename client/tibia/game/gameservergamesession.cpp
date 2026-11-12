#include "gameservergamesession.h"

#include "client/protocolgame.h"
#include "game/gameciptypes.h"
#include "game/tgamenotify.h"

#include <framework/core/logger.h>

#include "gamehub.h"
#include "gamehub_impl.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/localplayersessionthing.h>
#include <tibia/game/gamesession.h>
#include <tibia/runtime/host.h>
#include <tibia/schedule/schedule.h>

namespace tibia::game {

using SoftHook = tibia::game::SoftHook;
using Args = TGameNotify::Args;

namespace {

TGameserverGameSession* g_liveGameserverSession = nullptr;

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

TGameserverGameSession* TGameserverGameSession::liveInstance()
{
    return g_liveGameserverSession;
}

void TGameserverGameSession::setLiveInstance(TGameserverGameSession* const session)
{
    g_liveGameserverSession = session;
}

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
    Game& hub = qtclientGameHub();
    hub.processEnterGame();
    if (hub.m_protocolGame && !hub.m_protocolGame->m_gameInitialized) {
        hub.processGameStart();
        hub.m_protocolGame->m_gameInitialized = true;
    }
}

void TGameserverGameSession::wireProcessDeath(const uint8_t deathType, const uint8_t penality)
{
    Game& hub = qtclientGameHub();
    hub.m_dead = true;
    if (hub.m_localPlayer)
        hub.m_localPlayer->stopWalk();
    setSceneFrozen(true);

    tibia::game::notify("onDeath", deathType, penality);
    tibia::game::TGameNotify::instance()->emitDeath(deathType, penality);
}

void TGameserverGameSession::wireProcessPing()
{
    tibia::game::notify("onPing");
    if (Game& hub = qtclientGameHub(); hub.m_protocolGame)
        hub.m_protocolGame->sendPingBack();
}

void TGameserverGameSession::wireProcessPingBack()
{
    Game& hub = qtclientGameHub();
    ++hub.m_pingReceived;

    if (hub.m_pingReceived == hub.m_pingSent) {
        const ticks_t oldPing = hub.m_ping;
        hub.m_ping = hub.m_pingTimer.elapsed_millis();
        if (oldPing != hub.m_ping)
            tibia::game::notify("onPingBack", hub.m_ping);
    } else
        g_logger.error("Got an invalid ping from server");

    const int pingDelay = hub.m_pingDelay;
    hub.m_pingEvent = tibia::schedule::after([] { qtclientGameHub().ping(); }, pingDelay);
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
        qtclientGameHub().forceLogout();
    else
        qtclientGameHub().cancelLogin();
}

TGameserverGameSession::TGameserverGameSession(QObject* parent)
    : cip::tibia::game::TGameserverGameSession(parent)
{
    game::ownProductSurface(this);
    // Protocol::onConnect → SoftHooks (only ProtocolGame publishes).
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
    m_hooks.push_back(gameHook("onLoginError", [this](const Args& a) { Q_EMIT gameLoginErrorMessage(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onLoginAdvice", [this](const Args& a) { Q_EMIT gameLoginAdviceMessage(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onLoginWait", [this](const Args& a) {
        Q_EMIT gameLoginWaitMessage(argString(a, 0), static_cast<int>(argInt(a, 1)));
    }));
    m_hooks.push_back(gameHook("onConnectionError", [this](const Args& a) { Q_EMIT connectionError(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onSessionEnd", [this](const Args& a) { Q_EMIT sessionEndInformation(static_cast<int>(argInt(a, 0))); }));
    connect(TGameNotify::instance(), &TGameNotify::death, this, [this](int, int) { Q_EMIT deadMessage(); });
    m_hooks.push_back(gameHook("onUpdateNeeded", [this](const Args&) { Q_EMIT clientOutOfDate(); }));
}

TGameserverGameSession::~TGameserverGameSession()
{
    if (g_liveGameserverSession == this)
        g_liveGameserverSession = nullptr;
}

void TGameserverGameSession::connectToGameserver(const GameserverCredentials& c)
{
    // Remaining hub call: loginWorld (session-owned; IGameSession tip twin does not mirror).
    try {
        qtclientGameHub().loginWorld(c.account.toStdString(), c.password.toStdString(), c.worldName.toStdString(),
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
