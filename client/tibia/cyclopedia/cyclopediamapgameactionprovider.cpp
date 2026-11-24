#include "cyclopediamapgameactionprovider.h"

#include "worldmap/coordinates.h"

namespace tibia::cyclopedia {

namespace {

/// The walk target: the field drawn at `coord` when the map shows `floor`.
Position shearedTarget(const Position& coord, const int floor)
{
    const worldmap::TWorldMapCoordinate sheared = worldmap::TWorldMapCoordinate(coord).shearedToFloor(floor);
    return Position(sheared.x, sheared.y, static_cast<uint8_t>(sheared.z));
}

}

input::TGameActionList TCyclopediaMapGameActionProvider::fieldActions(const Position& coord, const int floor) const
{
    const auto movement = input::makeMovementIntent(shearedTarget(coord, floor), false);
    return {
        movement,
        input::makeSmartClick(movement),
        std::make_shared<input::TGameActionTeleportToCoordinate>(coord),
        std::make_shared<input::TGameActionContextMenu>(),
        std::make_shared<input::TGameActionShowDialogCreateMinimapMarker>(coord),
    };
}

input::TGameActionList TCyclopediaMapGameActionProvider::markerActions(const input::TMinimapMarkerData& marker, const int floor) const
{
    const Position& coord = marker.coordinate;
    const auto movement = input::makeMovementIntent(shearedTarget(coord, floor), false);
    return {
        movement,
        input::makeSmartClick(movement),
        std::make_shared<input::TGameActionTeleportToCoordinate>(coord),
        std::make_shared<input::TGameActionContextMenu>(),
        std::make_shared<input::TGameActionShowDialogEditMinimapMarker>(marker),
        std::make_shared<input::TGameActionDeleteMinimapMarker>(coord),
    };
}

}
