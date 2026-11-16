#include "minimapgameactionprovider.h"

#include "minimap/minimapmarkerstorage.h"

namespace tibia::minimap {

using A = input::EGameAction;

input::TGameActionList TMinimapGameActionProvider::actionsAt(const Position& coord) const
{
    if (TMinimapMarkerStorage* markers = TMinimapMarkerStorage::active(); markers && markers->markerAt(coord))
        return markerActions(coord);
    return fieldActions(coord);
}

input::TGameActionList TMinimapGameActionProvider::fieldActions(const Position& coord) const
{
    const input::TGameActionPtr movement = input::makeMovementIntent(coord, 0, false);
    return {
        movement,
        input::makeSmartClick(movement),
        input::makeObjectAction(A::GOD_TELEPORT, coord, -1),
        input::makeAction(A::CONTEXT_MENU),
        input::makeObjectAction(A::SHOW_DIALOG_CREATE_MINIMAP_MARKER, coord, -1),
    };
}

input::TGameActionList TMinimapGameActionProvider::markerActions(const Position& coord) const
{
    const input::TGameActionPtr movement = input::makeMovementIntent(coord, 0, false);
    return {
        movement,
        input::makeSmartClick(movement),
        input::makeObjectAction(A::GOD_TELEPORT, coord, -1),
        input::makeAction(A::CONTEXT_MENU),
        input::makeObjectAction(A::SHOW_DIALOG_EDIT_MINIMAP_MARKER, coord, -1),
        input::makeObjectAction(A::DELETE_MINIMAP_MARKER, coord, -1),
    };
}

}
