#pragma once

#include "container/containerciptypes.h"
#include "input/gameaction.h"

#include <cstdint>
#include <optional>

namespace tibia::container {

/// The game actions of the object slots the container panels show: the
/// inventory, a container window, the supply stash and the depot search. Each
/// builds the generic object actions of the slot's object, keeps the ones the
/// panel offers and adds the panel's own.
class TContainerGameActionProvider
{
public:
    /// An inventory slot's actions: CONTEXT_MENU, then the object actions the
    /// inventory offers. Empty for an empty slot.
    static input::TGameActionList inventorySlotActions(const TInventorySlot& slot, int slotId);
    /// A container window slot's actions: MOVE_UP when the container has a
    /// parent and the object is movable, CONTEXT_MENU, then the object actions
    /// container slots offer. Empty for an empty slot.
    static input::TGameActionList containerSlotActions(uint8_t containerId, int slotId);
    /// A supply stash entry's actions: CONTEXT_MENU, OPEN_CYCLOPEDIA, the loot
    /// list toggle, MARKET_BROWSE and, while some is stored, the retrieve dialog.
    static input::TGameActionList stashTypeActions(uint16_t typeId, uint32_t storedCount);
    /// An object type's actions: CONTEXT_MENU, OPEN_CYCLOPEDIA and MARKET_BROWSE.
    static input::TGameActionList typeActions(uint16_t typeId);
    /// A depot search depot (`inbox` false) or inbox item's actions: the
    /// object actions container slots offer, OPEN_PARENT_CONTAINER and
    /// CONTEXT_MENU. Empty for an empty slot.
    static input::TGameActionList depotSearchSlotActions(bool inbox, int index, const TInventorySlot& slot);
    /// The depot search's searched type: CONTEXT_MENU, OPEN_CYCLOPEDIA, the
    /// loot list toggle and MARKET_BROWSE on the searched tier.
    static input::TGameActionList depotSearchTypeActions(uint16_t typeId, std::optional<uint8_t> tier);
};

}
