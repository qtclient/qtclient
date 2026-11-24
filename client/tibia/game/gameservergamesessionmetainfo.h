#pragma once

#include "authentication/authenticationjsondocumenthelper.h"
#include "game/gamesessionmetainfo.h"
#include "gamewindow/characterlogindata.h"

#include <QString>

namespace tibia::game {

/// CIP game::TGameserverGameSessionMetaInfo: a live session, with the account's
/// login data and the chosen character.
class TGameserverGameSessionMetaInfo final : public TGameSessionMetaInfo
{
public:
    EType getType() const override;

    /// The account's login (e-mail, password, session key, ...).
    authentication::TLoginRequestData loginRequestData;
    /// The character and how to reach its world.
    gamewindow::TCharacterLoginData characterLoginData;
    /// Host: records the session's packets to this file (qtclient's packet
    /// log), for replay().
    QString recordTo;
};

}
