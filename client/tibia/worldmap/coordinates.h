#pragma once


#include <client/position.h>

#include <QPoint>

class QDebug;

#include <cstddef>
#include <iosfwd>

namespace tibia {
namespace worldmap {

/// Field coordinate (CIP: TWorldMapCoordinate; stack layout used by TLightMap::v7
/// at — x/y as field indices, z as floor).
class TWorldMapCoordinate
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
class TWorldMapExtent
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

    /// Every component made non-negative.
    TWorldMapExtent absolute() const
    {
        return { width < 0 ? -width : width, height < 0 ? -height : height, depth < 0 ? -depth : depth };
    }
    /// The number of fields of the box: width × height × depth, 0 unless all three are positive.
    std::size_t volume() const
    {
        if (width <= 0 || height <= 0 || depth <= 0)
            return 0;
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(depth);
    }

    bool operator==(const TWorldMapExtent& other) const
    {
        return width == other.width && height == other.height && depth == other.depth;
    }
};

/// The field `offset` fields away.
inline TWorldMapCoordinate operator+(const TWorldMapCoordinate& coordinate, const TWorldMapExtent& offset)
{
    return { coordinate.x + offset.width, coordinate.y + offset.height, coordinate.z + offset.depth };
}
/// The field `offset` fields back.
inline TWorldMapCoordinate operator-(const TWorldMapCoordinate& coordinate, const TWorldMapExtent& offset)
{
    return { coordinate.x - offset.width, coordinate.y - offset.height, coordinate.z - offset.depth };
}
/// How far `from` lies from `to`, per axis.
inline TWorldMapExtent operator-(const TWorldMapCoordinate& to, const TWorldMapCoordinate& from)
{
    return { to.x - from.x, to.y - from.y, to.z - from.z };
}
/// The smaller value of every axis.
inline TWorldMapCoordinate minimum(const TWorldMapCoordinate& a, const TWorldMapCoordinate& b)
{
    return { b.x < a.x ? b.x : a.x, b.y < a.y ? b.y : a.y, b.z < a.z ? b.z : a.z };
}
/// The larger value of every axis.
inline TWorldMapCoordinate maximum(const TWorldMapCoordinate& a, const TWorldMapCoordinate& b)
{
    return { b.x < a.x ? a.x : b.x, b.y < a.y ? a.y : b.y, b.z < a.z ? a.z : b.z };
}

/// Sub-field (pixel) coordinate on the map plane (CIP: TWorldMapSubfieldCoordinate
/// v0/v1 at ; QML twin is TWorldMapSubfieldCoordinateQmlType).
class TWorldMapSubfieldCoordinate
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

    /// Where this subfield is drawn on layer `layer`: one field down and right per
    /// layer it lies below `layer`.
    QPoint projectedToLayer(const int layer) const
    {
        const int shift = (z - layer) * 32;
        return { subfieldX + shift, subfieldY + shift };
    }
    /// The subfield on layer `layer` that is drawn where `projected` lies on
    /// layer `projectionLayer`.
    static TWorldMapSubfieldCoordinate fromProjection(const QPoint& projected, const int layer, const int projectionLayer = 0)
    {
        const int shift = (layer - projectionLayer) * 32;
        return { projected.x() - shift, projected.y() - shift, layer };
    }

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

    /// The top-left sub-field of `field`, moved `elevation` sub-fields up and left.
    static TWorldMapSubfieldCoordinate fromFieldElevated(const TWorldMapCoordinate& field, const int elevation)
    {
        return { field.x * 32 - elevation, field.y * 32 - elevation, field.z };
    }

    bool operator==(const TWorldMapSubfieldCoordinate& other) const
    {
        return subfieldX == other.subfieldX && subfieldY == other.subfieldY && z == other.z;
    }
};

/// Size in subfields (CIP: TWorldMapSubfieldExtent). The third component is an
/// elevation where one is carried (0 otherwise).
class TWorldMapSubfieldExtent
{
public:
    int width = 0;
    int height = 0;
    int depth = 0;

    TWorldMapSubfieldExtent() = default;
    TWorldMapSubfieldExtent(const int width_, const int height_, const int depth_ = 0)
        : width(width_)
        , height(height_)
        , depth(depth_)
    {
    }

