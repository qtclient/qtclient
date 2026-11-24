#include "minimap/waypointsectors.h"

#include "minimap/minimaptile.h"

#include <algorithm>
#include <cstring>

namespace tibia::minimap {

namespace {

/// The index of `coordinate` in the costs of the sector of tile `tileId`.
std::size_t costIndex(const Position& coordinate, const Position& tileId)
{
    return static_cast<std::size_t>((coordinate.x - tileId.x) + (coordinate.y - tileId.y) * TWaypointSector::Size);
}

} // namespace

TWaypointSector* TWaypointSectors::sectorOf(const Position& tileId)
{
    const auto it = std::find_if(m_sectors.begin(), m_sectors.end(),
                                 [&tileId](const TWaypointSector& sector) { return sector.tileId == tileId; });
    return it == m_sectors.end() ? nullptr : &*it;
}

TWaypointSector& TWaypointSectors::addSector(const Position& tileId)
{
    // Host: built in place (a sector is 64 KB, too large for the WASM stack).
    TWaypointSector& sector = m_sectors.emplace_back();
    sector.tileId = tileId;
    sector.costs.fill(TWaypointSector::UnknownCost);
    return sector;
}

uint8_t TWaypointSectors::costAt(const Position& coordinate) const
{
    const Position tileId = TMinimapTile::idOf(coordinate);
    for (const TWaypointSector& sector : m_sectors) {
        if (sector.tileId == tileId)
            return sector.costs[costIndex(coordinate, tileId)];
    }
    return TWaypointSector::UnknownCost;
}

void TWaypointSectors::setCost(const Position& coordinate, const uint8_t cost)
{
    const Position tileId = TMinimapTile::idOf(coordinate);
    TWaypointSector* sector = sectorOf(tileId);
    if (!sector)
        sector = &addSector(tileId);
    sector->costs[costIndex(coordinate, tileId)] = cost;
}

void TWaypointSectors::setSector(const Position& tileId, const QImage& waypoints)
{
    TWaypointSector* sector = sectorOf(tileId);
    if (!sector)
        sector = &addSector(tileId);
    // Host: an image too small to hold a tile (none loaded) leaves the costs.
    if (waypoints.sizeInBytes() < static_cast<qsizetype>(sector->costs.size()))
        return;
    std::memmove(sector->costs.data(), waypoints.constScanLine(0), sector->costs.size());
}

void TWaypointSectors::removeSectors(const std::vector<Position>& tileIds)
{
    m_sectors.erase(std::remove_if(m_sectors.begin(), m_sectors.end(),
                                   [&tileIds](const TWaypointSector& sector) {
                                       return std::find(tileIds.begin(), tileIds.end(), sector.tileId) != tileIds.end();
                                   }),
                    m_sectors.end());
}

} // namespace tibia::minimap
