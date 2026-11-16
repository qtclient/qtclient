#pragma once

#include "coordinate.h"

namespace tibia {
namespace shared {

/// CIP `shared::TExtent`: a size along x, y and z, as a coordinate offset.
class TExtent : public TCoordinate
{
public:
    TExtent() = default;
    TExtent(const int x_, const int y_, const int z_)
        : TCoordinate(x_, y_, z_)
    {
    }
    /// The offset from `from` to `to`.
    TExtent(const TCoordinate& from, const TCoordinate& to)
        : TCoordinate(to.x - from.x, to.y - from.y, to.z - from.z)
    {
    }

    TExtent operator+(const TExtent& other) const { return TExtent(other.x + x, other.y + y, other.z + z); }
    TExtent operator-(const TExtent& other) const { return TExtent(x - other.x, y - other.y, z - other.z); }
};

}
}
