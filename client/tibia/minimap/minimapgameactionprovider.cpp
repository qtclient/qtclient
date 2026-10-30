#include "minimapgameactionprovider.h"

#include <client/minimap.h>

namespace tibia::minimap {

input::TGameActionList TMinimapGameActionProvider::actionsAt(const Position& coord) const
{
    using A = input::EGameAction;
    const input::TGameActionPtr movement = input::makeMovementIntent(coord);
    input::TGameActionList list{
        movement,
        input::makeSmartClick(movement),
        // CIP: GOD_TELEPORT is a mapper candidate (Shift+Ctrl); Soft menus omit it.
        input::makeObjectAction(A::GOD_TELEPORT, nullptr, coord, -1),
        input::makeAction(A::CONTEXT_MENU),
    };
    MinimapMarker existing;
    if (g_minimap.markerAt(coord, existing)) {
        list.push_back(input::makeObjectAction(A::SHOW_DIALOG_EDIT_MINIMAP_MARKER, nullptr, coord, -1));
        list.push_back(input::makeObjectAction(A::DELETE_MINIMAP_MARKER, nullptr, coord, -1));
    } else {
        list.push_back(input::makeObjectAction(A::SHOW_DIALOG_CREATE_MINIMAP_MARKER, nullptr, coord, -1));
    }
    return list;
}

}
