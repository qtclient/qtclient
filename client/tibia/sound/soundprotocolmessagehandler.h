#pragma once

#include <cip_gen.h>

namespace tibia::protobuf::protocol {
class GameserverMessageSoundTrigger;
}

namespace tibia {
namespace sound {

/// CIP sound::TSoundProtocolMessageHandler: applies SoundTrigger (S 0x85).
/// Magic-effect SFX call TSoundStorage from protocolgameparse; this handles
/// the ambience stream, the music and one-shot sound effects.
class TSoundProtocolMessageHandler final : public cip::tibia::sound::TSoundProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::sound::TSoundProtocolMessageHandler::TSoundProtocolMessageHandler;

    /// type 0: ambience stream id, 1: music id (0 stops either), 2: plays the
    /// numeric sound effect once. Other types are ignored.
    void handleSoundTriggerMessage(const ::tibia::protobuf::protocol::GameserverMessageSoundTrigger& message);
};

}
}
