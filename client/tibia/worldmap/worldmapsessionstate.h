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

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

#include "worldmap/mapobserver.h"
#include <tibia/schedule/schedule.h>
#include "worldmap/pathfindtypes.h"

namespace tibia {
namespace minimap {
class TWaypointCosts;
}
}



/// Session overlay on `TWorldMapStorage`: pathfind, cover probe/revisions, observers,
/// static/dynamic/text overlays, per-floor missiles, aware range, central/light, pathAvoids.
/// Spatial fields live on storage — field mutates belong on `TWorldMapStorage` call sites.
class TWorldMapSessionState
{
public:
    void init();
    /// Resize per-floor missile vectors only (safe from TWorldMapStorage ctor).
    void ensureMissileFloorsSized();
    void terminate();

    /// A field changed (`creature`: a creature moved on it, not the field content).
    void notificateTileUpdate(const Position& pos, bool creature, tibia::client::Operation operation);
    void notificateCameraMove(const QPoint& offset) const;
    /// CIP TWorldmapProtocolMessageHandler + minimap feed both listen.
    void addObserver(MapObserver* observer);
    void removeObserver(MapObserver* observer);
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


    void setWidth(const uint16_t w) { m_width = w; }
    void setHeight(const uint16_t h) { m_height = h; }
    QSize getSize() { return { m_width, m_height }; }

    void clean();
    void cleanDynamicThings();
    void cleanTexts() { m_animatedTexts.clear(); m_staticTexts.clear(); }

