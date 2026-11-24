#pragma once


#include "input/gameaction.h"

namespace tibia {
namespace minimap {

class TMinimapMarkerStorage;

/// The actions a minimap field offers (CIP: TMinimapGameActionProvider,
/// ): walk there, as a plain and a smart click, god teleport,
/// the context menu and a new marker.
class TMinimapGameActionProvider
{
public:
    /// The actions of a field: its marker in `markers` decides which list.
    input::TGameActionList actionsAt(const Position& coord, const TMinimapMarkerStorage* markers) const;
    /// A field without a marker: walk there (also as the smart click), god
    /// teleport, the context menu and a new marker.
    input::TGameActionList fieldActions(const Position& coord) const;
    /// A field with a marker: as a plain field, but the marker is edited or
    /// deleted instead of created.
    input::TGameActionList markerActions(const input::TMinimapMarkerData& marker) const;
};

}
}
