#include "worldmap/cacheforworldmapstoragelookupmembers.h"

#include "worldmap/worldmapstorage.h"

namespace tibia::worldmap {

TCacheForWorldmapStorageLookupMembers::TCacheForWorldmapStorageLookupMembers(TWorldMapStorage* storage, Lookup lookup,
                                                                             const std::vector<TWorldMapExtent>& extents)
    : cip::tibia::worldmap::TCacheForWorldmapStorageLookupMembers(nullptr)
    , m_storage(storage)
    , m_lookup(std::move(lookup))
    , m_extents(extents)
{
    if (!m_storage)
        return;
    storedVolumeHasChanged(m_storage->storedVolume());
    connect(m_storage, &TWorldMapStorage::storedVolumeHasChanged, this,
            &TCacheForWorldmapStorageLookupMembers::storedVolumeHasChanged);
    connect(m_storage, &TWorldMapStorage::informationAtWorldMapCoordinateHasChanged, this,
            &TCacheForWorldmapStorageLookupMembers::informationAtWorldMapCoordinateHasChanged);
}

quint64 TCacheForWorldmapStorageLookupMembers::key(const TWorldMapCoordinate& coordinate)
{
    return (quint64(quint32(coordinate.x)) << 32) | (quint64(quint16(coordinate.y)) << 8) | quint8(coordinate.z);
}

bool TCacheForWorldmapStorageLookupMembers::inVolume(const TWorldMapCoordinate& coordinate) const
{
    return m_storage && m_storage->sessionState().isAwareOfPosition(coordinate.toPosition());
}

void TCacheForWorldmapStorageLookupMembers::storedVolumeHasChanged(const TShearedWorldMapCuboid&)
{
    m_answers.clear();
}

void TCacheForWorldmapStorageLookupMembers::informationAtWorldMapCoordinateHasChanged(const TWorldMapCoordinate& coordinate)
{
    if (!inVolume(coordinate))
        return;
    // The answers around the field are asked again when needed; the field's
    // own answer is asked right away.
    for (const TWorldMapExtent& offset : m_extents)
        m_answers.erase(key({ coordinate.x + offset.width, coordinate.y + offset.height, coordinate.z + offset.depth }));
    m_answers[key(coordinate)] = m_lookup && m_lookup(coordinate);
}

bool TCacheForWorldmapStorageLookupMembers::lookup(const TWorldMapCoordinate& coordinate)
{
    if (!inVolume(coordinate))
        return false;
    const quint64 k = key(coordinate);
    if (const auto it = m_answers.find(k); it != m_answers.end())
        return it->second;
    for (const TWorldMapExtent& offset : m_extents)
        m_answers.erase(key({ coordinate.x + offset.width, coordinate.y + offset.height, coordinate.z + offset.depth }));
    const bool answer = m_lookup && m_lookup(coordinate);
    m_answers[k] = answer;
    return answer;
}

}

#include "moc_cacheforworldmapstoragelookupmembers.cpp"
