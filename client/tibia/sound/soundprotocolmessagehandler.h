#pragma once

#include <cip_gen.h>

#include <tibia/game/tgamenotify.h>

namespace tibia {
namespace sound {

/// CIP sound::TSoundProtocolMessageHandler: applies SoundTrigger (S 0x85)
/// to the client's sound controller.
class TSoundProtocolMessageHandler final : public cip::tibia::sound::TSoundProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSoundProtocolMessageHandler(QObject* parent = nullptr);
    ~TSoundProtocolMessageHandler() override = default;

    /// type 0: the ambience stream, 1: the music template (0 stops either),
    /// 2: plays the numeric sound effect once. Other types are ignored.
    void handleSoundTriggerMessage(int type, int soundId);

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode);

private:
    tibia::game::SoftHook m_hook;
};

}
}
