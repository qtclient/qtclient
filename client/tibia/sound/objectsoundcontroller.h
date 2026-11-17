#pragma once

#include "sound/soundmetadatamanager.h"

#include <cip_gen.h>

#include <client/position.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tibia {
namespace sound {

/// CIP sound::TObjectSoundController: counts a game session's sound objects
/// around the player per ambience object stream.
class TObjectSoundController final : public cip::tibia::sound::TObjectSoundController
{
    Q_OBJECT
public:
    explicit TObjectSoundController(std::shared_ptr<TSoundMetadataManager> metadata);

    /// Recounts the objects around `listener` and announces the streams
    /// whose count or nearest distance changed.
    void updateObjectCounts(const Position& listener);
    /// Every stream's count is 0 (no map, or the session ended).
    void clearObjectCounts();

Q_SIGNALS:
    void ambienceObjectStreamCountChanged(uint16_t soundEffectId, size_t count, int nearestDistance);

private:
    struct TCount
    {
        size_t count = 0;
        int nearestDistance = 0;
        bool operator==(const TCount&) const = default;
    };

    void rebuildIndex();

    std::shared_ptr<TSoundMetadataManager> m_metadata;
    /// Appearance id -> the streams that count it.
    std::unordered_map<uint32_t, std::vector<uint16_t>> m_streamsByAppearance;
    uint32_t m_maxDistance = 0;
    size_t m_indexedStreams = 0;
    std::map<uint16_t, TCount> m_counts;
};

}
}
