#include "container/containergameactionprovider.h"

#include "analysers/analyserstorages.h"
#include "input/genericobjectgameactionprovider.h"
#include "quickloot/quicklootblackwhiteliststorage.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <algorithm>
#include <cstdint>

namespace tibia::container {

namespace {

using A = input::EGameAction;
using input::TGameActionList;

/// The object actions the inventory offers, in CipSoft's order.
constexpr A InventorySlotActionTypes[] = {
    A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
    A::MOVE_OBJECT, A::TRADE_ITEM, A::TARGET_SELECTED, A::SMART_CLICK, A::OPEN_MANAGE_LOOT_CONTAINER, A::LOOT_ADD_TO_BLACKWHITELIST,
    A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::STASH_STOW_CONTAINER_CONTENT, A::STASH_OPEN_STOW_STACK_DIALOG, A::MARKET_BROWSE,
    A::GET_TEXT_FOR_REPORT,
};

/// The object actions container and depot search slots offer, in CipSoft's order.
constexpr A ContainerSlotActionTypes[] = {
    A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
    A::WRAP, A::UNWRAP, A::ROTATE_OBJECT, A::CONFIGURE_SHOW_OFF_SOCKET, A::QUICK_LOOT, A::MOVE_OBJECT, A::TRADE_ITEM, A::TARGET_SELECTED,
    A::SMART_CLICK, A::OPEN_MANAGE_LOOT_CONTAINER, A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::STASH_STOW_TYPE,
    A::STASH_STOW_CONTAINER_CONTENT, A::STASH_OPEN_STOW_STACK_DIALOG, A::MARKET_BROWSE, A::CONTEXT_MENU_SEPARATOR, A::GET_TEXT_FOR_REPORT,
};

/// The most a retrieve dialog offers at once.
constexpr uint32_t MaximumRetrieveCount = 100;
constexpr uint32_t MaximumCumulativeRetrieveCount = 10000;

void append(TGameActionList& actions, TGameActionList more)
{
    actions.insert(actions.end(), std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
}

TAppearanceType* typeOf(const uint16_t typeId)
{
    return typeId ? tibia::game::rawThingType(typeId, ThingCategoryItem) : nullptr;
}

/// CONTEXT_MENU and OPEN_CYCLOPEDIA, which every type list offers.
TGameActionList typeActionsBeforeLoot(const TAppearanceType& type, const uint16_t typeId)
{
    TGameActionList actions{ input::makeGameAction(A::CONTEXT_MENU) };
    if (type.hasCyclopediaItem())
        actions.push_back(input::TGenericObjectGameActionProvider::openCyclopediaAction(typeId));
    return actions;
}

/// The loot list toggle of a pickupable, movable type that is not a corpse.
void addLootListToggle(TGameActionList& actions, const TAppearanceType& type, const uint16_t typeId)
{
    analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance();
    const quickloot::TQuickLootBlackWhitelistStorage* loot = storages ? storages->lootBlackWhitelist() : nullptr;
    if (loot && type.isPickupable() && !type.isNotMoveable() && !type.isCorpse() && !type.isPlayerCorpse()) {
        actions.push_back(std::make_shared<input::TGameActionToggleBlackWhitelistStatus>(
            loot->isListed(typeId) ? A::LOOT_REMOVE_FROM_BLACKWHITELIST : A::LOOT_ADD_TO_BLACKWHITELIST, typeId));
    }
}

void addMarketBrowse(TGameActionList& actions, const TAppearanceType& type, const uint16_t typeId, const uint8_t tier)
{
    const TContainerStorage* containers = TContainerStorage::instance();
    if (containers && containers->isMarketAvailable() && type.isMarketable())
        actions.push_back(std::make_shared<input::gameactions::TGameActionMarketBrowse>(
            input::TObjectGameActionContext{ {}, -1, typeId }, tier));
}

}

TGameActionList TContainerGameActionProvider::inventorySlotActions(const TInventorySlot& slot, const int slotId)
{
    if (slot.empty())
        return {};
    const Position position(UINT16_MAX, static_cast<uint16_t>(slotId), 0);
    TGameActionList actions{ input::makeGameAction(A::CONTEXT_MENU) };
    append(actions, input::filterActions(input::TGenericObjectGameActionProvider::objectActions(slot, position, 0),
                                         InventorySlotActionTypes));
    return actions;
}

TGameActionList TContainerGameActionProvider::containerSlotActions(const uint8_t containerId, const int slotId)
{
    TContainerStorage* storage = TContainerStorage::instance();
    const TStoredContainer* row = storage ? storage->containerAt(containerId) : nullptr;
    if (!row || slotId < 0)
        return {};
    const TInventorySlot slot = storage->slotAt(containerId, slotId);
    if (slot.empty())
        return {};
    // The slot on the shown page, as the server addresses it.
    const int pageSlot = slotId - static_cast<int>(row->firstIndex());
    const Position position(UINT16_MAX, static_cast<uint16_t>(containerId | 0x40), static_cast<uint8_t>(pageSlot));
    TGameActionList actions;
    if (row->hasParent()) {
        const TAppearanceType* type = slot.appearance ? slot.appearance->thingType() : typeOf(slot.typeId);
        if (!type || !type->isNotMoveable())
            actions.push_back(input::makeObjectGameAction(A::MOVE_UP, position, pageSlot, slot.typeId));
    }
    actions.push_back(input::makeGameAction(A::CONTEXT_MENU));
    append(actions, input::filterActions(input::TGenericObjectGameActionProvider::objectActions(slot, position, pageSlot),
                                         ContainerSlotActionTypes));
    return actions;
}

TGameActionList TContainerGameActionProvider::stashTypeActions(const uint16_t typeId, const uint32_t storedCount)
{
    TAppearanceType* type = typeOf(typeId);
    if (!type)
        return {};
    TGameActionList actions = typeActionsBeforeLoot(*type, typeId);
    addLootListToggle(actions, *type, typeId);
    addMarketBrowse(actions, *type, typeId, 0);
    if (storedCount != 0) {
        const uint32_t maximum = type->isStackable() ? MaximumCumulativeRetrieveCount : MaximumRetrieveCount;
        auto retrieve = std::make_shared<input::TGameActionStashRetrieve>(typeId, static_cast<int>(std::min(storedCount, maximum)));
        actions.push_back(std::make_shared<input::TGameActionStashOpenRetrieveDialog>(retrieve));
    }
    return actions;
}

TGameActionList TContainerGameActionProvider::typeActions(const uint16_t typeId)
{
    TAppearanceType* type = typeOf(typeId);
    if (!type)
        return {};
    TGameActionList actions = typeActionsBeforeLoot(*type, typeId);
    addMarketBrowse(actions, *type, typeId, 0);
    return actions;
}

TGameActionList TContainerGameActionProvider::depotSearchSlotActions(const bool inbox, const int index, const TInventorySlot& slot)
{
    if (slot.empty() || index < 0)
        return {};
    const input::TGameActionContextPtr location =
        inbox ? input::TGameActionContextPtr(std::make_shared<input::TLocationDepotSearchInboxItemGameActionContext>(index))
              : input::TGameActionContextPtr(std::make_shared<input::TLocationDepotSearchDepotItemGameActionContext>(index));
    TGameActionList actions = input::filterActions(input::TGenericObjectGameActionProvider::objectActions(location, slot),
                                                   ContainerSlotActionTypes);
    const TAppearanceType* type = slot.appearance ? slot.appearance->thingType() : typeOf(slot.typeId);
    const input::TObjectGameActionContext object{
        location, input::TGenericObjectGameActionProvider::objectContext(type, slot.typeId, slot.fluid, slot.tier, slot.decoItemId)
    };
    actions.push_back(std::make_shared<input::TGameActionOpenParentContainer>(object));
    actions.push_back(input::makeGameAction(A::CONTEXT_MENU));
    return actions;
}

TGameActionList TContainerGameActionProvider::depotSearchTypeActions(const uint16_t typeId, const std::optional<uint8_t> tier)
{
    TAppearanceType* type = typeOf(typeId);
    if (!type)
        return {};
    TGameActionList actions = typeActionsBeforeLoot(*type, typeId);
    addLootListToggle(actions, *type, typeId);
    addMarketBrowse(actions, *type, typeId, tier.value_or(0));
    return actions;
}

}
