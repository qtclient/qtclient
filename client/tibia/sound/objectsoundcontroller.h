#pragma once

#include "sound/soundmetadatamanager.h"

#include <cip_gen.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
namespace sound {

/// CIP sound::TObjectSoundController: after every prepared frame, reads the
/// render provider's object counter for each ambience object stream and
/// announces the streams whose object count or nearest distance changed.
class TObjectSoundController final : public cip::tibia::sound::TObjectSoundController
{
    Q_OBJECT
public:
    TObjectSoundController(std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider,
                           std::shared_ptr<TSoundMetadataManager> soundMetadataManager);

    /// Recounts every ambience object stream from the last built frame.
    void updateObjectCounts();

Q_SIGNALS:
    void ambienceObjectStreamCountChanged(uint16_t soundEffectId, size_t count, int nearestDistance);

private:
    struct TObjectCount
    {
        size_t count = 0;
        int nearestDistance = 0;
    };

    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    std::shared_ptr<TSoundMetadataManager> m_soundMetadataManager;
    /// Stream id -> the count last announced.
    std::map<uint16_t, TObjectCount> m_objectCounts;
};

}
}
