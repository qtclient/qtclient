#pragma once

#include "shared/cip_typed.h"

#include "container/containerciptypes.h"
#include "gameaction.h"
#include "worldmap/worldmappicker.h"

namespace tibia::creatures {
class TCreature;
}

namespace tibia::input {

/// The actions an object or creature offers (CIP: TGenericObjectGameActionProvider).
/// World-map context only; container and inventory contexts come with their panels.
class TGenericObjectGameActionProvider : public ::tibia::CipTyped
{
public:
    /// v4: what can be done to a creature. Id 0 or an unknown creature gives
    /// only TEMPORARY_SELECT_CREATURE(0).
    static TGameActionList creatureActions(uint32_t creatureId);
    /// v5: the SMART_CLICK on a creature.
    static TGameActionPtr creatureSmartClick(uint32_t creatureId);
    /// CIP world-map field stack (appearance + stackpos; creatures from Map registry).
    static TGameActionList objectActions(const worldmap::TFieldObject& object, const worldmap::TWorldMapField* field,
                                         const Position& position);
    /// Inventory/container slot from CIP storage (typeId path; no ThingPtr keep-alive).
    static TGameActionList objectActions(const container::TInventorySlot& slot, const Position& position,
                                         int stackPosition);
};

/// CIP's hireling: an NPC whose mapped inspection type is 4 (raw 7).
bool isHireling(const tibia::creatures::TCreature& stored);

}
