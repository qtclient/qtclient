/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once
#include "appearances/appearanceinstance.h"
#include "client/declarations.h"
#include "client/staticdata.h"
#include "framework/core/inputevent.h"

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

namespace tibia::minimap {
class TWaypointCosts;
}

class TileBlock
{
public:
    TileBlock() { m_tiles.fill(nullptr); }

    tibia::worldmap::TWorldMapField* create(const Position& pos);
    tibia::worldmap::TWorldMapField* getOrCreate(const Position& pos);
    tibia::worldmap::TWorldMapField* get(const Position& pos) { return m_tiles[getTileIndex(pos)]; }
    void assign(const Position& pos, tibia::worldmap::TWorldMapField* field) { m_tiles[getTileIndex(pos)] = field; }
    void remove(const Position& pos) { m_tiles[getTileIndex(pos)] = nullptr; }

    uint32_t getTileIndex(const Position& pos) { return ((pos.y % BLOCK_SIZE) * BLOCK_SIZE) + (pos.x % BLOCK_SIZE); }

    const std::array<tibia::worldmap::TWorldMapField*, BLOCK_SIZE* BLOCK_SIZE>& getTiles() const { return m_tiles; }

private:
    std::array<tibia::worldmap::TWorldMapField*, BLOCK_SIZE* BLOCK_SIZE> m_tiles;
};

struct PathFindResult
{
    tibia::client::PathFindResult status = tibia::client::PathFindResultNoWay;
    std::vector<tibia::client::Direction> path;
    int complexity = 0;
    Position start;
    Position destination;
};
using PathFindResult_ptr = std::shared_ptr<PathFindResult>;

struct Node
{
    float cost;
    float totalCost;
    Position pos;
    Node* prev;
    int distance;
    int unseen;
};

/// Engine-side listener for the fields the server describes (the Qt client's
/// minimap). Called on the parsing thread, once per field object change.
class MapObserver
{
public:
    virtual ~MapObserver() = default;
    /// An item or creature was added to or removed from the field, or the
    /// field was cleaned by a description. Not called for fields dropped
    /// because they left the aware range.
    virtual void onTileChanged(const Position& pos) = 0;
    /// A map description or move message is fully parsed (central position set).
    /// `fullDescription`: the whole window of the central floor was described
    /// (a map or central-floor description, or a floor change), not a step.
    virtual void onWindowChanged(bool fullDescription) = 0;
};

//@bindsingleton g_map
class Map
{
public:
    void init();
    void terminate();

    void notificateTileUpdate(const Position& pos, const ThingPtr& thing, tibia::client::Operation operation);
    void notificateWindowChanged(bool fullDescription) const;
    void notificateCameraMove(const Point& offset) const;
    /// CIP TWorldmapProtocolMessageHandler + minimap feed both listen.
    void addObserver(MapObserver* observer);
    void removeObserver(MapObserver* observer);
    /// Legacy single-slot: clears the list then adds `observer` (or clears if null).
    void setObserver(MapObserver* observer);
    /// Diagnostic counter: opaque add/remove / field clean. Cover *caches* are
    /// cleared per-cell via invalidateCoverCachesFrom — not by this revision.
    uint32_t getCoverRevision() const { return m_coverRevision.load(std::memory_order_relaxed); }
    /// Drop isCompletelyCovered / isCovered bits for tiles that look up through `pos`.
    void invalidateCoverCachesFrom(const Position& pos);
    /// One worldmap build: memoize opaque / top-ground probes shared across FOV cover walks.
    void beginCoverProbeBatch();
    void endCoverProbeBatch();
    /// Bumped when an item is added to or removed from a field, or a field is cleaned.
    uint32_t getItemRevision() const { return m_itemRevision.load(std::memory_order_relaxed); }
    /// Creatures, effects, and missiles added or removed. Walk offsets do not bump it.
    uint32_t getDynamicRevision() const { return m_dynamicRevision.load(std::memory_order_relaxed); }
    /// Drop pending static dirty (cover-flood coalescing counter). Full emit remits
    /// the whole FOV, so positions are not consumed by the renderer.
    /// Returns how many were discarded (for PERF).
    size_t clearStaticDirtyTiles();
    void notificateKeyRelease(const InputEvent& inputEvent) const;


    void setWidth(const uint16_t w) { m_width = w; }
    void setHeight(const uint16_t h) { m_height = h; }
    Size getSize() { return { m_width, m_height }; }

    void clean();
    void cleanDynamicThings();
    void cleanTexts() { m_animatedTexts.clear(); m_staticTexts.clear(); }

