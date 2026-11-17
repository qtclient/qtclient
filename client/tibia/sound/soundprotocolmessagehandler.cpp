#include "soundprotocolmessagehandler.h"
#include <protocol.pb.h>
#include <tibia/game/gamesession.h>
#include "protocol/protocolciptypes.h"

#include "sound/soundcontroller.h"
#include "sound/weatheraudio.h"

#include <client/gamecore.h>

namespace tibia::sound {

using client::gamecore::argInt;

TSoundProtocolMessageHandler::TSoundProtocolMessageHandler(QObject* parent)
    : cip::tibia::sound::TSoundProtocolMessageHandler(parent)
    , m_hook("onSoundTrigger", [this](const tibia::game::TGameNotify::Args& args) {
          handleSoundTriggerMessage(static_cast<int>(argInt(args, 0)), static_cast<int>(argInt(args, 1)));
          return std::any{};
      })
{
}

void TSoundProtocolMessageHandler::handleSoundTriggerMessage(const int type, const int soundId)
{
    TSoundController* controller = TSoundController::instance();
    if (!controller)
        return;
    switch (type) {
    case 2: {
        TSoundEffectRequest request;
        request.effectId = static_cast<TSoundEffectID>(soundId);
        const TSoundEffectParameters parameters = controller->effectParameters(request);
        if (std::optional<TSoundPlayerToken> token = controller->playSingleShot(parameters))
            controller->keepSingleShot(std::move(*token));
        break;
    }
    case 1:
        controller->setMusicTemplate(static_cast<uint16_t>(soundId));
        break;
    case 0:
        // Host: a pack weather bed may hold the ambience channel.
        if (!TWeatherAudio::instance().deferAmbienceStream(static_cast<uint16_t>(soundId)))
            controller->setAmbienceStream(static_cast<uint16_t>(soundId));
        break;
    default:
        break;
    }
}

void TSoundProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow || opcode != 133 || !shadow->HasExtension(GameserverMessageSoundTrigger::gameserver_message_sound_trigger))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageSoundTrigger::gameserver_message_sound_trigger);
    if (m.type() <= 2)
        tibia::game::processSoundTrigger(static_cast<uint8_t>(m.type()), static_cast<uint16_t>(m.sound_id()));
}

} // namespace tibia::sound
