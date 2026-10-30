#include "soundprotocolmessagehandler.h"

#include "sound/soundstorage.h"

#include <client/gamecore.h>
#include <framework/core/gameevents.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSoundProto, "tibia.sound.protocol")

namespace tibia::sound {

using client::gamecore::argInt;

TSoundProtocolMessageHandler::TSoundProtocolMessageHandler(QObject* parent)
    : cip::tibia::sound::TSoundProtocolMessageHandler(parent)
{
    m_hook = g_gameEvents.subscribe("g_game", "onSoundTrigger",
                                    [this](LuaObject*, const GameEvents::Args& args) {
                                        handleSoundTrigger(static_cast<int>(argInt(args, 0)),
                                                           static_cast<int>(argInt(args, 1)));
                                        return std::any{};
                                    });
}

TSoundProtocolMessageHandler::~TSoundProtocolMessageHandler()
{
    if (m_hook)
        g_gameEvents.unsubscribe(m_hook);
}

void TSoundProtocolMessageHandler::handleSoundTrigger(const int type, const int soundId)
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
        case 2:
            storage->playMusic(static_cast<uint16_t>(soundId));
            break;
        default:
            qCDebug(lcSoundProto) << "unknown sound trigger type" << type << "id" << soundId;
            break;
    }
}

}
