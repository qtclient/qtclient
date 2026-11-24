#pragma once

#include <cip_gen.h>

#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageGameEvent;
}

namespace tibia::game {

/// CIP game::TGameEventProtocolMessageHandler: the server's game events (level
/// and skill ups, achievements, bestiary and quest progress, unlocks, the simple
/// in-game events). Each one is published as an input::gameactions::TGameActionGameEvent;
/// some also write an info text into the chat or take an automatic screenshot.
class TGameEventProtocolMessageHandler final : public cip::tibia::game::TGameEventProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::game::TGameEventProtocolMessageHandler::TGameEventProtocolMessageHandler;

    /// Publishes the event. Simple events map to their screenshot type (an
    /// unknown one throws std::runtime_error("unhandled enum value")); the info
    /// banner events and the level 20 promotion hint add a text to the chat.
    void handleGameEventMessage(const ::tibia::protobuf::protocol::GameserverMessageGameEvent& GameEventMessage);
};

}
