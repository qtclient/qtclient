#pragma once

/// Pack-ABI map peels over the current session's `TWorldMapStorage` (walk, use,
/// sight, stored fields), the minimap's waypoint costs and the Cyclopedia map's
/// static data. Called from `mods/host_api_v1.cpp`
/// (and the few host callers that want POD positions).
/// Header must not include `worldmapstorage.h`, `TWorldMapField`, or `client/position.h`
/// (position.h pulls framework into pack_sdk).

#include <client/const.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
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

/// A stored field of the current session's map as the pack ABI hands it out
/// (`TWorldMapField`: ground speed, walkability with creatures, pathability).
struct MapFieldInfo {
    MapPos pos;
    int groundSpeed = 0;
    bool walkable = false;
    bool pathable = false;
};

/// The stored fields on floor `z` (empty between sessions). GUI thread.
std::vector<MapFieldInfo> storedFieldsOnFloor(int z);

/// The position lies in the current session's aware range.
bool isAwareOfPosition(MapPos pos);

/// Where the minimap's waypoint costs come from (CIP TWaypointCostMap): set by
/// the game client while its minimap is live, cleared on world leave.
using WaypointCostFactory = std::function<std::shared_ptr<tibia::minimap::TWaypointCosts>()>;
void setWaypointCostFactory(WaypointCostFactory factory);
void clearWaypointCostFactory();
/// A snapshot of the minimap's waypoint costs (the live minimap's tiles, else
/// the minimap files on disk). Taken on the GUI thread; one worker thread may
/// read it afterwards.
std::shared_ptr<tibia::minimap::TWaypointCosts> waypointCostsSnapshot();

/// An area or subarea of the Cyclopedia map's static data (map.dat).
struct StaticMapArea {
    uint16_t id = 0;
    bool isSubArea = false;
    /// A subarea's area; 0 for an area.
    uint16_t areaId = 0;
    std::string name;
    std::string alias;
    /// map.dat's label coordinate, else the middle of the area's fields; 0,0,0 for none.
    MapPos label;
    /// The fields a subarea's mask file covers (width / height 0 without a file).
    MapPos maskOrigin;
    int maskWidth = 0;
    int maskHeight = 0;
    bool hasMaskFile = false;
};

/// The first subarea whose mask holds `pos`; 0 for none.
uint16_t staticMapSubAreaAt(MapPos pos);
/// The subarea (`subArea`) or area `id`; false when map.dat has none.
bool staticMapArea(uint16_t id, bool subArea, StaticMapArea& out);
/// The ids of map.dat's areas (`subAreas` false) or subareas, ascending.
std::vector<uint16_t> staticMapAreaIds(bool subAreas);
/// The subareas an area lists, ascending.
std::vector<uint16_t> staticMapSubAreasOfArea(uint16_t areaId);

}
}
