#pragma once

#include <cip_gen.h>

namespace tibia::sound {

/// Bridges CIP SoundTrigger / OTC Anthem (S 0x85) GameEvents to TSoundStorage.
/// Magic-effect SFX call TSoundStorage from protocolgameparse; this handles
/// ambient / music / anthem.
class TSoundProtocolMessageHandler final : public cip::tibia::sound::TSoundProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSoundProtocolMessageHandler(QObject* parent = nullptr);
    ~TSoundProtocolMessageHandler() override;

    /// type: 0 ambient, 1 music (0 = stop), 2 anthem.
    void handleSoundTrigger(int type, int soundId);

private:
    int m_hook = 0;
};

}
