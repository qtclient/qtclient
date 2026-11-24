#include "worldmap/worldmapobjectcounter.h"

#include <algorithm>

namespace tibia::worldmap {

int TWorldMapObjectCounter::s_floorDistance = 0;

TWorldMapObjectCounter::TWorldMapObjectCounter()
{
    m_rows.resize(TypeCount);
}

void TWorldMapObjectCounter::setFloorDistance(const int floorDistance)
{
    s_floorDistance = floorDistance;
}

int TWorldMapObjectCounter::floorDistance()
{
    return s_floorDistance;
}

void TWorldMapObjectCounter::reset()
{
    for (const uint16_t typeId : m_addedTypes)
        m_rows[typeId].fill(0);
    m_addedTypes.clear();
}

void TWorldMapObjectCounter::setCentre(const TWorldMapCoordinate& centre)
{
    m_centre = centre;
}

void TWorldMapObjectCounter::addObject(const TWorldMapCoordinate& field, const uint16_t typeId)
{
    const auto added = std::lower_bound(m_addedTypes.begin(), m_addedTypes.end(), typeId);
    if (added == m_addedTypes.end() || *added != typeId)
        m_addedTypes.insert(added, typeId);

    // Underground (below floor 7) only the centre's floor is heard.
    const bool underground = m_centre.z > 7;
    if (underground && m_centre.z != field.z)
        return;
    const TWorldMapExtent distance = (m_centre - field).absolute();
    const int planar = std::max(distance.width, distance.height);
    const unsigned ring = static_cast<unsigned>(s_floorDistance * distance.depth + planar);
    if (ring >= RingCount || typeId >= TypeCount)
        return;
    uint8_t& objects = m_rows[typeId][ring];
    if (objects != 0xff)
        ++objects;
}

size_t TWorldMapObjectCounter::count(const uint16_t typeId, const int maxDistance, int& nearestDistance) const
{
    nearestDistance = static_cast<int>(RingCount);
    if (typeId >= TypeCount)
        return 0;
    std::array<uint8_t, RingCount> rings = m_rows[typeId];
    const unsigned firstIgnored = static_cast<unsigned>(maxDistance) + 1u;
    if (firstIgnored < RingCount)
        std::fill(rings.begin() + firstIgnored, rings.end(), uint8_t{ 0 });
    size_t objects = 0;
    for (size_t ring = 0; ring < RingCount; ++ring) {
        if (rings[ring] == 0)
            continue;
        objects += rings[ring];
        nearestDistance = std::min(nearestDistance, static_cast<int>(ring));
    }
    return objects;
}

}
