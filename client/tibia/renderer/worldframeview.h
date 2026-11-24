#pragma once

// Host: the CipSoft data an extension reads about the world frame (lighting
// driver fields, frame hooks, field stack peel). No CipSoft counterpart.

#include "qtclient/abi/lighting.h"

#include <cstdint>

namespace tibia {
namespace appearances {
class TAppearanceType;
}
namespace worldmap {
class TWorldMapField;
}
namespace qmlcomponents {

/// QTCLIENT_APPEARANCE_* flags of `type` (0 without one).
uint32_t appearanceFlagsOf(const appearances::TAppearanceType* type);

/// The stack of `field` in CipSoft order into `out` (at most `max` entries);
/// returns the stack's size.
int peelFieldStack(const worldmap::TWorldMapField& field, QtClientFieldStackEntry* out, int max);

/// Host ABI `world_field_stack`: the stack of the current session's field at
/// (x, y, z), -1 when the session holds no such field. `drawable` (may be null)
/// is set to whether the field draws anything (objects, stepping creatures or effects).
int worldFieldStack(int x, int y, int z, int* drawable, QtClientFieldStackEntry* out, int max);

}
}
