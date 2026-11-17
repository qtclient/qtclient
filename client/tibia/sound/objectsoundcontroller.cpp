#include "sound/objectsoundcontroller.h"

#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <algorithm>
#include <cstdlib>

namespace tibia::sound {

TObjectSoundController::TObjectSoundController(std::shared_ptr<TSoundMetadataManager> metadata)
    : m_metadata(std::move(metadata))
{}

void TObjectSoundController::rebuildIndex()
{
    m_streamsByAppearance.clear();
    m_maxDistance = 0;
    m_indexedStreams = m_metadata ? m_metadata->ambienceObjectStreams().size() : 0;
    if (!m_metadata)
        return;
    for (const auto& [id, stream] : m_metadata->ambienceObjectStreams()) {
        m_maxDistance = std::max(m_maxDistance, stream.maxSoundDistance);
        for (const uint16_t appearance : stream.countedAppearanceTypes)
            m_streamsByAppearance[appearance].push_back(id);
    }
}

void TObjectSoundController::updateObjectCounts(const Position& listener)
{
    if (!m_metadata)
        return;
    if (m_indexedStreams != m_metadata->ambienceObjectStreams().size())
        rebuildIndex();
    if (!listener.isMapPosition()) {
        clearObjectCounts();
        return;
    }
    // qtclient counts here; CipSoft's map storage reports its objects.
    std::map<uint16_t, TCount> counts;
    const int radius = static_cast<int>(m_maxDistance);
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            const Position position(listener.x + dx, listener.y + dy, listener.z);
            tibia::worldmap::TWorldMapField* field = tibia::worldmap::fieldAt(position);
            if (!field)
                continue;
            const int distance = std::max(std::abs(dx), std::abs(dy));
            for (const auto& appearance : field->appearances()) {
                if (!appearance || appearance->category() != ThingCategoryItem)
                    continue;
                const auto streams = m_streamsByAppearance.find(appearance->id());
                if (streams == m_streamsByAppearance.end())
                    continue;
                for (const uint16_t streamId : streams->second) {
                    const auto stream = m_metadata->ambienceObjectStreams().find(streamId);
                    if (stream == m_metadata->ambienceObjectStreams().end()
                        || distance > static_cast<int>(stream->second.maxSoundDistance))
                        continue;
                    TCount& count = counts[streamId];
                    if (count.count == 0 || distance < count.nearestDistance)
                        count.nearestDistance = distance;
                    ++count.count;
                }
            }
        }
    }
    for (auto& [streamId, previous] : m_counts) {
        if (!counts.contains(streamId) && previous.count != 0) {
            previous = TCount{};
            Q_EMIT ambienceObjectStreamCountChanged(streamId, 0, 0);
        }
    }
    for (const auto& [streamId, count] : counts) {
        TCount& previous = m_counts[streamId];
        if (previous == count)
            continue;
        previous = count;
        Q_EMIT ambienceObjectStreamCountChanged(streamId, count.count, count.nearestDistance);
    }
}

void TObjectSoundController::clearObjectCounts()
{
    for (auto& [streamId, previous] : m_counts) {
        if (previous.count == 0)
            continue;
        previous = TCount{};
        Q_EMIT ambienceObjectStreamCountChanged(streamId, 0, 0);
    }
}

}
