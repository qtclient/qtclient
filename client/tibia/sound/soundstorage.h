#pragma once

#include "sound/soundcontrollercombatsound.h"

#include <cip_gen.h>

#include <cstdint>
#include <vector>

namespace tibia {
namespace sound {

/// CIP sound::TSoundStorage: a game session's world sound effects to play.
class TSoundStorage final : public cip::tibia::sound::TSoundStorage
{
    Q_OBJECT
public:
    explicit TSoundStorage(int maxSoundCount = 100);
    ~TSoundStorage() override;

    const std::vector<TSoundToPlay>& soundsToPlay() const { return m_soundsToPlay; }
    /// The effects of one protocol message.
    void setSoundsToPlay(std::vector<TSoundToPlay> sounds, int64_t timestampMs);
    /// An effect that plays at its timestamp (update).
    void queueSound(const TSoundToPlay& sound);
    /// v12: the queued effects due at `nowMs` become the sounds to play.
    void update(int64_t nowMs);

private:
    std::vector<TSoundToPlay> m_soundsToPlay;
    int64_t m_timestampMs = 0;
    std::vector<TSoundToPlay> m_queuedSounds;
    int m_maxSoundCount = 100;
};

}
}