    /// Where a subfield lies inside its field (remainder toward zero).
    static TWorldMapSubfieldExtent offsetInField(const TWorldMapSubfieldCoordinate& coordinate)
    {
        return { coordinate.subfieldX % 32, coordinate.subfieldY % 32, 0 };
    }

    explicit TWorldMapSubfieldExtent(const TWorldMapExtent& fields)
        : width(fields.width * 32)
        , height(fields.height * 32)
    {
    }

    bool operator==(const TWorldMapSubfieldExtent& other) const
    {
        return width == other.width && height == other.height && depth == other.depth;
    }
};

/// The subfield `offset` subfields away.
inline TWorldMapSubfieldCoordinate operator+(const TWorldMapSubfieldCoordinate& coordinate, const TWorldMapSubfieldExtent& offset)
{
    return { coordinate.subfieldX + offset.width, coordinate.subfieldY + offset.height, coordinate.z + offset.depth };
}
/// How far `from` lies from `to`, per axis.
inline TWorldMapSubfieldExtent operator-(const TWorldMapSubfieldCoordinate& to, const TWorldMapSubfieldCoordinate& from)
{
    return { to.subfieldX - from.subfieldX, to.subfieldY - from.subfieldY, to.z - from.z };
}
/// A field's top-left subfield moved by `offset`'s width and height, then up and
/// left by its depth (the elevation).
inline TWorldMapSubfieldCoordinate subfieldOfField(const TWorldMapCoordinate& field, const TWorldMapSubfieldExtent& offset)
{
    return { field.x * 32 + offset.width - offset.depth, field.y * 32 + offset.height - offset.depth, field.z };
}

