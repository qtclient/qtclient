#include "sound/objectsoundcontroller.h"

#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmapobjectcounter.h"

#include <algorithm>

namespace tibia::sound {

TObjectSoundController::TObjectSoundController(
    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider,
    std::shared_ptr<TSoundMetadataManager> soundMetadataManager)
    : m_worldMapRenderProvider(std::move(worldMapRenderProvider))
    , m_soundMetadataManager(std::move(soundMetadataManager))
{}

void TObjectSoundController::updateObjectCounts()
{
    const auto& streams = m_soundMetadataManager->ambienceObjectStreams();
    const worldmap::TWorldMapObjectCounter& counter = m_worldMapRenderProvider->objectCounter();
    for (const auto& [key, stream] : streams) {
        TObjectCount previous;
        if (const auto known = m_objectCounts.find(stream.id); known != m_objectCounts.end())
            previous = known->second;

        size_t count = 0;
        int nearestDistance = static_cast<int>(worldmap::TWorldMapObjectCounter::RingCount);
        for (const uint16_t typeId : stream.countedAppearanceTypes) {
            int typeNearest = static_cast<int>(worldmap::TWorldMapObjectCounter::RingCount);
            count += counter.count(typeId, static_cast<int>(stream.maxSoundDistance), typeNearest);
            nearestDistance = std::min(nearestDistance, typeNearest);
        }

        TObjectCount& current = m_objectCounts[stream.id];
        current.count = count;
        current.nearestDistance = nearestDistance;
        if (previous.count != count || previous.nearestDistance != nearestDistance)
            Q_EMIT ambienceObjectStreamCountChanged(stream.id, count, nearestDistance);
    }
}

}
