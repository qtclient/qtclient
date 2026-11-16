#pragma once

#include "shared/cip_typed.h"

#include "input/gameaction.h"

namespace tibia {
namespace minimap {

/// The actions a minimap field offers (CIP: TMinimapGameActionProvider,
/// ): walk there, as a plain and a smart click, god teleport,
/// the context menu and a new marker.
class TMinimapGameActionProvider : public ::tibia::CipTyped
{
public:
    /// The actions of a field: the field's marker decides which list.
    input::TGameActionList actionsAt(const Position& coord) const;
    /// A field without a marker: walk there (also as the smart click), god
    /// teleport, the context menu and a new marker.
    input::TGameActionList fieldActions(const Position& coord) const;
    /// A field with a marker: as a plain field, but the marker is edited or
    /// deleted instead of created.
    input::TGameActionList markerActions(const Position& coord) const;
};

}
}
