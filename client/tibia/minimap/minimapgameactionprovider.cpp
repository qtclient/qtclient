#include "minimapgameactionprovider.h"

#include "minimap/minimapmarkerstorage.h"

namespace tibia::minimap {

using A = input::EGameAction;

input::TGameActionList TMinimapGameActionProvider::actionsAt(const Position& coord, const TMinimapMarkerStorage* markers) const
{
    if (markers && markers->markerAt(worldmap::TWorldMapCoordinate(coord)))
        return markerActions(markers->markerData(coord));
    return fieldActions(coord);
}

input::TGameActionList TMinimapGameActionProvider::fieldActions(const Position& coord) const
{
    const auto movement = input::makeMovementIntent(coord, false);
    return {
        movement,
        input::makeSmartClick(movement),
        std::make_shared<input::TGameActionTeleportToCoordinate>(coord),
        std::make_shared<input::TGameActionContextMenu>(),
        std::make_shared<input::TGameActionShowDialogCreateMinimapMarker>(coord),
    };
}

input::TGameActionList TMinimapGameActionProvider::markerActions(const input::TMinimapMarkerData& marker) const
{
    const Position& coord = marker.coordinate;
    const auto movement = input::makeMovementIntent(coord, false);
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
