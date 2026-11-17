#include "worldmap/cacheforworldmapstoragelookupmembers.h"

#include "worldmap/worldmapstorage.h"

#include <algorithm>

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

int TCacheForWorldmapStorageLookupMembers::indexOf(const TWorldMapCoordinate& coordinate) const
{
    // Slide the field along the floors' shear onto the volume's floor, then
    // count it in the box of width x height x floors.
    const int plane = m_volume.topLeft.z;
    const int dx = coordinate.x + coordinate.z - plane - m_volume.topLeft.x;
    const int dy = coordinate.y + coordinate.z - plane - m_volume.topLeft.y;
    return (coordinate.z * m_volume.extent.height + dy) * m_volume.extent.width + dx;
}

int TCacheForWorldmapStorageLookupMembers::inVolume(const TWorldMapCoordinate& coordinate) const
{
    if (!m_storage || !m_storage->sessionState().isAwareOfPosition(coordinate.toPosition()))
        return -1;
    const int index = indexOf(coordinate);
    return index >= 0 && static_cast<size_t>(index) < m_valid.size() ? index : -1;
}

void TCacheForWorldmapStorageLookupMembers::storedVolumeHasChanged(const TShearedWorldMapCuboid& volume)
{
    m_volume = volume;
    const size_t size = static_cast<size_t>(std::max(0, volume.extent.width * volume.extent.height * Floors));
    if (size != m_valid.size()) {
        m_values.assign(size, false);
        m_valid.assign(size, false);
        // The offsets as bit distances from the volume's first field.
        const TWorldMapCoordinate origin(volume.topLeft.x, volume.topLeft.y, volume.topLeft.z);
        const int base = indexOf(origin);
        m_offsets.clear();
        for (const TWorldMapExtent& extent : m_extents) {
            m_offsets.push_back(indexOf({ origin.x + extent.width, origin.y + extent.height, origin.z + extent.depth })
                                - base);
        }
    }
    std::fill(m_valid.begin(), m_valid.end(), false);
}

void TCacheForWorldmapStorageLookupMembers::refresh(const TWorldMapCoordinate& coordinate, const int index)
{
    for (const int offset : m_offsets) {
        const long neighbour = static_cast<long>(index) + offset;
        if (neighbour >= 0 && static_cast<size_t>(neighbour) < m_valid.size())
            m_valid[static_cast<size_t>(neighbour)] = false;
    }
    m_values[static_cast<size_t>(index)] = m_lookup && m_lookup(coordinate);
    m_valid[static_cast<size_t>(index)] = true;
}

void TCacheForWorldmapStorageLookupMembers::informationAtWorldMapCoordinateHasChanged(const TWorldMapCoordinate& coordinate)
{
    const int index = inVolume(coordinate);
    if (index >= 0)
        refresh(coordinate, index);
}

bool TCacheForWorldmapStorageLookupMembers::lookup(const TWorldMapCoordinate& coordinate)
{
    const int index = inVolume(coordinate);
    if (index < 0)
        return false;
    if (!m_valid[static_cast<size_t>(index)])
        refresh(coordinate, index);
    return m_values[static_cast<size_t>(index)];
}

size_t TCacheForWorldmapStorageLookupMembers::countTrue() const
{
    size_t count = 0;
    for (size_t i = 0; i < m_valid.size(); ++i)
        count += m_valid[i] && m_values[i];
    return count;
}

}

#include "moc_cacheforworldmapstoragelookupmembers.cpp"
