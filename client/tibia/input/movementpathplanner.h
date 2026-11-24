#pragma once

#include "creatures/movementdirection.h"
#include "minimap/waypointsectors.h"
#include "worldmap/coordinates.h"

#include <set>
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

/// Plan the first steps from `from` to `goal` on one floor.
///
/// Searches backwards from the goal over the fields within 110 of `from`,
/// where entering a field costs its waypoint cost (the minimap tile storage's
/// `waypointSectors`: 0..250 walkable, 254 unknown, 255 blocked) and a diagonal
/// step three times that. A neighbouring goal is reached with one step without
/// searching (diagonally only with `allowDiagonalStep`). Unless
/// `enterUnwalkableGoal`, a path to an unwalkable goal stops next to it. The
/// `avoidedFields` (the tile storage's fields to avoid) other than the start
/// and the goal are never entered.
int planMovementPath(const worldmap::TWorldMapCoordinate& from, const worldmap::TWorldMapCoordinate& goal,
                     bool enterUnwalkableGoal, bool allowDiagonalStep, const minimap::TWaypointSectors& waypointSectors,
                     const std::set<worldmap::TWorldMapCoordinate>& avoidedFields,
                     std::vector<creatures::EMovementDirection>& path);

}
