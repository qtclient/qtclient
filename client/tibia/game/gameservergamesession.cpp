#include "gameservergamesession.h"

#include "game/gameciptypes.h"
#include "game/tgamenotify.h"

#include <tibia/game/gamenotify.h>
#include <client/protocolgame.h>
#include <tibia/game/gamesession.h>

namespace tibia::game {

using client::gamecore::argBool;
using client::gamecore::argInt;
using client::gamecore::argString;
using client::gamecore::Hook;
using Args = std::vector<std::any>;

namespace {

template<typename F>
Hook gameHook(const char* field, F&& f)
{
    return Hook(tibia::game::kNotifyScope, field, [f = std::forward<F>(f)](GameObject*, const Args& args) {
        f(args);
        return std::any{};
    });
}

}

TGameserverGameSession::TGameserverGameSession(QObject* parent)
    : cip::tibia::game::TGameserverGameSession(parent)
{
    game::ownProductSurface(this);
    m_hooks.emplace_back("object", "onConnect", [this](GameObject* self, const Args&) {
        if (dynamic_cast<ProtocolGame*>(self))
            Q_EMIT connected();
        return std::any{};
    });
    m_hooks.push_back(gameHook("onLogin", [this](const Args&) { Q_EMIT gameLoginSuccessful(); }));
    // CIP typed lifecycle (TGameNotify) — prefer over string Hook for start/end/fail.
    connect(TGameNotify::instance(), &TGameNotify::gameStarted, this, [this] { Q_EMIT worldEntered(); });
    connect(TGameNotify::instance(), &TGameNotify::gameEnded, this, [this] {
        // Same-socket alt switch ends the session in-process. The socket stays
        // open, so this must not look like a logout (that reopens CharacterSelection).
        if (g_game.isSwitchingCharacter())
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

TGameserverGameSession::~TGameserverGameSession() = default;

void TGameserverGameSession::connectToGameserver(const GameserverCredentials& c)
{
    try {
        g_game.loginWorld(c.account.toStdString(), c.password.toStdString(), c.worldName.toStdString(),
                          c.host.toStdString(), c.port, c.characterName.toStdString(),
                          c.authenticatorToken.toStdString(), c.sessionKey.toStdString(), c.recordTo.toStdString());
    } catch (const std::exception& e) {
        Q_EMIT connectionError(QString::fromUtf8(e.what()));
    }
}

void TGameserverGameSession::requestLogout()
{
    g_game.safeLogout();
}

void TGameserverGameSession::disconnectNow()
{
    if (g_game.isOnline())
        g_game.forceLogout();
    else
        g_game.cancelLogin();
}

bool TGameserverGameSession::isOnline() const
{
    return g_game.isOnline();
}

QString TGameserverGameSession::characterName() const
{
    return QString::fromStdString(g_game.getCharacterName());
}

QString TGameserverGameSession::worldName() const
{
    return QString::fromStdString(g_game.getWorldName());
}

}
