#pragma once

/// CIP-neutral pack helpers over tip `g_map` — path / walk / aware / use / sight peels.
/// Header must not include `tibia/world/map.h`, `TWorldMapField`, or `client/position.h`
/// (position.h pulls framework into pack_sdk).

#include <cstdint>
#include <vector>

namespace tibia::game {

/// Tiny POD so packs avoid tip `Position` / framework.
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

/// Aware viewport margins (matches tip `AwareRange` left/top/right/bottom).
struct MapAwareRange {
    uint8_t left = 0;
    uint8_t top = 0;
    uint8_t right = 0;
    uint8_t bottom = 0;
};

/// POD peel of `TWorldMapField::TFieldItemHit` (ladder / rope / use — no ThingPtr).
struct MapUseHit {
    bool ok = false;
    uint16_t itemId = 0;
    int stackPos = -1;
};

/// POD peel of `Map::TItemHit` (`typeId` matches tip appearance id — no ItemPtr).
struct MapItemHit {
    MapPos pos;
    uint16_t typeId = 0;
    int stackPos = -1;
};

/// Sync Travel-mode path probe (`Map::canTravelPath`).
bool canTravelPath(MapPos from, MapPos to, int maxComplexity = 8000);

/// Null-safe `getTile(pos)->isWalkable(ignoreCreatures)` — false when tile missing.
bool isWalkable(MapPos pos, bool ignoreCreatures = true);

MapAwareRange awareRange();

/// Empty when tile missing. Matches `TWorldMapField::itemHits` (non-creature item rows).
std::vector<MapUseHit> itemHits(MapPos pos);

/// `ok=false` when tile missing or no use target. Matches `TWorldMapField::topUseHit`.
MapUseHit topUseHit(MapPos pos);

/// Matches `Map::findItemsInRange` (appearance/`typeId` scan in a rectangle).
std::vector<MapItemHit> findItemsInRange(MapPos center, int rangeX, int rangeY,
                                        const std::vector<uint16_t>& typeIds);

/// Matches `Map::isSightClear`.
bool isSightClear(MapPos from, MapPos to);

} // namespace tibia::game
