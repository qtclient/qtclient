#pragma once

#include "creatures/movementdirection.h"
#include "worldmap/coordinates.h"

#include <QtGlobal>

#include <functional>
#include <vector>

namespace tibia::input {

/// Result of planning a walk. Positive: `path` holds the first steps.
enum EMovementPathResult : int {
    PathInternalError = -5,
    PathNoWay = -4,
    PathOutOfRange = -3,
    PathGoDownstairs = -2,
    PathGoUpstairs = -1,
    PathAlreadyThere = 0,
    PathComplete = 1,
    /// The path leaves the visible area or is longer than ten steps: walk it, then plan again.
    PathPartial = 2,
};

/// Waypoint cost of a field: 0..250 walkable, 254 unknown, 255 blocked.
using WaypointCost = std::function<quint8(int x, int y, int z)>;

/// Plan the first steps from `from` to `goal` on one floor.
///
/// Searches backwards from the goal over the fields within 110 of `from`,
/// where entering a field costs its waypoint cost and a diagonal step three
/// times that. A neighbouring goal is reached with one step without searching
/// (diagonally only with `allowDiagonalStep`). Unless `enterUnwalkableGoal`, a
/// path to an unwalkable goal stops next to it. `blocked` fields (creatures in
/// the way) are never entered.
int planMovementPath(const worldmap::TWorldMapCoordinate& from, const worldmap::TWorldMapCoordinate& goal,
                     bool enterUnwalkableGoal, bool allowDiagonalStep, const WaypointCost& cost,
                     const std::vector<worldmap::TWorldMapCoordinate>& blocked,
                     std::vector<creatures::EMovementDirection>& path);

}
