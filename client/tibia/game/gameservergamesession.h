#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QString>

#include <string_view>
#include <vector>

namespace tibia {
namespace game {

/// Hub login teardown (`forceLogout` / `cancelLogin`). Only
/// `TGameserverGameSession`, `TSessiondumpGameSession`, and `gamecore::terminate`
/// may call this — the only path that touches `g_game` for disconnect.
void hubDisconnectNow();

/// What the game server needs to log a character in.
struct GameserverCredentials
{
    QString account;
    QString password;
    QString sessionKey;
    QString authenticatorToken;
    QString worldName;
    QString host;
    int port = 0;
    QString characterName;
    /// Records the session's packets to this file (CIP's packet log), for replay().
    QString recordTo;
};

/// Live gameserver session (CIP: TGameserverGameSession). Owns loginWorld,
/// forceLogout, and cancelLogin on the OTC hub; other queries/actions use
/// `gamesession` peels. SoftHooks / TGameNotify fan out CIP session signals.
class TGameserverGameSession final : public cip::tibia::game::TGameserverGameSession
{
    Q_OBJECT
public:
    explicit TGameserverGameSession(QObject* parent = nullptr);
    ~TGameserverGameSession() override;

    /// Connects and logs in; progress arrives as signals.
    void connectToGameserver(const GameserverCredentials& credentials);
    /// Asks the server to log the character out (it may refuse, e.g. in fight).
    void requestLogout();
    /// Drops the connection without waiting for the server.
    void disconnectNow();

    bool isOnline() const;
    QString characterName() const;
    QString worldName() const;

    static TGameserverGameSession* liveInstance();
    static void setLiveInstance(TGameserverGameSession* session);

    /// ProtocolGame lifecycle peels (notify bus + simple hub fields). `Game::process*`
    /// forwards here when a live session is registered; static fallbacks keep
    /// sessiondump / headless paths working without `g_game` bodies in game.cpp.
    static void wireProcessLogin();
    static void wireProcessLoginError(std::string_view error);
    static void wireProcessLoginAdvice(std::string_view message);
    static void wireProcessLoginWait(std::string_view message, uint8_t time);
    static void wireProcessSessionEnd(uint8_t reason);
    static void wireProcessWorldEntered();
    static void wireProcessDeath(uint8_t deathType, uint8_t penality);
    static void wireProcessPing();
    static void wireProcessPingBack();

Q_SIGNALS:
    /// CIP's carry an ELoginErrorAction / TSeconds the generator does not map.
    void gameLoginErrorMessage(const QString& ErrorMessage);
    void gameLoginWaitMessage(const QString& WaitMessage, int WaitSecondsUntilNextTry);
    void sessionEndInformation(int Reason);
    /// The server wants a newer client (UpdateNeeded).
    void clientOutOfDate();

private:
    std::vector<tibia::game::SoftHook> m_hooks;
};

}
}
