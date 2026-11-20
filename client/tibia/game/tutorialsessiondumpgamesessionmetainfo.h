#pragma once

#include "game/sessiondumpgamesessionmetainfo.h"

#include <QString>

namespace tibia::game {

/// CIP game::TTutorialSessiondumpGameSessionMetaInfo: the tutorial dump a new
/// character watches before its live session.
class TTutorialSessiondumpGameSessionMetaInfo final : public TSessiondumpGameSessionMetaInfo
{
public:
    EType getType() const override;

    /// The character the tutorial is played for.
    QString characterName;
};

}
