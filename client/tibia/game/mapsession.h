#pragma once

/// Pack-ABI map peels over the current session's `TWorldMapStorage`: walk, use,
/// sight and the IdleOT travel path probe. Called from `mods/host_api_v1.cpp`
/// (and the few host callers that want POD positions).
/// Header must not include `worldmapstorage.h`, `TWorldMapField`, or `client/position.h`
/// (position.h pulls framework into pack_sdk).

#include <client/const.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace tibia {
namespace minimap {
class TWaypointCosts;
}
namespace game {

/// Tiny POD so packs avoid qtclient `Position` / framework.
struct MapPos {
    int x = 0;
    int y = 0;
    int z = 0;

    friend bool operator==(const MapPos& a, const MapPos& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    friend bool operator!=(const MapPos& a, const MapPos& b) { return !(a == b); }
};

/// Aware viewport margins (matches qtclient `AwareRange` left/top/right/bottom).
struct MapAwareRange {
    uint8_t left = 0;
    uint8_t top = 0;
    uint8_t right = 0;
    uint8_t bottom = 0;

    uint8_t horizontal() const { return static_cast<uint8_t>(left + right + 1); }
    uint8_t vertical() const { return static_cast<uint8_t>(top + bottom + 1); }
};

/// POD peel of `TWorldMapField::TFieldItemHit` (ladder / rope / use — no ThingPtr).
struct MapUseHit {
    bool ok = false;
    uint16_t itemId = 0;
    int stackPos = -1;
};

/// Sync travel path probe (AutoWalkPathTravel): the IdleOT travel router asks
/// it before arming an auto walk. `maxComplexity` caps the A* expansion.
bool canTravelPath(MapPos from, MapPos to, int maxComplexity = 8000);

/// Async travel path probe (AutoWalkPathTravel) on a worker thread.
/// Live blockers (fire, splash, parcels) fail when aware — no soft-blocker bypass.
/// Callback runs on the GUI thread via schedule::post.
void canTravelPathAsync(MapPos from, MapPos to, const std::function<void(bool ok)>& callback);

/// Null-safe `fieldAt(pos)->isWalkable(ignoreCreatures)` — false when the field is missing.
bool isWalkable(MapPos pos, bool ignoreCreatures = true);

/// The current session's aware range.
MapAwareRange awareRange();

/// Empty when the field is missing. Matches `TWorldMapField::itemHits` (non-creature item rows).
std::vector<MapUseHit> itemHits(MapPos pos);

/// `ok=false` when the field is missing or has no use target. Matches `TWorldMapField::topUseHit`.
MapUseHit topUseHit(MapPos pos);

/// Line-of-sight between two map positions.
bool isSightClear(MapPos from, MapPos to);

/// Fields the travel path finder keeps off (`TWorldMapStorage` path avoids).
void setPathAvoid(MapPos pos, bool avoid);
void clearPathAvoids();

/// Minimap waypoint costs for the travel path finder (CIP TWaypointCostMap):
/// set while the CipSoft minimap is live, cleared on world leave.
using WaypointCostFactory = std::function<std::shared_ptr<tibia::minimap::TWaypointCosts>()>;
void setWaypointCostFactory(WaypointCostFactory factory);
void clearWaypointCostFactory();

}
}
