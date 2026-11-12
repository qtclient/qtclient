#pragma once

#include "minimap/minimaptile.h"

#include <client/position.h>

#include <QImage>
#include <QString>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace tibia {
namespace minimap {

class TMinimapTileStorage;

/// CIP Indexed8 minimap feed for pathfinding: waypoint costs + automap colours.
/// Costs: 0..250 walkable, 0xfe unknown, 0xff blocked. Prefer in-memory
/// qtclient tiles, then AppData/minimap PNGs. Built on the GUI thread (image
/// copies); safe for one async worker afterward.
class TWaypointCosts
{
public:
    /// `waypoints` / `colors`: Indexed8 images keyed by `coordinateKey` of the tile id.
    TWaypointCosts(std::unordered_map<uint64_t, QImage> waypoints,
                   std::unordered_map<uint64_t, QImage> colors,
                   QString minimapDir);

    uint8_t costAt(int x, int y, int z);
    /// Automap palette index (0 = unknown / empty). CIP Minimap_Color_*.png.
    uint8_t colorIndexAt(int x, int y, int z);
    bool isWalkable(int x, int y, int z) { return costAt(x, y, z) <= MaxCost; }

    static constexpr uint8_t MaxCost = 250;
    static constexpr uint8_t UnknownCost = 0xfe;
    static constexpr uint8_t BlockedCost = 0xff;

private:
    const QImage& diskWaypoint(const Position& id);
    const QImage& diskColor(const Position& id);
    static uint8_t sampleIndexed8(const QImage& image, int px, int py, uint8_t fallback);

    std::unordered_map<uint64_t, QImage> m_waypoints;
    std::unordered_map<uint64_t, QImage> m_diskWaypoints;
    std::unordered_map<uint64_t, QImage> m_colors;
    std::unordered_map<uint64_t, QImage> m_diskColors;
    QString m_dir;
};

/// Snapshot qtclient in-memory tiles + disk dir for off-thread pathfinding (CIP
/// TWaypointCostMap feed into the movement path planner).
std::shared_ptr<TWaypointCosts> snapshotWaypointCosts(TMinimapTileStorage* storage);

}
}
