#include "mapsession.h"

#include <client/position.h>
#include <QThreadPool>
#include <tibia/schedule/schedule.h>
#include <tibia/worldmap/worldmapstorage.h>
#include "minimap/waypointcosts.h"
#include "worldmap/worldmapfield.h"

#include <algorithm>
#include <cstdlib>
#include <list>
#include <queue>
#include <unordered_map>

namespace tibia::game {
namespace {

using tibia::worldmap::TWorldMapField;
using tibia::worldmap::TWorldMapStorage;

Position toPosition(const MapPos& p)
{
    return Position(p.x, p.y, static_cast<uint8_t>(p.z));
}

MapUseHit fromFieldHit(const TWorldMapField::TFieldItemHit& hit)
{
    if (!hit.ok())
        return {};
    return { true, hit.typeId, hit.stackPos };
}

/// The current session's map; between sessions the storage's stand-in.
TWorldMapStorage& storage()
{
    return *TWorldMapStorage::ensureStorage();
}

/// The stored field at a map position (null off the map).
TWorldMapField* mapFieldAt(TWorldMapStorage& map, const Position& pos)
{
    return pos.isMapPosition() ? map.fieldAt(pos) : nullptr;
}

// --- Travel path finder (the IdleOT travel router's path probe) ---

struct PathFindResult
{
    tibia::client::PathFindResult status = tibia::client::PathFindResultNoWay;
    std::vector<tibia::client::Direction> path;
    int complexity = 0;
    Position start;
    Position destination;
};
using PathFindResultPtr = std::shared_ptr<PathFindResult>;

struct Node
{
    float cost;      // Indexed8 / ground tile cost (CIP TNavigationNode[4])
    float totalCost; // f = g + h (CIP [6]; heap key)
    float pathCost;  // g from start (CIP [3])
    Position pos;
    Node* prev;
    int distance;
    int unseen;
};

std::shared_ptr<tibia::minimap::TWaypointCosts> resolveSoftCosts(
    const std::shared_ptr<tibia::minimap::TWaypointCosts>& soft, const TWorldMapStorage::WaypointCostFactory& factory)
{
    if (soft)
        return soft;
    if (factory)
        return factory();
    // Disk-only Indexed8 (AppData/minimap) when the CIP session factory is cleared.
    return tibia::minimap::snapshotWaypointCosts(nullptr);
}

/// `pathMode`: Minimap (strict), Cyclopedia/Travel (CIP 0xfe fog; Travel also 0xff goals).
PathFindResultPtr findTravelPath(TWorldMapStorage& map, const Position& start, const Position& goal,
                                 const std::shared_ptr<std::list<Node*>>& visibleNodes,
                                 const std::shared_ptr<tibia::minimap::TWaypointCosts>& softCosts,
                                 const tibia::client::AutoWalkPathMode pathMode, const int maxComplexity = 50000)
{
    auto ret = std::make_shared<PathFindResult>();
    ret->start = start;
    ret->destination = goal;

    if (start == goal) {
        ret->status = tibia::client::PathFindResultSamePosition;
        return ret;
    }

    if (goal.z != start.z) {
        return ret;
    }

    // CIP : |dx|/|dy| ≤ 0x6e (110) or fail.
    constexpr int kCipPathWindow = 0x6e;
    if (std::abs(goal.x - start.x) > kCipPathWindow || std::abs(goal.y - start.y) > kCipPathWindow) {
        return ret;
    }

    const bool allowUnknownSoft = pathMode != tibia::client::AutoWalkPathMinimap;
    // CIP / : 0xff neighbors impassable; 0xfe fog walkable when
    // allowUnknownSoft (raw cost 254). Travel may still *goal* on Indexed8 0xff hub/climb stands.
    const auto costs = resolveSoftCosts(softCosts, map.waypointCostFactory());

    // Goal walkability: live aware tile first, else Indexed8 costs (CIP client minimap).
    // Cyclopedia/Travel: path-unknown (0xfe) goals OK; Travel also CipSoft-blocked (0xff) goals.
    // Live fire / splash / parcels: hard-block when aware (no soft-blocker bypass).
    if (map.isAwareOfPosition(goal)) {
        const auto& goalTile = mapFieldAt(map, goal);
        if (!goalTile || !goalTile->isWalkable())
            return ret;
    } else {
        const uint8_t goalCost = costs->costAt(goal.x, goal.y, goal.z);
        if (goalCost <= tibia::minimap::TWaypointCosts::MaxCost) {
            // walkable Indexed8
        } else if (goalCost == tibia::minimap::TWaypointCosts::UnknownCost && allowUnknownSoft) {
            // Cyclopedia/Travel fog goal (CIP treats 0xfe as cost 254)
        } else if (goalCost == tibia::minimap::TWaypointCosts::BlockedCost
            && pathMode == tibia::client::AutoWalkPathTravel) {
            // Travel CipSoft-blocked hub/climb stand
        } else {
            return ret;
        }
    }

    // CIP : min tile cost over ±0x6e window (admissible heuristic scale).
    uint32_t minCost = 0xffffffffu;
    for (int dy = -kCipPathWindow; dy <= kCipPathWindow; ++dy) {
        for (int dx = -kCipPathWindow; dx <= kCipPathWindow; ++dx) {
            const int wx = start.x + dx;
            const int wy = start.y + dy;
            if (wx < 0 || wy < 0)
                continue;
            const uint8_t wc = costs->costAt(wx, wy, start.z);
            if (wc < minCost)
                minCost = wc;
        }
    }
    if (minCost == 0xffffffffu || minCost == 0)
        minCost = 1;

    // CIP h = (manhattan − 1) × minCost + tileCost (0 at goal).
    const auto cipHeuristic = [minCost](const Position& pos, const Position& dest, const float tileCost) -> float {
        const int manh = std::abs(pos.x - dest.x) + std::abs(pos.y - dest.y);
        if (manh <= 0)
            return 0.f;
        return static_cast<float>((manh - 1) * static_cast<int>(minCost)) + tileCost;
    };

    const auto tileCostAt = [&](const Position& pos) -> float {
        const uint8_t wc = costs->costAt(pos.x, pos.y, pos.z);
        if (wc <= tibia::minimap::TWaypointCosts::MaxCost)
            return static_cast<float>(std::max<uint8_t>(1, wc));
        // CIP Indexed8 0xfe stays 254 (stores the raw byte).
        if (wc == tibia::minimap::TWaypointCosts::UnknownCost)
            return static_cast<float>(tibia::minimap::TWaypointCosts::UnknownCost);
        return 1.f;
    };

    struct LessNode
    {
        bool operator()(const Node* a, const Node* b) const
        {
            return b->totalCost < a->totalCost;
        }
    };

    std::unordered_map<Position, Node*, Position::Hasher> nodes;
    std::priority_queue<Node*, std::vector<Node*>, LessNode> searchList;

    if (visibleNodes) {
        for (auto& node : *visibleNodes) {
            // cost < 0: impassable aware-tile sentinel from buildTravelVisibleNodes.
            // Emplacing a real Node here let A* expand through blocked live tiles and
            // the server answered "Sorry, not possible."
            if (node->cost < 0.f) {
                nodes.emplace(node->pos, nullptr);
                delete node;
            } else {
                node->pathCost = 1e9f;
                node->totalCost = 1e9f;
                nodes.emplace(node->pos, node);
            }
        }
    }

    const float startTileCost = tileCostAt(start);
    auto* initNode = new Node{ .cost = startTileCost,
                               .totalCost = cipHeuristic(start, goal, startTileCost),
                               .pathCost = 0.f,
                               .pos = start,
                               .prev = nullptr,
                               .distance = 0,
                               .unseen = 0 };
    nodes[start] = initNode;
    searchList.push(initNode);

    int limit = maxComplexity > 0 ? maxComplexity : 1;
    const int complexityBudget = limit;

    const Node* dstNode = nullptr;
    while (!searchList.empty() && --limit) {
        Node* node = searchList.top();
        searchList.pop();
        if (node->pos == goal) {
            dstNode = node;
            break;
        }
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0)
                    continue;
                Position neighbor = node->pos.translated(i, j);
                if (neighbor.x < 0 || neighbor.y < 0)
                    continue;
                // Stay inside CIP ±0x6e window relative to start.
                if (std::abs(neighbor.x - start.x) > kCipPathWindow
                    || std::abs(neighbor.y - start.y) > kCipPathWindow)
                    continue;
                auto it = nodes.find(neighbor);
                if (it == nodes.end()) {
                    // pathfinder avoid set: treat as impassable (CIP minimap path avoid).
                    if (map.isPathAvoided(neighbor) && neighbor != goal) {
                        it = nodes.emplace(neighbor, nullptr).first;
                    } else {
                        // CIP TWaypointCostMap: 0..250 walkable, 0xfe unknown, 0xff blocked.
                        // Cyclopedia/Travel: 0xfe expensive (CIP cost 254). 0xff impassable.
                        // Live aware tiles still win via visibleNodes.
                        const uint8_t wc = costs->costAt(neighbor.x, neighbor.y, neighbor.z);
                        const bool softFog = wc == tibia::minimap::TWaypointCosts::UnknownCost;
                        const bool passFog = (softFog && allowUnknownSoft) || neighbor == goal;
                        if (wc > tibia::minimap::TWaypointCosts::MaxCost && !passFog) {
                            it = nodes.emplace(neighbor, nullptr).first;
                        } else {
                            const float speed = tileCostAt(neighbor);
                            it = nodes.emplace(neighbor,
                                               new Node{ .cost = speed,
                                                         .totalCost = 1e9f,
                                                         .pathCost = 1e9f,
                                                         .pos = neighbor,
                                                         .prev = node,
                                                         .distance = node->distance + 1,
                                                         .unseen = 0 })
                                     .first;
                        }
                    }
                }
                if (!it->second) // no way
                    continue;

                if (it->second->unseen > 50)
                    continue;

                // CIP : leave current tile —
                // ortho g' = g + tileCost[current]
                // diag g' = g + tileCost[current] + 2×tileCost[current] = g + 3×tileCost
                const bool ortho = (i == 0 || j == 0);
                const float stepCost = ortho ? node->cost : (node->cost * 3.f);
                const float g = node->pathCost + stepCost;
                if (g >= it->second->pathCost)
                    continue;

                it->second->pathCost = g;
                it->second->totalCost = g + cipHeuristic(neighbor, goal, it->second->cost);
                it->second->prev = node;
                if (it->second->unseen)
                    it->second->unseen = node->unseen + 1;
                it->second->distance = node->distance + 1;
                searchList.push(it->second);
            }
        }
    }

    if (dstNode) {
        while (dstNode && dstNode->prev) {
            // Legacy fog unseen-prefix truncate (Indexed8 soft paths keep unseen=0).
            if (dstNode->unseen) {
                ret->path.clear();
            } else {
                ret->path.push_back(dstNode->prev->pos.getDirectionFromPosition(dstNode->pos));
            }
            dstNode = dstNode->prev;
        }
        std::reverse(ret->path.begin(), ret->path.end());
        ret->status = tibia::client::PathFindResultOk;
    }
    ret->complexity = complexityBudget - limit;

    for (const auto& node : nodes) {
        delete node.second;
    }

    return ret;
}