    // thing related
    ThingPtr getThing(const Position& pos, int16_t stackPos);
    void addThing(const ThingPtr& thing, const Position& pos, int16_t stackPos = -1);
    /// Place a protocol-decoded item appearance (no ephemeral Item).
    bool placeAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& pos,
                         int16_t stackPos = -1);
    bool removeThing(const ThingPtr& thing);
    bool removeThingByPos(const Position& pos, int16_t stackPos);

    void addStaticText(const StaticTextPtr& txt, const Position& pos);
    bool removeStaticText(const StaticTextPtr& txt);

    void addAnimatedText(const AnimatedTextPtr& txt, const Position& pos);
    bool removeAnimatedText(const AnimatedTextPtr& txt);

    StaticTextPtr getStaticText(const Position& pos) const;

    // field / tile related
    tibia::worldmap::TWorldMapField* createTile(const Position& pos);
    template <typename... Items>
    tibia::worldmap::TWorldMapField* createTileEx(const Position& pos, const Items&... items);
    tibia::worldmap::TWorldMapField* getOrCreateTile(const Position& pos);
    tibia::worldmap::TWorldMapField* getTile(const Position& pos);
    /// CipSoft naming — same as getTile / getOrCreateTile.
    tibia::worldmap::TWorldMapField* fieldAt(const Position& pos) { return getTile(pos); }
    tibia::worldmap::TWorldMapField* getOrCreateField(const Position& pos) { return getOrCreateTile(pos); }
    WorldMapFieldList getTiles(int8_t floor = -1);
    void setFieldInBlock(const Position& pos, tibia::worldmap::TWorldMapField* field);
    void cleanTile(const Position& pos);

    void beginGhostMode(float opacity);
    void endGhostMode();

    /// CIP-shaped map item scan hit (appearance/typeId + stack index — no ItemPtr).
    struct TItemHit
    {
        Position pos;
        uint16_t typeId = 0;
        int stackPos = -1;
    };

    stdext::map<Position, uint16_t, Position::Hasher> findItemsById(uint16_t clientId, uint32_t max);
    std::vector<TItemHit> findItemsInRange(const Position& center, int rangeX, int rangeY,
                                           const std::vector<uint16_t>& clientIds);

    /// Session LocalPlayer only — no Map creature registry (Phase 5.0-B).
    CreaturePtr getCreatureById(uint32_t id);

    /// Field ops by creature id. Prefer `fieldPos` (CIP Move/Change/DeleteOnMap
    /// coordinate) — storage position alone desyncs and leaves outfit trails.
    bool removeCreatureFromFields(uint32_t creatureId, const Position& fieldPos = {});
    bool placeCreatureId(uint32_t creatureId, const Position& pos, int16_t stackPos = -1);

    /// Shared MoveCreature apply (classic parseCreatureMove + native H).
    /// `byId` true when wire used 0xffff+creature_id (no from/stack scrub first).
    /// Returns false when creatureId unresolved or remove failed for pos+stack form.
    bool applyCreatureMove(uint32_t creatureId, const Position& oldPos, int oldStack,
                           const Position& newPos, bool byId);

    /// Shared MoveCreature apply (classic `parseCreatureMove` + native H peel).
    /// `oldPos`/`oldStack` are wire source (invalid when mapped-thing is by-id);
    /// `byId` is the creature id when the mapped-thing header used 0xffff+id.
    void applyCreatureMove(uint32_t creatureId, const Position& oldPos, int oldStack, uint32_t byId,
                           const Position& newPos);

    std::vector<CreaturePtr> getSpectators(const Position& centerPos, const bool multiFloor)
    {
        return getSpectatorsInRangeEx(centerPos, multiFloor, m_awareRange.left, m_awareRange.right, m_awareRange.top, m_awareRange.bottom);
    }

    std::vector<CreaturePtr> getSightSpectators(const Position& centerPos, const bool multiFloor)
    {
        return getSpectatorsInRangeEx(centerPos, multiFloor, m_awareRange.left - 1, m_awareRange.right - 2, m_awareRange.top - 1, m_awareRange.bottom - 2);
    }

    std::vector<CreaturePtr> getSpectatorsInRange(const Position& centerPos, const bool multiFloor, const int32_t xRange, const int32_t yRange)
    {
        return getSpectatorsInRangeEx(centerPos, multiFloor, xRange, xRange, yRange, yRange);
    }

    std::vector<CreaturePtr> getSpectatorsInRangeEx(const Position& centerPos, bool multiFloor, int32_t minXRange, int32_t maxXRange, int32_t minYRange, int32_t maxYRange);

    void setLight(const Light& light);

    void setCentralPosition(const Position& centralPosition);

    bool isLookPossible(const Position& pos);
    bool isCovered(const Position& pos, uint8_t firstFloor = 0) {
        bool isLoading = false;
        return isCovered(pos, isLoading, firstFloor);
    }

    bool isCompletelyCovered(const Position& pos, uint8_t firstFloor = 0) {
        bool isLoading = false;
        return isCompletelyCovered(pos, isLoading, firstFloor);
    }

    bool isCovered(const Position& pos, bool& isLoading, uint8_t firstFloor = 0);
    bool isCompletelyCovered(const Position& pos, bool& isLoading, uint8_t firstFloor = 0);
    /// Wipe every resident tile's cover bits (stream-flood coalesce).
    void invalidateAllCoverCaches();
    bool isAwareOfPosition(const Position& pos) const { return isAwareOfPosition(pos, m_awareRange); }
    bool isAwareOfPosition(const Position& pos, const AwareRange& awareRange) const;

    void resetLastCamera() const;

    void setAwareRange(const AwareRange& range);
    void resetAwareRange();
    AwareRange getAwareRange() const { return m_awareRange; }

    Light getLight() const { return m_light; }
    Position getCentralPosition() { return m_centralPosition; }
    uint8_t getFirstAwareFloor() const;
    uint8_t getLastAwareFloor() const;
    const std::vector<tibia::appearances::TAppearanceInstancePtr>& getFloorMissiles(const uint8_t z)
    {
        return m_floors[z].missileAppearances;
    }
    void addMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance);
    bool removeMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance);
    /// Protocol magic effect → field `m_effectAppearances` (no Effect Thing).
    void addEffectAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& pos);

    std::vector<AnimatedTextPtr> getAnimatedTexts() { return m_animatedTexts; }
    std::vector<StaticTextPtr> getStaticTexts() { return m_staticTexts; }

    std::tuple<std::vector<tibia::client::Direction>, tibia::client::PathFindResult> findPath(const Position& start, const Position& goal,
                                                                          int maxComplexity, int flags = 0);
    /// `pathMode`: Minimap (strict qtclient), Cyclopedia (qtclient 0xfe fog), Travel (fog + soft blockers).
    PathFindResult_ptr newFindPath(const Position& start, const Position& goal,
                                   const std::shared_ptr<std::list<Node*>>& visibleNodes,
                                   const std::shared_ptr<tibia::minimap::TWaypointCosts>& softCosts = nullptr,
                                   tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap,
                                   int maxComplexity = 50000);
    void findPathAsync(const Position& start, const Position& goal,
                       const std::function<void(PathFindResult_ptr)>& callback,
                       tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);
    /// Sync Travel-mode qtclient probe (same Indexed8 costs as findPathAsync). Used by
    /// TravelRouter to skip path-unreachable hubs before arming autoWalk.
    /// ``maxComplexity`` caps A* expansion so path-probe cannot freeze the GUI thread.
    bool canTravelPath(const Position& start, const Position& goal, int maxComplexity = 8000);

    /// pathfinder avoid set (CIP minimap avoid): skipped by newFindPath.
    void setPathAvoid(const Position& pos, bool avoid);
    void clearPathAvoids();
    bool isPathAvoided(const Position& pos) const;

    /// Indexed8 waypoint snapshot factory (CIP TWaypointCostMap). Set from
    /// TGameClient while the CipSoft minimap is live; cleared on world leave.
    using WaypointCostFactory = std::function<std::shared_ptr<tibia::minimap::TWaypointCosts>()>;
    void setWaypointCostFactory(WaypointCostFactory factory);
    void clearWaypointCostFactory();

    void setFloatingEffect(const bool enable) { m_floatingEffect = enable; }
    bool isDrawingFloatingEffects() { return m_floatingEffect; }

    std::map<std::string, std::tuple<int, int, int, std::string>> findEveryPath(const Position& start, int maxDistance, const std::map<std::string, std::string>& params);
    std::vector<CreaturePtr> getSpectatorsByPattern(const Position& centerPos, const std::string& pattern, tibia::client::Direction direction);

    int getMinimapColor(const Position& pos);
    bool isSightClear(const Position& fromPos, const Position& toPos);

