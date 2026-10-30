#pragma once

#include "shared/cip_typed.h"

#include <client/position.h>

#include <QPoint>

namespace tibia::worldmap {

/// Field coordinate (CIP: TWorldMapCoordinate; stack layout used by TLightMap::v7
/// at 0x10107ae40 — x/y as field indices, z as floor).
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

    bool operator==(const TWorldMapCoordinate& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

/// Inclusive volume size in fields (CIP: TWorldMapExtent v0/v1 at 0x1008c2b40).
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
/// v0/v1 at 0x10089a3f0; QML twin is TWorldMapSubfieldCoordinateQmlType).
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

    /// Field that contains this subfield (CIP field size 32).
    TWorldMapCoordinate toFieldCoordinate() const
    {
        return { subfieldX >> 5, subfieldY >> 5, z };
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

/// Size in subfields (CIP: TWorldMapSubfieldExtent v0/v1 at 0x100954d20).
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

}