std::shared_ptr<std::list<Node*>> buildTravelVisibleNodes(TWorldMapStorage& map, const Position& start,
                                                          const Position& goal)
{
    // CIP auto-path: aware tiles use live walkable/pathable (fire, splash, parcels
    // block when in view). No soft-blocker bypass — path fails → SnapBack avoid /
    // TravelRouter replan or stop.
    auto visibleNodes = std::make_shared<std::list<Node*>>();
    for (const auto& tile : map.fieldList(static_cast<int8_t>(start.z))) {
        if (tile->getPosition() == start)
            continue;
        const bool isNotWalkable = !tile->isWalkable(false);
        const bool isNotPathable = !tile->isPathable();
        const float baseSpeed = static_cast<float>(tile->getGroundSpeed());
        if ((isNotWalkable || isNotPathable) && tile->getPosition() != goal) {
            // cost < 0 → findTravelPath stores nullptr (must not expand).
            visibleNodes->push_back(new Node{ .cost = -1.f, .totalCost = 0, .pathCost = 0, .pos = tile->getPosition(),
                                              .prev = nullptr, .distance = 0, .unseen = 0 });
        } else {
            visibleNodes->push_back(new Node{ .cost = baseSpeed, .totalCost = 1e9f, .pathCost = 1e9f,
                                              .pos = tile->getPosition(), .prev = nullptr, .distance = 0, .unseen = 0 });
        }
    }
    return visibleNodes;
}

