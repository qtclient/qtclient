#include "minimapgameactionprovider.h"

#include "minimap/minimapmarkerstorage.h"

namespace tibia::minimap {

using A = input::EGameAction;

namespace {

/// The marker at the field as the marker actions carry it.
input::TMinimapMarkerData markerData(const Position& coord)
{
    input::TMinimapMarkerData data{ coord, 0, {} };
    if (TMinimapMarkerStorage* markers = TMinimapMarkerStorage::active()) {
        if (const std::optional<MinimapMarker> marker = markers->markerAt(coord)) {
            data.icon = marker->icon;
            data.description = QString::fromStdString(marker->description);
        }
    }
    return data;
}

}

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
        std::make_shared<input::TGameActionShowDialogCreateMinimapMarker>(coord),
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
        std::make_shared<input::TGameActionShowDialogEditMinimapMarker>(markerData(coord)),
        std::make_shared<input::TGameActionDeleteMinimapMarker>(coord),
    };
}

}
