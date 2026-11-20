#pragma once

#include "game/gamesessionmetainfo.h"

#include <QString>

namespace tibia::game {

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

/// CIP game::TGameserverGameSessionMetaInfo: a live session, with the account's
/// login data and the chosen character.
class TGameserverGameSessionMetaInfo final : public TGameSessionMetaInfo
{
public:
    EType getType() const override;

    GameserverCredentials loginData;
};

}