    // thing related
    /// Place a protocol-decoded item appearance (no ephemeral Item).
    bool placeAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& pos,
                         int16_t stackPos = -1);
    void addStaticText(const StaticTextPtr& txt, const Position& pos);
    bool removeStaticText(const StaticTextPtr& txt);

    void addAnimatedText(const AnimatedTextPtr& txt, const Position& pos);
    bool removeAnimatedText(const AnimatedTextPtr& txt);
    // field / tile related
    tibia::worldmap::TWorldMapField* getOrCreateTile(const Position& pos);
    /// Prefer `tibia::worldmap::fieldAt` at call sites (storage hash, then spatial fallback).
    tibia::worldmap::TWorldMapField* getTile(const Position& pos);
    /// Spatial index slot only — does not consult `TWorldMapStorage::m_fields`.
    /// Legacy Map naming — delegates to `TWorldMapStorage` (not `getTile`).
    tibia::worldmap::TWorldMapField* fieldAt(const Position& pos);
    tibia::worldmap::TWorldMapField* getOrCreateField(const Position& pos);
    WorldMapFieldList getTiles(int8_t floor = -1);
    void cleanTile(const Position& pos);
    /// CIP-shaped map item scan hit (appearance/typeId + stack index — no ItemPtr).
    struct TItemHit
    {
        Position pos;
        uint16_t typeId = 0;
        int stackPos = -1;
    };
    std::vector<TItemHit> findItemsInRange(const Position& center, int rangeX, int rangeY,
                                           const std::vector<uint16_t>& clientIds);
    /// Takes the creature off the field `fieldPos` names, else off the field it
    /// stands on. `fireLocalDisappear`: DeleteOnMap and unaware fields only.
    bool removeCreatureFromFields(uint32_t creatureId, const Position& fieldPos = {},
                                  bool fireLocalDisappear = true);
    bool placeCreatureId(uint32_t creatureId, const Position& pos, int16_t stackPos = -1);

    /// MoveCreature. `byId`: the message names the creature instead of its field and slot.
    bool applyCreatureMove(uint32_t creatureId, const Position& oldPos, int oldStack,
                           const Position& newPos, bool byId);
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
    /// The local player's field as the map last placed it; its creature's
    /// position takes over once the creature stands somewhere.
    Position localPlayerPosition() const { return m_localPlayerPosition; }
    /// A new character session: the player is off the map and has no field yet.
    void resetLocalPlayer();
    /// The map placed the local player on `pos` (onPositionChange when it moved).
    void setLocalPlayerPosition(const Position& pos);

    void setAwareRange(const AwareRange& range);
    void resetAwareRange();
    AwareRange getAwareRange() const { return m_awareRange; }

    Light getLight() const { return m_light; }
    Position getCentralPosition() const { return m_centralPosition; }
    uint8_t getFirstAwareFloor() const;
    uint8_t getLastAwareFloor() const;
    const std::vector<tibia::appearances::TAppearanceInstancePtr>& getFloorMissiles(uint8_t z) const;
    void addMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance);
    /// Protocol magic effect → field `m_effectAppearances` (no Effect Thing).
    void addEffectAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& pos);
    std::tuple<std::vector<tibia::client::Direction>, tibia::client::PathFindResult> findPath(const Position& start, const Position& goal,
                                                                          int maxComplexity, int flags = 0);
    /// `pathMode`: Minimap (strict), Cyclopedia/Travel (CIP 0xfe fog; Travel also 0xff goals).
    PathFindResult_ptr newFindPath(const Position& start, const Position& goal,
                                   const std::shared_ptr<std::list<Node*>>& visibleNodes,
                                   const std::shared_ptr<tibia::minimap::TWaypointCosts>& softCosts = nullptr,
                                   tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap,
                                   int maxComplexity = 50000);
    void findPathAsync(const Position& start, const Position& goal,
                       const std::function<void(PathFindResult_ptr)>& callback,
                       tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);
    /// Sync Travel path probe (CIP auto-path / AutoWalkPathTravel). Used by
    /// TravelRouter before arming autoWalk. ``maxComplexity`` caps A* expansion.
    bool canTravelPath(const Position& start, const Position& goal, int maxComplexity = 8000);

    /// CIP pathfinder avoid set (marks these 0xff): skipped by newFindPath.
    void setPathAvoid(const Position& pos, bool avoid);
    void clearPathAvoids();
    bool isPathAvoided(const Position& pos) const;

    /// Indexed8 waypoint snapshot factory (CIP TWaypointCostMap). Set from
    /// TGameClient while the CipSoft minimap is live; cleared on world leave.
    using WaypointCostFactory = std::function<std::shared_ptr<tibia::minimap::TWaypointCosts>()>;
    /// Minimap waypoint costs for the movement path planner (null without a minimap).
    std::shared_ptr<tibia::minimap::TWaypointCosts> waypointCosts() const
    {
        return m_waypointCostFactory ? m_waypointCostFactory() : nullptr;
    }
    void setWaypointCostFactory(WaypointCostFactory factory);
    void clearWaypointCostFactory();
    int getMinimapColor(const Position& pos);
    bool isSightClear(const Position& fromPos, const Position& toPos);

private:
    struct FloorData
    {
    };

    void removeUnawareThings();
    /// The local player's creature went onto a field: a field it did not
    /// step to from the last one ends the motion and the walk goal.
    void localPlayerAppeared();
    /// The local player's creature left its field; unless it reappears before
    /// the event loop turns, it counts as off the map and its step stops.
    void localPlayerDisappeared();
    /// Packed cover-probe flags for one build (see beginCoverProbeBatch).
    uint8_t probeCoverTile(const Position& pos);

    std::vector<FloorData> m_floors;

    Position m_localPlayerPosition;
    /// The field the player left (the jump test); cleared once it stays off the map.
    Position m_localPlayerLastField;
    bool m_localPlayerOffMap{ true };
    tibia::schedule::TScheduledTaskPtr m_localPlayerLeave;

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

    /// Frame-local memo: seen|empty|opaque|topGround|loading (see worldmapsessionstate.cpp).
    bool m_coverProbeActive{ false };
    std::unordered_map<Position, uint8_t, Position::Hasher> m_coverProbe;


    /// pathfinder avoid tiles for newFindPath (CIP waypoint/avoid pathfinder subset).
    std::unordered_set<Position, Position::Hasher> m_pathAvoids;
    WaypointCostFactory m_waypointCostFactory;
};

