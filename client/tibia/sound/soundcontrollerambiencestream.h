#pragma once

#include "sound/soundcontrollerbase.h"

#include <map>

namespace tibia {
namespace sound {

/// CIP sound::TSoundControllerAmbienceStream: the place's ambience loop and
/// its randomly delayed effects.
class TSoundControllerAmbienceStream final : public TSoundControllerBase
{
    Q_OBJECT
public:
    TSoundControllerAmbienceStream(std::shared_ptr<TSoundMetadataManager> metadata,
                                   std::shared_ptr<TSoundManager> soundManager,
                                   std::weak_ptr<TSoundPlayerManager> playerManager,
                                   std::shared_ptr<TSoundConfiguration> configuration);
    ~TSoundControllerAmbienceStream() override;

    void update(int64_t nowMs) override;
    void setSuspended(bool suspended) override;
    /// The server's ambience stream (0: none).
    void setAmbienceStream(uint16_t streamId);
    uint16_t ambienceStreamId() const { return m_requestedStreamId; }
    /// Stops the loop (fades it out unless `immediately`) and its effects.
    void stop(bool immediately);
    bool isStreamPlaying(uint16_t streamId) const;

protected:
    void onSoundConfigurationChanged() override;

private:
    struct TScheduledEffect
    {
        uint16_t effectId = 0;
        int64_t delayMs = 0;
    };

    void startStream(bool updatePlayer);
    void releasePlayer();

    TAmbienceStreamMetadata m_stream;
    TActiveSoundPlayer* m_player = nullptr;
    std::multimap<int64_t, TScheduledEffect> m_scheduledEffects;
    uint16_t m_requestedStreamId = 0;
};

}
}
