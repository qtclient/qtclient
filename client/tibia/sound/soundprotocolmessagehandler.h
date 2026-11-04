#pragma once

#include <cip_gen.h>

#include <tibia/game/tgamenotify.h>

namespace tibia::sound {

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

private:
    tibia::game::SoftHook m_hook;
};

}
