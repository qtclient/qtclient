#pragma once

#include "shared/cip_typed.h"

#include <client/position.h>

#include <QPoint>

class QDebug;

#include <iosfwd>

namespace tibia {
namespace worldmap {

/// Field coordinate (CIP: TWorldMapCoordinate; stack layout used by TLightMap::v7
/// at — x/y as field indices, z as floor).
class TWorldMapCoordinate : public ::tibia::CipTyped
{
public:
    int x = 0;
    int y = 0;
    int z = 0;

    TWorldMapCoordinate() = default;
    TWorldMapCoordinate(const int x_, const int y_, const int z_)
        : x(x_)
        , y(y_)
        , z(z_)
    {
    }

    explicit TWorldMapCoordinate(const Position& pos)
        : x(pos.x)
        , y(pos.y)
        , z(pos.z)
    {
    }

    Position toPosition() const { return Position(x, y, static_cast<uint8_t>(z)); }

    /// The field drawn at the same place on floor `floor`: every floor up shifts
    /// the picture one field up and left.
    TWorldMapCoordinate shearedToFloor(const int floor) const { return { x + z - floor, y + z - floor, floor }; }
    /// The field moved diagonally by `floor - z` fields (down-right), on the same floor.
    TWorldMapCoordinate shiftedTowardsFloor(const int floor) const { return { x + floor - z, y + floor - z, z }; }
    /// The field moved diagonally by `floor - z` fields (up-left), on the same floor.
    TWorldMapCoordinate shiftedAwayFromFloor(const int floor) const { return { x - floor + z, y - floor + z, z }; }

    bool operator==(const TWorldMapCoordinate& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
    /// Ordered by x, then y, then z.
    bool operator<(const TWorldMapCoordinate& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;
        return z < other.z;
    }
};

/// Inclusive volume size in fields (CIP: TWorldMapExtent v0/v1 at).
class TWorldMapExtent : public ::tibia::CipTyped
{
public:
    int width = 0;
    int height = 0;
    int depth = 0;

    TWorldMapExtent() = default;
    TWorldMapExtent(const int width_, const int height_, const int depth_ = 1)
        : width(width_)
        , height(height_)
        , depth(depth_)
    {
    }

    bool operator==(const TWorldMapExtent& other) const
    {
        return width == other.width && height == other.height && depth == other.depth;
    }
};

/// Sub-field (pixel) coordinate on the map plane (CIP: TWorldMapSubfieldCoordinate
/// v0/v1 at ; QML twin is TWorldMapSubfieldCoordinateQmlType).
class TWorldMapSubfieldCoordinate : public ::tibia::CipTyped
{
public:
    int subfieldX = 0;
    int subfieldY = 0;
    int z = 0;

    TWorldMapSubfieldCoordinate() = default;
    TWorldMapSubfieldCoordinate(const int subfieldX_, const int subfieldY_, const int z_)
        : subfieldX(subfieldX_)
        , subfieldY(subfieldY_)
        , z(z_)
    {
    }

    explicit TWorldMapSubfieldCoordinate(const QPoint& mapPixel, const int floor = 0)
        : subfieldX(mapPixel.x())
        , subfieldY(mapPixel.y())
        , z(floor)
    {
    }

    QPoint toMapPixel() const { return { subfieldX, subfieldY }; }

    /// Field that contains this subfield (CIP field size 32; division toward zero).
    TWorldMapCoordinate toFieldCoordinate() const
    {
        return { subfieldX / 32, subfieldY / 32, z };
    }

    static TWorldMapSubfieldCoordinate fromField(const TWorldMapCoordinate& field, const int offsetX = 16,
                                                 const int offsetY = 16)
    {
        return { field.x * 32 + offsetX, field.y * 32 + offsetY, field.z };
    }

    bool operator==(const TWorldMapSubfieldCoordinate& other) const
    {
        return subfieldX == other.subfieldX && subfieldY == other.subfieldY && z == other.z;
    }
};

/// Size in subfields (CIP: TWorldMapSubfieldExtent v0/v1 at).
class TWorldMapSubfieldExtent : public ::tibia::CipTyped
{
public:
    int width = 0;
    int height = 0;

