#include <gtest/gtest.h>

#include "input/movementpathplanner.h"

#include <set>
#include <utility>

using namespace tibia::input;
using tibia::creatures::EMovementDirection;
using tibia::worldmap::TWorldMapCoordinate;

namespace {

struct Map
{
    std::set<std::pair<int, int>> walls;
    quint8 ground = 100;
    WaypointCost cost() const
    {
        return [this](const int x, const int y, int) -> quint8 { return walls.count({ x, y }) ? 0xff : ground; };
    }
};

std::vector<EMovementDirection> plan(const Map& map, const TWorldMapCoordinate& from, const TWorldMapCoordinate& to,
                                     int* result = nullptr, bool enterUnwalkable = false)
{
    std::vector<EMovementDirection> path;
    const int r = planMovementPath(from, to, enterUnwalkable, false, map.cost(), {}, path);
    if (result)
        *result = r;
    return path;
}

TWorldMapCoordinate walk(TWorldMapCoordinate at, const std::vector<EMovementDirection>& path)
{
    for (const EMovementDirection d : path) {
        const auto o = tibia::creatures::stepOffset(d);
        at = TWorldMapCoordinate(at.x + o.width, at.y + o.height, at.z);
    }
    return at;
}

}

TEST(MovementPathPlanner, ShortcutsAndErrors)
{
    Map map;
    std::vector<EMovementDirection> path;
    const TWorldMapCoordinate here(1000, 1000, 7);
    EXPECT_EQ(planMovementPath(here, here, false, false, map.cost(), {}, path), PathAlreadyThere);
    EXPECT_EQ(planMovementPath(here, { 1000, 1000, 6 }, false, false, map.cost(), {}, path), PathGoUpstairs);
    EXPECT_EQ(planMovementPath(here, { 1000, 1000, 8 }, false, false, map.cost(), {}, path), PathGoDownstairs);
    EXPECT_EQ(planMovementPath(here, { 1200, 1000, 7 }, false, false, map.cost(), {}, path), PathOutOfRange);
    EXPECT_EQ(planMovementPath(here, { 1001, 1000, 7 }, false, false, map.cost(), {}, path), PathComplete);
    ASSERT_EQ(path.size(), 1u);
    EXPECT_EQ(path[0], EMovementDirection::East);
    EXPECT_EQ(planMovementPath(here, { 1001, 999, 7 }, false, true, map.cost(), {}, path), PathComplete);
    EXPECT_EQ(path[0], EMovementDirection::NorthEast);
}

TEST(MovementPathPlanner, StraightLinePrefersOrthogonalSteps)
{
    Map map;
    int result = 0;
    const auto path = plan(map, { 1000, 1000, 7 }, { 1004, 1000, 7 }, &result);
    EXPECT_EQ(result, PathComplete);
    EXPECT_EQ(path, std::vector<EMovementDirection>(4, EMovementDirection::East));
}

TEST(MovementPathPlanner, WalksAroundWalls)
{
    Map map;
    for (int y = 999; y <= 1001; ++y)
        map.walls.insert({ 1002, y });
    int result = 0;
    const TWorldMapCoordinate from(1000, 1000, 7);
    const TWorldMapCoordinate to(1004, 1000, 7);
    const auto path = plan(map, from, to, &result);
    EXPECT_EQ(result, PathComplete);
    EXPECT_TRUE(walk(from, path) == to);
    TWorldMapCoordinate at = from;
    for (const EMovementDirection d : path) {
        const auto o = tibia::creatures::stepOffset(d);
        at = TWorldMapCoordinate(at.x + o.width, at.y + o.height, at.z);
        EXPECT_FALSE(map.walls.count({ at.x, at.y }));
    }
}

TEST(MovementPathPlanner, FarGoalsComeInPieces)
{
    Map map;
    int result = 0;
    const auto path = plan(map, { 1000, 1000, 7 }, { 1050, 1000, 7 }, &result);
    EXPECT_EQ(result, PathPartial);
    EXPECT_EQ(path.size(), 8u);
}

TEST(MovementPathPlanner, UnwalkableGoalStopsNextToIt)
{
    Map map;
    map.walls.insert({ 1003, 1000 });
    int result = 0;
    const TWorldMapCoordinate from(1000, 1000, 7);
    auto path = plan(map, from, { 1003, 1000, 7 }, &result);
    EXPECT_EQ(result, PathComplete);
    EXPECT_TRUE(walk(from, path) == TWorldMapCoordinate(1002, 1000, 7));
    path = plan(map, from, { 1003, 1000, 7 }, &result, true);
    EXPECT_TRUE(walk(from, path) == TWorldMapCoordinate(1003, 1000, 7));
}

TEST(MovementPathPlanner, NoWayWhenEnclosed)
{
    Map map;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy)
                map.walls.insert({ 1000 + dx, 1000 + dy });
    int result = 0;
    plan(map, { 1000, 1000, 7 }, { 1005, 1000, 7 }, &result);
    EXPECT_EQ(result, PathNoWay);
}

TEST(MovementPathPlanner, BlockedSetSparesStartAndGoal)
{
    // A creature standing on the goal does not block it: the path still ends there.
    Map map;
    const TWorldMapCoordinate from(1000, 1000, 7);
    const TWorldMapCoordinate to(1003, 1000, 7);
    std::vector<EMovementDirection> path;
    EXPECT_EQ(planMovementPath(from, to, false, false, map.cost(), { from, to }, path), PathComplete);
    EXPECT_TRUE(walk(from, path) == to);
    // The same creature one field short of the goal is walked around.
    EXPECT_EQ(planMovementPath(from, to, false, false, map.cost(), { { 1002, 1000, 7 } }, path), PathComplete);
    EXPECT_TRUE(walk(from, path) == to);
    TWorldMapCoordinate at = from;
    for (const EMovementDirection d : path) {
        const auto o = tibia::creatures::stepOffset(d);
        at = TWorldMapCoordinate(at.x + o.width, at.y + o.height, at.z);
        EXPECT_FALSE(at == TWorldMapCoordinate(1002, 1000, 7));
    }
}