bool isTravelPathFound(const PathFindResultPtr& ret)
{
    return ret
        && (ret->status == tibia::client::PathFindResultOk || ret->status == tibia::client::PathFindResultSamePosition);
}

} // namespace

bool canTravelPath(const MapPos from, const MapPos to, const int maxComplexity)
{
    const Position start = toPosition(from);
    const Position goal = toPosition(to);
    if (!start.isValid() || !goal.isValid())
        return false;
    if (start == goal)
        return true;
    if (start.z != goal.z)
        return false;
    TWorldMapStorage& map = storage();
    const auto visibleNodes = buildTravelVisibleNodes(map, start, goal);
    const TWorldMapStorage::WaypointCostFactory& factory = map.waypointCostFactory();
    const std::shared_ptr<tibia::minimap::TWaypointCosts> softCosts = factory ? factory() : nullptr;
    return isTravelPathFound(
        findTravelPath(map, start, goal, visibleNodes, softCosts, tibia::client::AutoWalkPathTravel, maxComplexity));
}

void canTravelPathAsync(const MapPos from, const MapPos to, const std::function<void(bool ok)>& callback)
{
    if (!callback) {
        return;
    }
    if (from == to) {
        callback(true);
        return;
    }
    if (from.z != to.z) {
        callback(false);
        return;
    }
    const Position start = toPosition(from);
    const Position goal = toPosition(to);
    TWorldMapStorage* map = &storage();
    const auto visibleNodes = buildTravelVisibleNodes(*map, start, goal);
    // CIP costs: snapshot on the GUI thread (tile images), path on a worker.
    const TWorldMapStorage::WaypointCostFactory& factory = map->waypointCostFactory();
    const std::shared_ptr<tibia::minimap::TWaypointCosts> softCosts = factory ? factory() : nullptr;
    QThreadPool::globalInstance()->start([=] {
        const bool ok = isTravelPathFound(
            findTravelPath(*map, start, goal, visibleNodes, softCosts, tibia::client::AutoWalkPathTravel));
        tibia::schedule::post([callback, ok] { callback(ok); });
    });
}

