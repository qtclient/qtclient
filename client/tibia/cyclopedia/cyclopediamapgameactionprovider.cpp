#include "cyclopediamapgameactionprovider.h"

#include "minimap/minimapmarkerstorage.h"
#include "worldmap/coordinates.h"

namespace tibia::cyclopedia {

namespace {

/// The walk target: the field drawn at `coord` when the map shows `floor`.
Position shearedTarget(const Position& coord, const int floor)
{
    const worldmap::TWorldMapCoordinate sheared = worldmap::TWorldMapCoordinate(coord).shearedToFloor(floor);
    return Position(sheared.x, sheared.y, static_cast<uint8_t>(sheared.z));
}

/// The marker at the field as the marker actions carry it.
input::TMinimapMarkerData markerData(const Position& coord)
{
    input::TMinimapMarkerData data{ coord, 0, {} };
    if (minimap::TMinimapMarkerStorage* markers = minimap::TMinimapMarkerStorage::active()) {
        if (const minimap::TMinimapMarkerPtr marker = markers->markerAt(worldmap::TWorldMapCoordinate(coord))) {
            data.icon = marker->icon;
            data.description = marker->description;
        }
    }
    return data;
}

}

input::TGameActionList TCyclopediaMapGameActionProvider::fieldActions(const Position& coord, const int floor) const
{
    const auto movement = input::makeMovementIntent(shearedTarget(coord, floor), 0, false);
    return {
        movement,
        input::makeSmartClick(movement),
        std::make_shared<input::TGameActionTeleportToCoordinate>(coord),
        std::make_shared<input::TGameActionContextMenu>(),
        std::make_shared<input::TGameActionShowDialogCreateMinimapMarker>(coord),
    };
}

input::TGameActionList TCyclopediaMapGameActionProvider::markerActions(const Position& coord, const int floor) const
{
    const auto movement = input::makeMovementIntent(shearedTarget(coord, floor), 0, false);
    return {
        movement,
        input::makeSmartClick(movement),
        std::make_shared<input::TGameActionTeleportToCoordinate>(coord),
        std::make_shared<input::TGameActionContextMenu>(),
        std::make_shared<input::TGameActionShowDialogEditMinimapMarker>(markerData(coord)),
        std::make_shared<input::TGameActionDeleteMinimapMarker>(coord),
    };
}

}