/// A box of fields: its origin, the field past its far corner and its size
/// (CIP: TWorldMapCuboid).
class TWorldMapCuboid
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

    /// The single-floor box of `width` × `height` fields at `origin_`.
    TWorldMapCuboid(const TWorldMapCoordinate& origin_, const int width, const int height)
        : TWorldMapCuboid(origin_, TWorldMapExtent(width, height, 1))
    {
    }
    /// The box holding just the field `field`.
    static TWorldMapCuboid ofField(const TWorldMapCoordinate& field) { return TWorldMapCuboid(field, TWorldMapExtent(1, 1, 1)); }

    TWorldMapCoordinate getOrigin() const { return origin; }
    TWorldMapCoordinate getEnd() const { return end; }
    TWorldMapExtent getExtent() const { return extent; }
    /// The origin's x and y on the box's last floor.
    TWorldMapCoordinate lastFloorOrigin() const { return { origin.x, origin.y, origin.z - 1 + extent.depth }; }

    /// The fields both boxes hold; the empty box when they do not overlap.
    TWorldMapCuboid intersected(const TWorldMapCuboid& other) const
    {
        const auto overlap = [](const TWorldMapCuboid& a, const TWorldMapCuboid& b, TWorldMapCuboid& out) {
            const TWorldMapCoordinate hi = minimum(a.origin + a.extent, b.origin + b.extent);
            const TWorldMapCoordinate lo = maximum(a.origin, b.origin);
            const TWorldMapExtent size = hi - lo;
            if (size.width < 0 || size.height < 0 || size.depth < 0)
                return false;
            out = TWorldMapCuboid(lo, size);
            return true;
        };
        TWorldMapCuboid result;
        if (overlap(*this, other, result) || overlap(other, *this, result))
            return result;
        return TWorldMapCuboid(TWorldMapCoordinate(), TWorldMapExtent(0, 0, 0));
    }
    /// The smallest box holding both boxes.
    TWorldMapCuboid united(const TWorldMapCuboid& other) const
    {
        const TWorldMapCoordinate hi = maximum(origin + extent, other.origin + other.extent);
        const TWorldMapCoordinate lo = minimum(origin, other.origin);
        return TWorldMapCuboid(lo, hi - lo);
    }
    /// One floor of the box as drawn on floor `floor`: the origin sheared to that
    /// floor, one floor deep.
    TWorldMapCuboid floorArea(const int floor) const
    {
        return TWorldMapCuboid(origin.shearedToFloor(floor), TWorldMapExtent(extent.width, extent.height, 1));
    }
    /// The box widened by its depth on every side of x and y: the origin moved up
    /// and left and the end down and right by the number of floors.
    TWorldMapCuboid shearEnvelope() const
    {
        const TWorldMapCoordinate first = origin.shiftedAwayFromFloor(end.z);
        const TWorldMapCoordinate last = end.shiftedAwayFromFloor(origin.z);
        return TWorldMapCuboid(first, last - first);
    }
    /// The field, moved one field up-left per floor below the box's first floor,
    /// lies inside the box (half-open on every axis).
    bool containsSheared(const TWorldMapCoordinate& coordinate) const
    {
        const TWorldMapCoordinate sheared = coordinate.shiftedAwayFromFloor(origin.z);
        return origin.x <= sheared.x && sheared.x < end.x && origin.y <= sheared.y && sheared.y < end.y
               && origin.z <= sheared.z && sheared.z < end.z;
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
    /// The `index`th field of the box counted column first (y runs fastest,
    /// then x, then the floors from the origin's floor down), moved diagonally
    /// back by its distance to the origin's floor.
    TWorldMapCoordinate coordinateForIndexByColumnFirstAndSkyToGroundIncludingShearing(const std::size_t index) const
    {
        const auto height = static_cast<std::size_t>(static_cast<long>(extent.height));
        const auto floorSize = static_cast<std::size_t>(static_cast<long>(extent.height * extent.width));
        const int column = static_cast<int>(index / height);
        const int row = static_cast<int>(index % height);
        const int floor = static_cast<int>(index / floorSize);
        const TWorldMapCoordinate field = origin + TWorldMapExtent(column % extent.width, row, floor % extent.depth);
        return field.shiftedTowardsFloor(origin.z);
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

    /// The box at `origin_`'s x and y from floor `firstFloor`, sheared at
    /// `origin_`'s floor; the empty box when the floors do not fit.
    static TShearedWorldMapCuboid fromShearOrigin(const TWorldMapCoordinate& origin_, const TWorldMapExtent& extent_,
                                                  const int firstFloor)
    {
        const int shear = origin_.z;
        const int last = shear - firstFloor + extent_.depth;
        if (last < shear || shear < firstFloor || last < firstFloor)
            return TShearedWorldMapCuboid(TWorldMapCuboid(), 0);
        return TShearedWorldMapCuboid(TWorldMapCoordinate(origin_.x, origin_.y, firstFloor), extent_, shear);
    }
    /// The same fields with the shear at the origin's own floor: the origin moved
    /// diagonally by its distance to the shear floor.
    TShearedWorldMapCuboid unsheared() const
    {
        const TWorldMapCoordinate moved = origin.shiftedTowardsFloor(shearFloor);
        return TShearedWorldMapCuboid(TWorldMapCuboid(moved, extent), moved.z);
    }

    /// The origin's field on the box's last floor, moved diagonally by that
    /// floor's distance to the shear floor (its z stays the last floor).
    TWorldMapCoordinate shearedLastFloorOrigin() const { return lastFloorOrigin().shiftedTowardsFloor(shearFloor); }

    /// The box itself.
    TWorldMapCuboid cuboid() const { return *this; }
    /// The box's fields on floor `floor` as one floor: the origin's x and y on
    /// that floor, moved diagonally by its distance to the shear floor (the
    /// base's floorArea measures from the origin's own floor instead).
    TWorldMapCuboid shearedFloorArea(const int floor) const
    {
        return TWorldMapCuboid(TWorldMapCoordinate(origin.x, origin.y, floor).shiftedTowardsFloor(shearFloor),
                               TWorldMapExtent(extent.width, extent.height, 1));
    }
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

    /// The field, seen from the shear floor, lies inside the box (half-open on
    /// every axis).
    bool contains(const TWorldMapCoordinate& coordinate) const
    {
        const int shift = coordinate.z - shearFloor;
        const int x = coordinate.x + shift;
        const int y = coordinate.y + shift;
        return origin.x <= x && x < end.x && origin.y <= y && y < end.y && origin.z <= coordinate.z && coordinate.z < end.z;
    }

    /// The `index`th field of the box in the order of a map area above ground:
    /// column first (y fastest, then x), the floors from the last one up to the
    /// origin's floor; the field moved diagonally by its floor's distance to the
    /// shear floor.
    TWorldMapCoordinate coordinateForIndexByColumnFirstAndGroundToSky(const std::size_t index) const
    {
        const TWorldMapCoordinate field = fieldForIndex(index, true);
        return field.shiftedTowardsFloor(shearFloor);
    }
    /// The same order with the floors from the origin's floor down to the last one
    /// (a map area underground).
    TWorldMapCoordinate coordinateForIndexByColumnFirstAndSkyToGround(const std::size_t index) const
    {
        const TWorldMapCoordinate field = fieldForIndex(index, false);
        return field.shiftedTowardsFloor(shearFloor);
    }

    bool operator==(const TShearedWorldMapCuboid& other) const
    {
        return TWorldMapCuboid::operator==(other) && shearFloor == other.shearFloor;
    }

private:
    TWorldMapCoordinate fieldForIndex(const std::size_t index, const bool lastFloorFirst) const
    {
        const auto height = static_cast<std::size_t>(extent.height);
        const auto floorSize = height * static_cast<std::size_t>(extent.width);
        const int column = static_cast<int>((index / height) % static_cast<std::size_t>(extent.width));
        const int row = static_cast<int>(index % height);
        const int floorIndex = static_cast<int>((index / floorSize) % static_cast<std::size_t>(extent.depth));
        const int floor = lastFloorFirst ? extent.depth + ~floorIndex : floorIndex;
        return origin + TWorldMapExtent(column, row, floor);
    }
};

/// The central position of the map messages and the area the client knows
/// around it (CIP: the central position of the server message translator and of
/// the world map protocol handler: the centre field, the visible width and
/// height and the centre's offset from the area's top-left field). Every map
/// message moves it the way the server moved the player's view, and each
/// message's fields cover the area its move added.
class TWorldMapCentralPosition
{
public:
    static constexpr int GroundFloor = 7;
    static constexpr int LastFloor = 15;
    /// Floors seen above and below an underground centre.
    static constexpr int UndergroundFloorRange = 2;

    TWorldMapCoordinate center;
    TWorldMapExtent size;
    TWorldMapExtent offset;

    /// Field (10, 8, 7) in an 18 × 14 area, 8 columns and 6 rows from its
    /// top-left field.
    TWorldMapCentralPosition()
        : center(10, 8, 7)
        , size(18, 14, 0)
        , offset(8, 6, 0)
    {
    }
    TWorldMapCentralPosition(const TWorldMapCoordinate& center_, const TWorldMapExtent& size_, const TWorldMapExtent& offset_)
        : center(center_)
        , size(size_)
        , offset(offset_)
    {
    }

    /// The centre is below the ground floor: the floors are read sky to ground.
    bool isUnderground() const { return center.z > GroundFloor; }

    /// The area around the centre: width × height fields from the centre minus
    /// the offset; floors 0 to 7 above ground, two floors up and down (at most
    /// floor 15) underground; sheared at the centre's floor.
    TShearedWorldMapCuboid visibleArea() const
    {
        int lastFloor = GroundFloor;
        if (center.z > GroundFloor)
            lastFloor = center.z + (LastFloor - center.z < UndergroundFloorRange ? LastFloor - center.z : UndergroundFloorRange);
        const int firstFloor = center.z < GroundFloor + 1 ? 0 : center.z - UndergroundFloorRange;
        const TWorldMapCoordinate topLeft = center - offset;
        return TShearedWorldMapCuboid(
            TWorldMapCuboid({ topLeft.x, topLeft.y, firstFloor }, TWorldMapExtent(size.width, size.height, lastFloor - firstFloor + 1)),
            center.z);
    }
    /// The visible area's first column / last column / first row / last row.
    TShearedWorldMapCuboid leftColumn() const { return slice(false, false); }
    TShearedWorldMapCuboid rightColumn() const { return slice(false, true); }
    TShearedWorldMapCuboid topRow() const { return slice(true, false); }
    TShearedWorldMapCuboid bottomRow() const { return slice(true, true); }
    /// The floors a move one floor up adds: floors 0 to 5 when the centre came up
    /// to the ground floor, the first floor underground, nothing above ground.
    TShearedWorldMapCuboid topFloorArea() const
    {
        const TShearedWorldMapCuboid area = visibleArea();
        const int firstFloor = area.origin.shiftedTowardsFloor(area.shearFloor).z;
        if (center.z < GroundFloor + 1) {
            if (center.z != GroundFloor)
                return TShearedWorldMapCuboid(TWorldMapCuboid(), 0);
            return area.restrictedToFloors(firstFloor, GroundFloor - UndergroundFloorRange);
        }
        return area.restrictedToFloors(firstFloor, firstFloor);
    }
    /// The floors a move one floor down adds: floors 8 to 10 when the centre went
    /// below the ground floor, the last floor further down (centre floors 9 to
    /// 13), nothing otherwise.
    TShearedWorldMapCuboid bottomFloorArea() const
    {
        const TShearedWorldMapCuboid area = visibleArea();
        const int lastFloor = (area.origin + TWorldMapExtent(0, 0, area.extent.depth - 1)).shiftedTowardsFloor(area.shearFloor).z;
        if (center.z > GroundFloor + 1 && center.z <= LastFloor - UndergroundFloorRange)
            return area.restrictedToFloors(lastFloor, lastFloor);
        if (center.z == GroundFloor + 1)
            return area.restrictedToFloors(lastFloor - UndergroundFloorRange, lastFloor);
        return TShearedWorldMapCuboid(TWorldMapCuboid(), 0);
    }

    /// The moves of the four slice messages.
    void moveLeft() { --center.x; }
    void moveRight() { ++center.x; }
    void moveUp() { --center.y; }
    void moveDown() { ++center.y; }
    /// One floor up / down, the field moved diagonally so that it stays drawn at
    /// the same place.
    void moveUpFloor() { center = TWorldMapCoordinate(center.x, center.y, center.z - 1).shiftedTowardsFloor(center.z); }
    void moveDownFloor() { center = TWorldMapCoordinate(center.x, center.y, center.z + 1).shiftedTowardsFloor(center.z); }

    /// The `index`th field of a map area in `area`, in the order the server
    /// describes it from this centre (ground to sky above ground, sky to
    /// ground underground).
    TWorldMapCoordinate coordinateOfAreaIndex(const TShearedWorldMapCuboid& area, const std::size_t index) const
    {
        return isUnderground() ? area.coordinateForIndexByColumnFirstAndSkyToGround(index)
                               : area.coordinateForIndexByColumnFirstAndGroundToSky(index);
    }

private:
    TShearedWorldMapCuboid slice(const bool row, const bool last) const
    {
        const TShearedWorldMapCuboid area = visibleArea();
        TWorldMapCoordinate origin = area.origin;
        if (last) {
            if (row)
                origin.y += area.extent.height - 1;
            else
                origin.x += area.extent.width - 1;
        }
        const TWorldMapExtent extent(row ? area.extent.width : 1, row ? 1 : area.extent.height, area.extent.depth);
        return TShearedWorldMapCuboid(TWorldMapCuboid(origin, extent), area.shearFloor);
    }
};

/// "TWorldMapCuboid([x,y,z] - [endX, endY, endZ], Extent[width, height, depth])"
/// (CIP's debug print of a box; the QDebug one prints without automatic spaces).
QDebug operator<<(QDebug debug, const TWorldMapCuboid& cuboid);
std::ostream& operator<<(std::ostream& stream, const TWorldMapCuboid& cuboid);
/// "TShearedWorldMapCuboid(<the box>,Center Layer shearFloor)".
QDebug operator<<(QDebug debug, const TShearedWorldMapCuboid& cuboid);

/// "[x=fieldX+offsetX, y=fieldY+offsetY, z]" (CIP's debug print of a subfield coordinate).
QDebug operator<<(QDebug debug, const TWorldMapSubfieldCoordinate& coordinate);
std::ostream& operator<<(std::ostream& stream, const TWorldMapSubfieldCoordinate& coordinate);

}
}
