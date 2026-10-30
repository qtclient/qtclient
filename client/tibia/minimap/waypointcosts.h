#pragma once

#include "minimap/minimaptile.h"

#include <client/position.h>

#include <QImage>
#include <QString>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace tibia::minimap {

class TMinimapTileStorage;

/// CIP minimap waypoint costs (0..250 walkable, 0xfe unknown, 0xff blocked).
/// Prefer in-memory Soft tiles, then fall back to AppData/minimap PNGs.
/// Built on the GUI thread (image copies); safe for one async worker afterward.
class TWaypointCosts
{
public:
    /// `loaded`: waypoint images keyed by `coordinateKey` of the tile id.
    TWaypointCosts(std::unordered_map<uint64_t, QImage> loaded, QString minimapDir);

    uint8_t costAt(int x, int y, int z);
    bool isWalkable(int x, int y, int z) { return costAt(x, y, z) <= MaxCost; }

    static constexpr uint8_t MaxCost = 250;
    static constexpr uint8_t UnknownCost = 0xfe;
    static constexpr uint8_t BlockedCost = 0xff;

private:
    const QImage& diskTile(const Position& id);

    std::unordered_map<uint64_t, QImage> m_loaded;
    std::unordered_map<uint64_t, QImage> m_disk;
    QString m_dir;
};

/// Snapshot Soft in-memory tiles + disk dir for off-thread pathfinding (CIP
/// TWaypointCostMap feed into the movement path planner).
std::shared_ptr<TWaypointCosts> snapshotWaypointCosts(TMinimapTileStorage* storage);

}
