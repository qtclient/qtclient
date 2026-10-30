#pragma once

#include "shared/cip_typed.h"

#include "input/gameaction.h"

namespace tibia::minimap {

/// The actions a minimap field offers (CIP: TMinimapGameActionProvider,
/// FUN_100ebb550): walk there, as a plain and a smart click, god teleport,
/// the context menu and a new marker.
class TMinimapGameActionProvider : public ::tibia::CipTyped
{
public:
    input::TGameActionList actionsAt(const Position& coord) const;
};

}
