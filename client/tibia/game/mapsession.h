#pragma once

/// CIP-neutral pack helpers — path / walk / aware / use / sight peels over `TWorldMapStorage`.
/// Header must not include `worldmapsessionstate.h`, `TWorldMapField`, or `client/position.h`
/// (position.h pulls framework into pack_sdk).

#include <client/const.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

struct PathFindResult;
using PathFindResult_ptr = std::shared_ptr<PathFindResult>;

class MapObserver;

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

/// World ambient light (matches qtclient `Light` on Map).
struct MapWorldLight {
    uint8_t intensity = 0;
    uint8_t color = 0;
};

/// POD peel of `TWorldMapField::TFieldItemHit` (ladder / rope / use — no ThingPtr).
struct MapUseHit {
    bool ok = false;
    uint16_t itemId = 0;
    int stackPos = -1;
};

/// POD peel of session item scan hits (`typeId` matches qtclient appearance id — no ItemPtr).
struct MapItemHit {
    MapPos pos;
    uint16_t typeId = 0;
    int stackPos = -1;
};

/// Sync CIP auto-path probe (`canTravelPath` / AutoWalkPathTravel).
bool canTravelPath(MapPos from, MapPos to, int maxComplexity = 8000);

/// Async CIP auto-path probe (`findPathAsync` + AutoWalkPathTravel).
/// Live blockers (fire, splash, parcels) fail when aware — no soft-blocker bypass.
/// Callback runs on the GUI thread via schedule::post.
void canTravelPathAsync(MapPos from, MapPos to, const std::function<void(bool ok)>& callback);

/// Null-safe `getTile(pos)->isWalkable(ignoreCreatures)` — false when tile missing.
bool isWalkable(MapPos pos, bool ignoreCreatures = true);

/// True only when the field exists and is not walkable. Missing field → false.
bool blocksWalk(MapPos pos, bool ignoreCreatures = true);

/// Empty when tile missing. Matches `TWorldMapField::walkingCreatureIds`.
std::vector<uint32_t> walkingCreatureIds(MapPos pos);

MapAwareRange awareRange();
void setAwareRange(MapAwareRange range);
void resetAwareRange();

/// Central map position as MapPos.
MapPos centralPosition();
void setCentralPosition(MapPos pos);

MapWorldLight worldLight();
/// Idempotent when unchanged; emits `onWorldLightChange` when intensity/color differ.
void applyWorldLight(uint8_t intensity, uint8_t color);

bool isAwareOf(MapPos pos);

/// Empty when tile missing. Matches `TWorldMapField::itemHits` (non-creature item rows).
std::vector<MapUseHit> itemHits(MapPos pos);

/// `ok=false` when tile missing or no use target. Matches `TWorldMapField::topUseHit`.
MapUseHit topUseHit(MapPos pos);

/// Appearance/`typeId` scan in a rectangle.
std::vector<MapItemHit> findItemsInRange(MapPos center, int rangeX, int rangeY,
                                        const std::vector<uint16_t>& typeIds);

/// Line-of-sight between two map positions.
bool isSightClear(MapPos from, MapPos to);

/// Cover-cache invalidation counter.
uint32_t coverRevision();
/// Drain static dirty tiles for cover-flood coalescing.
void clearStaticDirtyTiles();
/// Share opaque/top-ground probes across one FOV cover walk.
void beginCoverProbeBatch();
void endCoverProbeBatch();

/// CIP pathfinder avoid set (`setPathAvoid` / `clearPathAvoids`).
void setPathAvoid(MapPos pos, bool avoid);
void clearPathAvoids();

/// Async GoPath / auto-walk (`findPathAsync`).
void findPathAsync(MapPos from, MapPos to, const std::function<void(PathFindResult_ptr)>& callback,
                   tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);

struct MapPoint {
    int x = 0;
    int y = 0;
};

void addMapObserver(::MapObserver* observer);
void removeMapObserver(::MapObserver* observer);

void notifyCameraMove(MapPoint offset);

void cleanMap();
void cleanMapDynamicThings();

uint8_t firstAwareFloor();
uint8_t lastAwareFloor();

uint32_t itemRevision();
uint32_t dynamicRevision();

bool isCompletelyCovered(MapPos pos, bool& isLoading, uint8_t firstFloor = 0);
bool isCovered(MapPos pos, bool& isLoading, uint8_t firstFloor = 0);

using WaypointCostFactory = std::function<std::shared_ptr<tibia::minimap::TWaypointCosts>()>;
void setWaypointCostFactory(WaypointCostFactory factory);
void clearWaypointCostFactory();
/// Snapshot of the minimap waypoint costs (null without a minimap).
std::shared_ptr<tibia::minimap::TWaypointCosts> waypointCosts();

void initMap();
void terminateMap();

}
}
