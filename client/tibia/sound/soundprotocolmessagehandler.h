#pragma once

#include <cip_gen.h>

#include <QByteArray>

#include <tibia/game/tgamenotify.h>

namespace tibia {
namespace sound {

/// CIP sound::TSoundProtocolMessageHandler: applies SoundTrigger (S 0x85).
/// Magic-effect SFX call TSoundStorage from protocolgameparse; this handles
/// the ambience stream, the music and one-shot sound effects.
class TSoundProtocolMessageHandler final : public cip::tibia::sound::TSoundProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSoundProtocolMessageHandler(QObject* parent = nullptr);
    ~TSoundProtocolMessageHandler() override = default;

    /// type 0: ambience stream id, 1: music id (0 stops either), 2: plays the
    /// numeric sound effect `soundId` once. Other types are ignored.
    void handleSoundTriggerMessage(int type, int soundId);

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode);

private:
    tibia::game::SoftHook m_hook;
};

}
}