    TWorldMapSubfieldExtent() = default;
    TWorldMapSubfieldExtent(const int width_, const int height_)
        : width(width_)
        , height(height_)
    {
    }

    explicit TWorldMapSubfieldExtent(const TWorldMapExtent& fields)
        : width(fields.width * 32)
        , height(fields.height * 32)
    {
    }

    bool operator==(const TWorldMapSubfieldExtent& other) const
    {
        return width == other.width && height == other.height;
    }
};

/// A box of fields: its origin, the field past its far corner and its size
/// (CIP: TWorldMapCuboid).
class TWorldMapCuboid : public ::tibia::CipTyped
{
public:
    TWorldMapCoordinate origin;
    /// origin + extent: one past the last field on every axis.
    TWorldMapCoordinate end;
    TWorldMapExtent extent;

    TWorldMapCuboid()
        : extent(0, 0, 0)
    {
    }
    TWorldMapCuboid(const TWorldMapCoordinate& origin_, const TWorldMapExtent& extent_)
        : origin(origin_)
        , end(origin_.x + extent_.width, origin_.y + extent_.height, origin_.z + extent_.depth)
        , extent(extent_)
    {
    }

    /// The same box moved by `offset` fields.
    TWorldMapCuboid translated(const TWorldMapExtent& offset) const
    {
        return TWorldMapCuboid({ origin.x + offset.width, origin.y + offset.height, origin.z + offset.depth }, extent);
    }
    /// The middle field of the origin's floor.
    TWorldMapCoordinate center() const
    {
        return { origin.x + (end.x - origin.x) / 2, origin.y + (end.y - origin.y) / 2, origin.z };
    }

    bool operator==(const TWorldMapCuboid& other) const
    {
        return origin == other.origin && end == other.end && extent == other.extent;
    }
};

/// A box of fields seen through the isometric shear: floors other than
/// `shearFloor` are shifted one field per floor (CIP: TShearedWorldMapCuboid; the
/// stored volume and the visible area).
class TShearedWorldMapCuboid : public TWorldMapCuboid
{
public:
    int shearFloor = 0;

    TShearedWorldMapCuboid() = default;
    TShearedWorldMapCuboid(const TWorldMapCoordinate& origin_, const TWorldMapExtent& extent_, const int shearFloor_)
        : TWorldMapCuboid(origin_, extent_)
        , shearFloor(shearFloor_)
    {
    }
    /// Sheared at the box's own floor.
    explicit TShearedWorldMapCuboid(const TWorldMapCuboid& cuboid)
        : TWorldMapCuboid(cuboid)
        , shearFloor(cuboid.origin.z)
    {
    }
    TShearedWorldMapCuboid(const TWorldMapCuboid& cuboid, const int shearFloor_)
        : TWorldMapCuboid(cuboid)
        , shearFloor(shearFloor_)
    {
    }

    /// The box itself.
    TWorldMapCuboid cuboid() const { return *this; }
    /// The floors `firstFloor` to `lastFloor` of the box (same shear); empty when
    /// lastFloor is above firstFloor.
    TShearedWorldMapCuboid restrictedToFloors(const int firstFloor, const int lastFloor) const
    {
        if (lastFloor < firstFloor)
            return {};
        const TWorldMapCuboid moved = translated(TWorldMapExtent(0, 0, firstFloor - origin.z));
        return TShearedWorldMapCuboid(
            TWorldMapCuboid(moved.origin, TWorldMapExtent(moved.extent.width, moved.extent.height, lastFloor - firstFloor + 1)),
            shearFloor);
    }

    bool operator==(const TShearedWorldMapCuboid& other) const
    {
        return TWorldMapCuboid::operator==(other) && shearFloor == other.shearFloor;
    }
};

/// "[x=fieldX+offsetX, y=fieldY+offsetY, z]" (CIP's debug print of a subfield coordinate).
QDebug operator<<(QDebug debug, const TWorldMapSubfieldCoordinate& coordinate);
std::ostream& operator<<(std::ostream& stream, const TWorldMapSubfieldCoordinate& coordinate);

}
}
