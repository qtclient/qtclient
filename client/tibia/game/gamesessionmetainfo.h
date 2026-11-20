#pragma once

#include "game/playermovementintenthandlerconfiguration.h"

#include <QtGlobal>

#include <memory>

namespace tibia::protocol {
struct TProtocolMessageQueueConfiguration;
}

namespace tibia::game {

/// CIP game::TGameSessionMetaInfo: what a game session is built from. The game
/// client queues one per session it is going to play (the tutorial dump before
/// the live login, a chosen replay) and builds the session from the next one.
class TGameSessionMetaInfo
{
public:
    /// The kind of session the meta info builds.
    enum class EType : int
    {
        Gameserver = 0,
        Sessiondump = 1,
        TutorialSessiondump = 2,
    };

    virtual ~TGameSessionMetaInfo();

    virtual EType getType() const = 0;

    /// The session's protocol message queue delays (DEBUG/simulatedGameserverLag,
    /// DEBUG/simulatedGameclientLag).
    std::shared_ptr<protocol::TProtocolMessageQueueConfiguration> protocolMessageQueueConfiguration;
    /// GRAPHICS/maximumNumberOfEffects (400, at most 800).
    quint32 maximumNumberOfEffects = 0;
    /// GRAPHICS/millisecondsForAmbientLightChange (10200).
    qint64 millisecondsForAmbientLightChange = 0;
    /// CHAT/maxChatBacklogLength (1000, at least 5).
    quint32 maxChatBacklogLength = 0;
    std::shared_ptr<TPlayerMovementIntentHandlerConfiguration> playerMovementIntentHandlerConfiguration =
        std::make_shared<TPlayerMovementIntentHandlerConfiguration>();
};

}
