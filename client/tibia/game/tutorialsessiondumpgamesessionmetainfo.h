#pragma once

#include "game/sessiondumpgamesessionmetainfo.h"

#include <QString>
#include <QUrl>

namespace tibia::game {

/// CIP game::TTutorialSessiondumpGameSessionMetaInfo: the tutorial dump a new
/// character watches before its live session.
class TTutorialSessiondumpGameSessionMetaInfo final : public TSessiondumpGameSessionMetaInfo
{
public:
    EType getType() const override;

    /// The character the tutorial is played for.
    QString characterName;
    /// Whether the character is male (the tutorial character's outfit).
    bool isMale = false;
    /// Where the tutorial progress of the character is posted.
    QUrl tutorialProgressWebService;
    /// The client session the progress is posted for.
    QString clientSessionId;
};

}
