#include "minimapgameactionprovider.h"

#include "minimap/minimapmarkerstorage.h"

namespace tibia::minimap {

input::TGameActionList TMinimapGameActionProvider::actionsAt(const Position& coord) const
{
    using A = input::EGameAction;
    const input::TGameActionPtr movement = input::makeMovementIntent(coord, 0, false);
    input::TGameActionList list{
        movement,
        input::makeSmartClick(movement),
        // CIP: GOD_TELEPORT is a mapper candidate (Shift+Ctrl); qtclient menus omit it.
        input::makeObjectAction(A::GOD_TELEPORT, coord, -1),
        input::makeAction(A::CONTEXT_MENU),
    };
    if (TMinimapMarkerStorage* markers = TMinimapMarkerStorage::active(); markers && markers->markerAt(coord)) {
        list.push_back(input::makeObjectAction(A::SHOW_DIALOG_EDIT_MINIMAP_MARKER, coord, -1));
        list.push_back(input::makeObjectAction(A::DELETE_MINIMAP_MARKER, coord, -1));
    } else {
        list.push_back(input::makeObjectAction(A::SHOW_DIALOG_CREATE_MINIMAP_MARKER, coord, -1));
    }
    return list;
}

}
