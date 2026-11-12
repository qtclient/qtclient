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
    input::TGameActionList actionsAt(const Position& coord) const;
};

}
}
