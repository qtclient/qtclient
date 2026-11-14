#pragma once

#include <cip_gen.h>

#include <QByteArray>

#include <tibia/game/tgamenotify.h>

namespace tibia {
namespace sound {

/// Bridges CIP SoundTrigger / OTC Anthem (S 0x85) qtclient notify to TSoundStorage.
/// Magic-effect SFX call TSoundStorage from protocolgameparse; this handles
/// ambient / music / anthem.
class TSoundProtocolMessageHandler final : public cip::tibia::sound::TSoundProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSoundProtocolMessageHandler(QObject* parent = nullptr);
    ~TSoundProtocolMessageHandler() override = default;

    /// type: 0 ambient, 1 music (0 = stop), 2 anthem.
    void handleSoundTrigger(int type, int soundId);

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);

private:
    tibia::game::SoftHook m_hook;
};

}
}
