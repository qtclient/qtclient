#pragma once

#include "sound/soundcontrollerbase.h"

#include <unordered_map>

namespace tibia {
namespace sound {

/// CIP sound::TSoundControllerAmbienceObjectSound: loops of the sound objects
/// (fountains, fire, …) around the player, by how many there are.
class TSoundControllerAmbienceObjectSound final : public TSoundControllerBase
{
    Q_OBJECT
public:
    TSoundControllerAmbienceObjectSound(std::shared_ptr<TSoundMetadataManager> metadata,
                                        std::shared_ptr<TSoundManager> soundManager,
                                        std::weak_ptr<TSoundPlayerManager> playerManager,
                                        std::shared_ptr<TSoundConfiguration> configuration);
    ~TSoundControllerAmbienceObjectSound() override;

    void update(int64_t nowMs) override;
    void setSuspended(bool suspended) override;
    /// `count` objects of the stream are in range, the nearest
    /// `nearestDistance` fields away.
    void ambienceObjectStreamCountChanged(uint16_t streamId, size_t count, int nearestDistance);
    void stopAllAmbienceObjectSounds();

protected:
    void onSoundConfigurationChanged() override;

private:
    struct TEntry
    {
        TAmbienceObjectStreamMetadata stream;
        size_t count = 0;
        int nearestDistance = 0;
        TActiveSoundPlayer* player = nullptr;
    };

    void applyDistance(TActiveSoundPlayer* player, int nearestDistance);

    std::unordered_map<uint16_t, TEntry> m_entries;
};

}
}
