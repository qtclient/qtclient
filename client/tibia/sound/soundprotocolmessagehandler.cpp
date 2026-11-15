#include "soundprotocolmessagehandler.h"
#include <protocol.pb.h>
#include <tibia/game/gamesession.h>
#include "protocol/protocolciptypes.h"

#include "sound/soundstorage.h"

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
    // Volumes are applied on option changes and when entering the world.
    // Re-applying here stops the music channel before an ambience-only trigger.
    auto* storage = TSoundStorage::instance();
    if (!storage)
        return;
    switch (type) {
        case 0:
            storage->playAmbience(static_cast<uint16_t>(soundId));
            break;
        case 1:
            storage->playMusic(static_cast<uint16_t>(soundId));
            break;
        case 2:
            // Same one-shot path as a SOUND_TRIGGER game action.
            storage->playUiEffect(static_cast<uint16_t>(soundId));
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
