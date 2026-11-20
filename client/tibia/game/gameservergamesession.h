#pragma once

#include "game/gamesessionbase.h"
#include "game/gameservergamesessionmetainfo.h"

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

/// Live gameserver session (CIP: TGameserverGameSession), one per login. Owns
/// loginWorld, forceLogout, and cancelLogin on the OTC hub; other
/// queries/actions use `gamesession` peels. SoftHooks / TGameNotify fan out
/// CIP session signals.
class TGameserverGameSession final : public TGameSessionBase
{
    Q_OBJECT
public:
    explicit TGameserverGameSession(QObject* parent = nullptr);
    ~TGameserverGameSession() override;

    /// Builds the session from a TGameserverGameSessionMetaInfo and keeps its
    /// login data.
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;
    void startSession() override;
    /// Connects with the meta info's login data and logs in; progress arrives
    /// as signals.
    void connectToGameserver();
    /// Asks the server to log the character out (it may refuse, e.g. in fight).
    void requestLogout();
    /// Drops the connection without waiting for the server.
    void disconnectNow() override;

    bool isOnline() const;
    QString characterName() const;
    QString worldName() const;

    /// Session lifecycle side effects (notify bus + session state). `tibia::game::process*`
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
    /// The server wants a newer client (UpdateNeeded).
    void clientOutOfDate();

private:
    GameserverCredentials m_loginData;
    std::vector<tibia::game::SoftHook> m_hooks;
};

}
}
