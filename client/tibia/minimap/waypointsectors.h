#pragma once

#include <client/position.h>

#include <QImage>

#include <array>
#include <cstdint>
#include <vector>

namespace tibia::minimap {

/// The waypoint costs of one minimap tile: 256 x 256 fields, row by row
/// from the tile's top-left field.
struct TWaypointSector
{
    static constexpr int Size = 256;
    /// The cost of a field nothing is known of.
    static constexpr uint8_t UnknownCost = 0xfe;

    /// The id of the tile (its top-left field).
    Position tileId;
    std::array<uint8_t, Size * Size> costs{};
};

/// The waypoint costs the path planner reads, kept per minimap tile (CIP:
/// the minimap tile storage's sector list; the planner gets a copy).
class TWaypointSectors
{
public:
    const std::vector<TWaypointSector>& sectors() const { return m_sectors; }

    /// The cost of the field `coordinate`; unknown without a sector holding it.
    uint8_t costAt(const Position& coordinate) const;
    /// Sets the cost of the field `coordinate`; a tile without a sector gets
    /// one of unknown costs first.
    void setCost(const Position& coordinate, uint8_t cost);
    /// The sector of tile `tileId` takes the costs of the tile's waypoint
    /// image (256 x 256, one byte per field); a new sector is added for a tile
    /// without one.
    void setSector(const Position& tileId, const QImage& waypoints);
    /// Drops the sectors of the tiles `tileIds`.
    void removeSectors(const std::vector<Position>& tileIds);

private:
    TWaypointSector* sectorOf(const Position& tileId);
    TWaypointSector& addSector(const Position& tileId);

    std::vector<TWaypointSector> m_sectors;
};

} // namespace tibia::minimap