private:
    struct FloorData
    {
        std::vector<tibia::appearances::TAppearanceInstancePtr> missileAppearances;
        std::unordered_map<uint32_t, TileBlock > tileBlocks;
    };

    void removeUnawareThings();
    /// Packed cover-probe flags for one build (see beginCoverProbeBatch).
    uint8_t probeCoverTile(const Position& pos);

    uint16_t getBlockIndex(const Position& pos) { return ((pos.y / BLOCK_SIZE) * (65536 / BLOCK_SIZE)) + (pos.x / BLOCK_SIZE); }

    std::vector<FloorData> m_floors;

    std::vector<AnimatedTextPtr> m_animatedTexts;
    std::vector<StaticTextPtr> m_staticTexts;


    uint16_t m_width{ 0 };
    uint16_t m_height{ 0 };

    Light m_light;
    Position m_centralPosition;

    AwareRange m_awareRange;

    std::vector<MapObserver*> m_observers;
    bool m_removingUnawareThings{ false };
    std::atomic<uint32_t> m_coverRevision{ 0 };
    std::atomic<uint32_t> m_itemRevision{ 0 };
    std::atomic<uint32_t> m_dynamicRevision{ 0 };
    std::vector<Position> m_staticDirty;
    static constexpr size_t MaxStaticDirty = 4096;
    /// Opaque stream hit MaxStaticDirty — defer one full cover wipe instead of N column walks.
    bool m_coverInvalidateFlood{ false };

    /// Frame-local memo: seen|empty|opaque|topGround|loading (see map.cpp).
    bool m_coverProbeActive{ false };
    std::unordered_map<Position, uint8_t, Position::Hasher> m_coverProbe;

    bool m_floatingEffect{ true };

    /// pathfinder avoid tiles for newFindPath (CIP waypoint/avoid pathfinder subset).
    std::unordered_set<Position, Position::Hasher> m_pathAvoids;
    WaypointCostFactory m_waypointCostFactory;
};

extern Map g_map;
