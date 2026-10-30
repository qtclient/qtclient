#pragma once

#include "shared/cip_typed.h"

#include "gameaction.h"

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
    static TGameActionPtr creatureSmartClick(const CreaturePtr& creature);
    /// v6: what can be done to an object at a world-map field and stack position.
    /// The stash actions need the supply stash and MARKET_BROWSE the market
    /// to be available (CIP: v6's last two arguments, TContainerStorage +0x40
    /// / +0x41 set from SpecialContainersAvailable by 0x100a31d30).
    static TGameActionList objectActions(const ThingPtr& object, const Position& position, int stackPosition);
};

/// CIP's hireling: an NPC whose mapped inspection type is 4 (raw 7).
bool isHireling(const CreaturePtr& creature);

}
