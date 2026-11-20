#pragma once

#include "shared/cip_typed.h"

#include "input/gameaction.h"

namespace tibia {
namespace cyclopedia {

/// The actions a field of the Cyclopedia map offers (CIP:
/// TCyclopediaMapGameActionProvider): those of a minimap field, but the walk
/// goes to the field as drawn on the floor the map shows.
class TCyclopediaMapGameActionProvider : public ::tibia::CipTyped
{
public:
    /// A field without a marker: walk to the field sheared onto `floor` (also as
    /// the smart click), god teleport to the field, the context menu and a new
    /// marker on the field.
    input::TGameActionList fieldActions(const Position& coord, int floor) const;
    /// A field with a marker: as a plain field, but the marker is edited or
    /// deleted instead of created.
    input::TGameActionList markerActions(const Position& coord, int floor) const;
};

}
}
