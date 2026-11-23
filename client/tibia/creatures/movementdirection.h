#pragma once

#include "worldmap/coordinates.h"

#include <QDebug>
#include <QtGlobal>

#include <stdexcept>
#include <ostream>
#include <string>

namespace tibia::creatures {

/// One-field step directions, clockwise from north. The values 8..12 are not
/// steps: 8 leaves the look direction alone, 9..12 only turn the creature.
enum class EMovementDirection : quint8 {
    North = 0,
    NorthEast = 1,
    East = 2,
    SouthEast = 3,
    South = 4,
    SouthWest = 5,
    West = 6,
    NorthWest = 7,
    None = 8,
    TurnNorth = 9,
    TurnEast = 10,
    TurnSouth = 11,
    TurnWest = 12,
};

/// Look directions of an outfit (`ECreatureLookDirection` in the QML contract).
enum class ECreatureLookDirection : quint8 { North = 0, East = 1, South = 2, West = 3 };

inline bool isDiagonal(const EMovementDirection direction)
{
    const auto d = static_cast<unsigned>(direction);
    return d < 8 && ((0xaau >> d) & 1u) != 0;
}

/// Field offset of one step in `direction` (no offset for non-steps).
inline worldmap::TWorldMapExtent stepOffset(const EMovementDirection direction)
{
    switch (direction) {
    case EMovementDirection::North: return { 0, -1, 0 };
    case EMovementDirection::NorthEast: return { 1, -1, 0 };
    case EMovementDirection::East: return { 1, 0, 0 };
    case EMovementDirection::SouthEast: return { 1, 1, 0 };
    case EMovementDirection::South: return { 0, 1, 0 };
    case EMovementDirection::SouthWest: return { -1, 1, 0 };
    case EMovementDirection::West: return { -1, 0, 0 };
    case EMovementDirection::NorthWest: return { -1, -1, 0 };
    default: return { 0, 0, 0 };
    }
}

/// Offset from the destination of a step back to where it started.
inline worldmap::TWorldMapExtent reverseStepOffset(const EMovementDirection direction)
{
    const worldmap::TWorldMapExtent step = stepOffset(direction);
    return { -step.width, -step.height, 0 };
}

/// Direction of a one-field step on the same floor.
/// Throws std::invalid_argument for any other offset.
inline EMovementDirection directionOfStep(const worldmap::TWorldMapExtent& offset)
{
    constexpr EMovementDirection order[] = {
        EMovementDirection::East, EMovementDirection::West, EMovementDirection::South,
        EMovementDirection::North, EMovementDirection::SouthEast, EMovementDirection::NorthEast,
        EMovementDirection::SouthWest, EMovementDirection::NorthWest,
    };
    for (const EMovementDirection d : order) {
        if (stepOffset(d) == offset)
            return d;
    }
    throw std::invalid_argument("(" + std::to_string(offset.width) + ", " + std::to_string(offset.height) + ", "
                                + std::to_string(offset.depth) + ") cannot be mapped to a single step!");
}

/// Direction from `from` to `to`: a step direction for neighbouring fields on the
/// same floor, `None` for the same field, otherwise the turn towards `to`.
inline EMovementDirection directionBetween(const worldmap::TWorldMapCoordinate& from,
                                           const worldmap::TWorldMapCoordinate& to)
{
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    const int dz = to.z - from.z;
    if (dz == 0 && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) {
        if (dx == -1)
            return static_cast<EMovementDirection>(6 - dy);
        if (dx == 1)
            return static_cast<EMovementDirection>(dy + 2);
        if (dy == -1)
            return EMovementDirection::North;
        return dy == 1 ? EMovementDirection::South : EMovementDirection::None;
    }
    if (dx > 0)
        return EMovementDirection::TurnEast;
    if (dx == 0) {
        if (dy < 0)
            return EMovementDirection::TurnNorth;
        return dy != 0 ? EMovementDirection::TurnSouth : EMovementDirection::None;
    }
    return EMovementDirection::TurnWest;
}

/// Look direction after moving or turning in `direction`; other values keep `current`.
inline ECreatureLookDirection lookDirectionAfter(const ECreatureLookDirection current,
                                                 const EMovementDirection direction)
{
    constexpr quint8 table[13] = { 0, 1, 1, 1, 2, 3, 3, 3, 0, 0, 1, 2, 3 };
    const auto d = static_cast<unsigned>(direction);
    if (d < 13 && ((0x1effu >> d) & 1u) != 0)
        return static_cast<ECreatureLookDirection>(table[d]);
    return current;
}

/// A one-field step (not a teleport or turn).
inline bool isStep(const EMovementDirection direction)
{
    return static_cast<unsigned>(direction) < 8;
}

/// The step direction a look direction faces.
inline EMovementDirection movementDirectionOfLook(const ECreatureLookDirection look)
{
    const auto l = static_cast<unsigned>(look);
    if (l < 4)
        return static_cast<EMovementDirection>(l * 2);
    throw std::logic_error("missing case for ECreatureLookDirection");
}

/// CipSoft's name of a look direction; nullptr for an unknown value.
inline const char* lookDirectionName(const ECreatureLookDirection look)
{
    switch (look) {
    case ECreatureLookDirection::North: return "NORTH";
    case ECreatureLookDirection::East: return "EAST";
    case ECreatureLookDirection::South: return "SOUTH";
    case ECreatureLookDirection::West: return "WEST";
    }
    return nullptr;
}

/// CipSoft's name of a movement direction (8..12 are its teleports); nullptr
/// for an unknown value.
inline const char* movementDirectionName(const EMovementDirection direction)
{
    switch (direction) {
    case EMovementDirection::North: return "NORTH";
    case EMovementDirection::NorthEast: return "NORTHEAST";
    case EMovementDirection::East: return "EAST";
    case EMovementDirection::SouthEast: return "SOUTHEAST";
    case EMovementDirection::South: return "SOUTH";
    case EMovementDirection::SouthWest: return "SOUTHWEST";
    case EMovementDirection::West: return "WEST";
    case EMovementDirection::NorthWest: return "NORTHWEST";
    case EMovementDirection::None: return "TELEPORT_UP_OR_DOWN";
    case EMovementDirection::TurnNorth: return "TELEPORT_NORTH";
    case EMovementDirection::TurnEast: return "TELEPORT_EAST";
    case EMovementDirection::TurnSouth: return "TELEPORT_SOUTH";
    case EMovementDirection::TurnWest: return "TELEPORT_WEST";
    }
    return nullptr;
}

/// The name; nothing for an unknown value.
inline std::ostream& operator<<(std::ostream& stream, const ECreatureLookDirection look)
{
    if (const char* name = lookDirectionName(look))
        stream << name;
    return stream;
}
inline std::ostream& operator<<(std::ostream& stream, const EMovementDirection direction)
{
    if (const char* name = movementDirectionName(direction))
        stream << name;
    return stream;
}
/// The name without quotes; nothing for an unknown value.
inline QDebug operator<<(QDebug debug, const ECreatureLookDirection look)
{
    if (const char* name = lookDirectionName(look))
        debug << name;
    return debug;
}
inline QDebug operator<<(QDebug debug, const EMovementDirection direction)
{
    if (const char* name = movementDirectionName(direction))
        debug << name;
    return debug;
}

}
