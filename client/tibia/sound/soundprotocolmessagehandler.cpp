#include "soundprotocolmessagehandler.h"

#include "sound/soundcontroller.h"
#include "sound/ambiencehold.h"

#include <protocol.pb.h>

namespace tibia::sound {

TSoundProtocolMessageHandler::TSoundProtocolMessageHandler(TSoundController* SoundController)
    : cip::tibia::sound::TSoundProtocolMessageHandler(nullptr)
    , m_soundController(SoundController)
{
}

void TSoundProtocolMessageHandler::handleSoundTriggerMessage(const ::tibia::protobuf::protocol::GameserverMessageSoundTrigger& message)
{
    TSoundController* controller = m_soundController;
    if (!controller)
        return;
    const auto soundId = static_cast<uint16_t>(message.sound_id());
    switch (message.type()) {
    case 2: {
        TSoundEffectRequest request;
        request.effectId = static_cast<TSoundEffectID>(soundId);
        const TSoundEffectParameters parameters = controller->effectParameters(request);
        if (std::optional<TSoundPlayerToken> token = controller->playSingleShot(parameters))
            controller->keepSingleShot(std::move(*token));
        break;
    }
    case 1:
        controller->setMusicTemplate(soundId);
        break;
    case 0:
        // Host: an extension may hold the ambience channel (no hold without a pack).
        if (!TAmbienceHold::instance().deferAmbienceStream(soundId))
            controller->setAmbienceStream(soundId);
        break;
    default:
        break;
    }
}

} // namespace tibia::sound
