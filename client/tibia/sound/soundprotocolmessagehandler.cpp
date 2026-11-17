#include "soundprotocolmessagehandler.h"

#include "sound/soundstorage.h"

#include <protocol.pb.h>

namespace tibia::sound {

void TSoundProtocolMessageHandler::handleSoundTriggerMessage(const ::tibia::protobuf::protocol::GameserverMessageSoundTrigger& message)
{
    // Volumes are applied on option changes and when entering the world.
    // Re-applying here stops the music channel before an ambience-only trigger.
    auto* storage = TSoundStorage::instance();
    if (!storage)
        return;
    const auto soundId = static_cast<uint16_t>(message.sound_id());
    switch (message.type()) {
        case 0:
            storage->playAmbience(soundId);
            break;
        case 1:
            storage->playMusic(soundId);
            break;
        case 2:
            // Same one-shot path as a SOUND_TRIGGER game action.
            storage->playUiEffect(soundId);
            break;
        default:
            break;
    }
}

} // namespace tibia::sound