bool isWalkable(const MapPos pos, const bool ignoreCreatures)
{
    if (const TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos)))
        return tile->isWalkable(ignoreCreatures);
    return false;
}

MapAwareRange awareRange()
{
    const AwareRange range = storage().awareRange();
    return { range.left, range.top, range.right, range.bottom };
}

std::vector<MapUseHit> itemHits(const MapPos pos)
{
    const TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos));
    if (!tile)
        return {};
    std::vector<MapUseHit> out;
    for (const auto& hit : tile->itemHits()) {
        if (const MapUseHit peeled = fromFieldHit(hit); peeled.ok)
            out.push_back(peeled);
    }
    return out;
}

MapUseHit topUseHit(const MapPos pos)
{
    const TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos));
    if (!tile)
        return {};
    return fromFieldHit(tile->topUseHit());
}

bool isSightClear(const MapPos from, const MapPos to)
{
    const Position fromPos = toPosition(from);
    const Position toPos = toPosition(to);
    if (fromPos == toPos) {
        return true;
    }

    TWorldMapStorage& map = storage();
    Position start(fromPos.z > toPos.z ? toPos : fromPos);
    const Position destination(fromPos.z > toPos.z ? fromPos : toPos);

    const int8_t mx = start.x < destination.x ? 1 : start.x == destination.x ? 0 : -1;
    const int8_t my = start.y < destination.y ? 1 : start.y == destination.y ? 0 : -1;

    const int32_t A = destination.y - start.y;
    const int32_t B = start.x - destination.x;
    const int32_t C = -(A * destination.x + B * destination.y);

    while (start.x != destination.x || start.y != destination.y) {
        const int32_t move_hor = std::abs(A * (start.x + mx) + B * (start.y) + C);
        const int32_t move_ver = std::abs(A * (start.x) + B * (start.y + my) + C);
        const int32_t move_cross = std::abs(A * (start.x + mx) + B * (start.y + my) + C);

        if (start.y != destination.y && (start.x == destination.x || move_hor > move_ver || move_hor > move_cross)) {
            start.y += my;
        }

        if (start.x != destination.x && (start.y == destination.y || move_ver > move_hor || move_ver > move_cross)) {
            start.x += mx;
        }

        TWorldMapField* tile = mapFieldAt(map, Position(start.x, start.y, start.z));
        if (tile && !tile->isLookPossible()) {
            return false;
        }
    }

    while (start.z != destination.z) {
        TWorldMapField* tile = mapFieldAt(map, Position(start.x, start.y, start.z));
        if (tile && tile->getThingCount() > 0) {
            return false;
        }
        start.z++;
    }

    return true;
}

void setPathAvoid(const MapPos pos, const bool avoid)
{
    storage().setPathAvoid(toPosition(pos), avoid);
}

void clearPathAvoids()
{
    storage().clearPathAvoids();
}

void setWaypointCostFactory(WaypointCostFactory factory)
{
    storage().setWaypointCostFactory(std::move(factory));
}

void clearWaypointCostFactory()
{
    storage().clearWaypointCostFactory();
}

} // namespace tibia::game
