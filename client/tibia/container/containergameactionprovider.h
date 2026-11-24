#pragma once

#include "container/containerciptypes.h"
#include "input/gameaction.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace tibia::input {
class TGenericObjectGameActionProvider;
}
namespace tibia::quickloot {
class TQuickLootBlackWhitelistStorage;
}

namespace tibia::container {

/// The game actions of the object slots the container panels show: the
/// inventory, a container window, the supply stash and the depot search. Each
/// builds the generic object actions of the slot's object, keeps the ones the
/// panel offers and adds the panel's own. The session makes it after the
/// generic object provider.
class TContainerGameActionProvider
{
public:
    /// Host: CipSoft also hands the appearance storage; qtclient looks object
    /// types up in the loaded things.
    TContainerGameActionProvider(std::shared_ptr<TContainerStorage> containerStorage, std::shared_ptr<TStashStorage> stashStorage,
                                 std::shared_ptr<TDepotSearchDetailListStorage> depotSearchDetailListStorage,
                                 std::shared_ptr<quickloot::TQuickLootBlackWhitelistStorage> quickLootBlackWhitelistStorage,
                                 std::shared_ptr<input::TGenericObjectGameActionProvider> genericObjectGameActionProvider);
    virtual ~TContainerGameActionProvider() = default;

    /// An inventory slot's actions: CONTEXT_MENU, then the object actions the
    /// inventory offers. Empty for an empty slot.
    input::TGameActionList inventorySlotActions(const TInventorySlot& slot, int slotId) const;
    /// A container window slot's actions: MOVE_UP when the container has a
    /// parent and the object is movable, CONTEXT_MENU, then the object actions
    /// container slots offer. Empty for an empty slot.
    input::TGameActionList containerSlotActions(uint8_t containerId, int slotId) const;
    /// A supply stash entry's actions: CONTEXT_MENU, OPEN_CYCLOPEDIA, the loot
    /// list toggle, MARKET_BROWSE and, while some is stored, the retrieve dialog.
    input::TGameActionList stashTypeActions(uint16_t typeId, uint32_t storedCount) const;
    /// An object type's actions: CONTEXT_MENU, OPEN_CYCLOPEDIA and MARKET_BROWSE.
    input::TGameActionList typeActions(uint16_t typeId) const;
    /// A depot search depot (`inbox` false) or inbox item's actions: the
    /// object actions container slots offer, OPEN_PARENT_CONTAINER and
    /// CONTEXT_MENU. Empty for an empty slot.
    input::TGameActionList depotSearchSlotActions(bool inbox, int index, const TInventorySlot& slot) const;
    /// The depot search's searched type: CONTEXT_MENU, OPEN_CYCLOPEDIA, the
    /// loot list toggle and MARKET_BROWSE on the searched tier.
    input::TGameActionList depotSearchTypeActions(uint16_t typeId, std::optional<uint8_t> tier) const;

private:
    /// The loot list toggle of a pickupable, movable type that is not a corpse.
    void addLootListToggle(input::TGameActionList& actions, const TAppearanceType& type, uint16_t typeId) const;
    /// MARKET_BROWSE on a marketable type while the market is available.
    void addMarketBrowse(input::TGameActionList& actions, const TAppearanceType& type, uint16_t typeId, uint8_t tier) const;

    std::shared_ptr<TContainerStorage> m_containerStorage;
    std::shared_ptr<TStashStorage> m_stashStorage;
    std::shared_ptr<TDepotSearchDetailListStorage> m_depotSearchDetailListStorage;
    std::shared_ptr<quickloot::TQuickLootBlackWhitelistStorage> m_quickLootBlackWhitelistStorage;
    std::shared_ptr<input::TGenericObjectGameActionProvider> m_genericObjectGameActionProvider;
};

}
