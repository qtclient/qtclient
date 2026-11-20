#pragma once

#include "game/gamesessionmetainfo.h"

#include "sessiondump/sessiondumpcatalog.h"

namespace tibia::game {

/// CIP game::TSessiondumpGameSessionMetaInfo: a recorded session to replay, as
/// the sessiondump catalog lists it.
class TSessiondumpGameSessionMetaInfo : public TGameSessionMetaInfo
{
public:
    TSessiondumpGameSessionMetaInfo();

    EType getType() const override;

    sessiondump::TSessiondumpCatalog::TEntry sessiondump;
    /// True when the player started the replay (the start-sessiondump game
    /// action); the login's tutorial dump leaves it false.
    bool startedByGameAction = false;
};

}
