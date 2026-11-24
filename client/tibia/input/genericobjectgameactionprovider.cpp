#include "genericobjectgameactionprovider.h"

#include "input/genericchatgameactionprovider.h"

#include "appearances/appearanceinstance.h"
#include "creatures/creaturestorage.h"
#include "creatures/inspectplayergameactionprovider.h"
#include "creatures/partygameactionprovider.h"
#include "game/gameciptypes.h"
#include "quickloot/quicklootblackwhiteliststorage.h"

#include "protocol/protocolcodes.h"
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <stdexcept>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"

namespace tibia::input {

namespace {

using A = EGameAction;

/// CIP's party shields: member or leader, with or without shared experience.
bool isPartyMemberShield(const uint8_t shield)
{
    return shield >= 3 && shield <= 10;
}

}

bool isHireling(const tibia::creatures::TCreature& stored)
{
    return stored.isNpc() && stored.inspectionType() == 7;
}

TGenericObjectGameActionProvider::TGenericObjectGameActionProvider(
    std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
    std::shared_ptr<TGenericChatGameActionProvider> genericChatGameActionProvider,
    std::shared_ptr<creatures::TPartyGameActionProvider> partyGameActionProvider,
    std::shared_ptr<creatures::TInspectPlayerGameActionProvider> inspectPlayerGameActionProvider,
    std::shared_ptr<quickloot::TQuickLootBlackWhitelistStorage> quickLootBlackWhitelistStorage)
    : m_creatureStorage(std::move(creatureStorage))
    , m_genericChatGameActionProvider(std::move(genericChatGameActionProvider))
    , m_partyGameActionProvider(std::move(partyGameActionProvider))
    , m_inspectPlayerGameActionProvider(std::move(inspectPlayerGameActionProvider))
    , m_quickLootBlackWhitelistStorage(std::move(quickLootBlackWhitelistStorage))
{
}

void TGenericObjectGameActionProvider::setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage)
{
    m_premiumTimeStorage = premiumTimeStorage;
}

TGameActionList TGenericObjectGameActionProvider::creatureActions(const uint32_t creatureId) const
{
    const std::shared_ptr<tibia::creatures::TCreature> stored =
        creatureId && m_creatureStorage ? m_creatureStorage->creatureAt(creatureId) : nullptr;
    if (!stored)
        return { makeCreatureAction(A::TEMPORARY_SELECT_CREATURE, 0) };

    TGameActionList actions;
    const auto push = [&](const EGameAction type) { actions.push_back(makeCreatureAction(type, creatureId)); };
    const bool own = stored->isLocalPlayer();
    const bool hireling = isHireling(*stored);
    const QString name = stored->name();

    const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    const uint32_t attackingId = session ? session->attackingCreatureId() : 0;
    const uint32_t followingId = session ? session->followingCreatureId() : 0;

    if (!own) {
        if (stored->isNpc()) {
            push(A::START_TALKING_TO_NPC);
            if (hireling) {
                actions.push_back(TGameActionRequestOutfitsFromServer::forHireling(creatureId));
                actions.push_back(std::make_shared<TGameActionReportHireling>(A::REPORT_HIRELING_NAME, name, creatureId));
                actions.push_back(std::make_shared<TGameActionBuyHirelingNameOrSexChange>());
            }
        } else {
            push(attackingId == creatureId ? A::STOP_ATTACKING_CREATURE : A::ATTACK_CREATURE);
        }
        if (!hireling)
            push(followingId == creatureId ? A::STOP_FOLLOWING_CREATURE : A::FOLLOW_CREATURE);
    } else {
        actions.push_back(std::make_shared<TGameActionRequestOutfitsFromServer>());
        push(stored->outfit().getMount() == 0 ? A::MOUNT_PLAYER : A::UNMOUNT_PLAYER);
        push(A::OPEN_PREY_DIALOG);
    }

    if (stored->isPlayer() && !own) {
        // The chat actions on the name, then the bot report by name.
        if (m_genericChatGameActionProvider)
            m_genericChatGameActionProvider->appendGameActions(actions, name);
        actions.push_back(std::make_shared<TGameActionReportCharacter>(A::REPORT_BOT, name));
    }
    if (stored->isPlayer()) {
        if (m_partyGameActionProvider)
            m_partyGameActionProvider->appendActions(actions, creatureId);
        if (m_inspectPlayerGameActionProvider)
            m_inspectPlayerGameActionProvider->appendActions(actions, creatureId);
    }

    push(A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD);
    push(A::TARGET_SELECTED);
    push(A::LOOK_AT_CREATURE);
    actions.push_back(creatureSmartClick(creatureId));
    push(A::TEMPORARY_SELECT_CREATURE);
    return actions;
}

IGameActionPtr TGenericObjectGameActionProvider::creatureSmartClick(const uint32_t creatureId) const
{
    const std::shared_ptr<tibia::creatures::TCreature> stored =
        creatureId && m_creatureStorage ? m_creatureStorage->creatureAt(creatureId) : nullptr;
    EGameAction inner = A::LOOK_AT_CREATURE;
    if (stored) {
        if (stored->isNpc())
            inner = A::START_TALKING_TO_NPC;
        else if (!stored->isLocalPlayer() && !isPartyMemberShield(stored->shield())) {
            const tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
            const uint32_t attackingId = session ? session->attackingCreatureId() : 0;
            inner = attackingId == creatureId ? A::STOP_ATTACKING_CREATURE : A::ATTACK_CREATURE;
        }
    }
    return makeSmartClick(makeCreatureAction(inner, creatureId));
}

IGameActionPtr TGenericObjectGameActionProvider::openCyclopediaAction(const uint16_t clientId)
{
    auto action = std::make_shared<TGameActionOpenCyclopedia>(0);
    if (TAppearanceType* type = clientId ? tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, clientId) : nullptr)
        action->setTypeId(static_cast<uint16_t>(type->getCyclopediaType() ? type->getCyclopediaType() : type->getId()));
    return action;
}

namespace {

/// CIP's loot corpse: a container corpse that is not a
/// player's, of the object bank.
bool isLootCorpse(const TAppearanceType* type)
{
    return type && type->isContainer() && type->isCorpse() && !type->isPlayerCorpse() && type->isItem();
}

bool isMapLocation(const IGameActionContext* location)
{
    return location
        && location->gameActionContextType() == EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION;
}

const TLocationContainerWithSlotGameActionContext* containerLocation(const IGameActionContext* location)
{
    return location && location->gameActionContextType() == EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT
               ? static_cast<const TLocationContainerWithSlotGameActionContext*>(location)
               : nullptr;
}

/// The location of qtclient's classic object position. A container slot gets
/// the flags CIP's container provider (v3) sets: the window shows a loot corpse
/// (+0x10), it is the "Browse Field" one (TContainer +0x54).
TGameActionContextPtr locationOf(const Position& position, const int stackPosition)
{
    TGameActionContextPtr location = locationContextFromPosition(position, stackPosition);
    const TLocationContainerWithSlotGameActionContext* slot = containerLocation(location.get());
    if (!slot)
        return location;
    auto flagged = std::make_shared<TLocationContainerWithSlotGameActionContext>(*slot);
    const container::TContainerStorage* storage = tibia::game::currentContainerStorage();
    if (const container::TStoredContainer* row = storage ? storage->containerAt(slot->containerId()) : nullptr) {
        const container::TInventorySlot& item = row->containerItem();
        const TAppearanceType* type = item.appearance ? item.appearance->thingType()
                                                : (item.typeId ? tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, item.typeId) : nullptr);
        flagged->setContainerIsLootCorpse(isLootCorpse(type));
        flagged->setContainerIsBrowseField(row->name() == QLatin1String("Browse Field"));
    }
    return flagged;
}

} // namespace

TGameActionList TGenericObjectGameActionProvider::objectActionsImpl(const TGameActionContextPtr& location,
                                                                    const TGameActionContextPtr& objectContext,
                                                                    const uint32_t creatureId, const uint8_t fieldTier,
                                                                    TAppearanceType* flags, const uint16_t fieldCount,
                                                                    const appearances::TAppearanceInstance* instance) const
{
    TGameActionList actions;
    const uint16_t clientId = appearanceTypeIdOf(objectContext.get());
    if (!clientId)
        return actions;
    const uint32_t id = clientId;
    const TObjectGameActionContext object{ location, objectContext };
    const auto make = [&](const EGameAction type) -> IGameActionPtr { return makeObjectGameAction(type, object); };
    const auto push = [&](const EGameAction type) {
        IGameActionPtr action = make(type);
        if (action) // makeObjectGameAction logs a type it has no object action for
            actions.push_back(action);
        return action;
    };
    const bool mapLocation = isMapLocation(location.get());
    const TLocationContainerWithSlotGameActionContext* container = containerLocation(location.get());

    const auto has = [flags](bool (TAppearanceType::*flag)() const) { return flags && (flags->*flag)(); };
    const bool isContainer = has(&TAppearanceType::isContainer);
    const bool take = has(&TAppearanceType::isPickupable);
    const bool unmove = has(&TAppearanceType::isNotMoveable);
    const bool podium = has(&TAppearanceType::isPodium);
    // The object's count: a cumulative one's stack, 1 for any other.
    const int count = has(&TAppearanceType::isStackable) ? static_cast<int>(fieldCount) : 1;

    const IGameActionPtr look = push(A::LOOK);
    if (take && !has(&TAppearanceType::isIgnoreLook))
        push(A::INSPECT_OBJECT);

    IGameActionPtr use;
    if (!isContainer) {
        use = push(has(&TAppearanceType::isMultiUse) ? A::USE_WITH : A::USE);
    } else {
        use = push(A::OPEN);
        if (id != tibia::appearances::TAppearancesManager::instance().getSupplyStashId()) {
            // A container inside a container opens in its window by default;
            // anywhere else in a new one.
            const IGameActionPtr openInNewWindow = push(A::OPEN_IN_NEW_WINDOW);
            if (!container)
                use = openInNewWindow;
        }
        // CIP: a TGameActionSimple; the dialog takes no object context.
        actions.push_back(makeGameAction(A::OPEN_MANAGE_LOOT_CONTAINER));
    }
    if (id == tibia::appearances::TAppearancesManager::instance().getStandardRewardChestId())
        push(A::GATHER_ALL);
    if (has(&TAppearanceType::isWrapable))
        push(A::WRAP);
    else if (has(&TAppearanceType::isUnwrapable))
        push(A::UNWRAP);
    if (podium)
        push(A::CONFIGURE_SHOW_OFF_SOCKET);
    if (has(&TAppearanceType::isRotateable) || podium)
        push(A::ROTATE_OBJECT);
    if (has(&TAppearanceType::isReportable))
        actions.push_back(std::make_shared<TGameActionGetTextForReport>(object));
    if (take && !unmove)
        push(A::TRADE_ITEM);
    // The loot list offers a pickupable object that is not a corpse.
    if (const quickloot::TQuickLootBlackWhitelistStorage* loot = m_quickLootBlackWhitelistStorage.get();
        loot && take && !unmove && !has(&TAppearanceType::isCorpse) && !has(&TAppearanceType::isPlayerCorpse)) {
        actions.push_back(std::make_shared<TGameActionToggleBlackWhitelistStatus>(
            loot->isListed(clientId) ? A::LOOT_REMOVE_FROM_BLACKWHITELIST : A::LOOT_ADD_TO_BLACKWHITELIST, clientId));
    }
    if (has(&TAppearanceType::hasCyclopediaItem)) {
        auto cyclopedia = std::make_shared<TGameActionOpenCyclopedia>(0);
        cyclopedia->setTypeId(static_cast<uint16_t>(flags->getCyclopediaType()));
        actions.push_back(cyclopedia);
    }
    if (has(&TAppearanceType::hasProficiency))
        actions.push_back(std::make_shared<gameactions::TGameActionOpenWeaponProficiencyDialog>(clientId));
    actions.push_back(std::make_shared<TGameActionMoveObject>(object, std::nullopt, false, count));

    // A creature is targeted as itself, except the player's own one.
    const std::shared_ptr<tibia::creatures::TCreature> creature =
        creatureId && m_creatureStorage ? m_creatureStorage->creatureAt(creatureId) : nullptr;
    const bool ownCreature = creature && creature->isLocalPlayer();
    if (creatureId && !ownCreature)
        actions.push_back(std::make_shared<TGameActionTargetSelected>(
            TObjectGameActionContext{ location, std::make_shared<TObjectCreatureGameActionContext>(creatureId) }));
    else
        actions.push_back(make(A::TARGET_SELECTED));

    // The lens help of a world map object: a creature's is 1000 (1001 the
    // player's own), a container without one has 1002.
    if (mapLocation) {
        int lensHelp = 0;
        if (creatureId) {
            lensHelp = 1000 | (ownCreature ? 1 : 0);
        } else {
            lensHelp = flags ? flags->getLensHelp() : 0;
            if (lensHelp == 0 && isContainer)
                lensHelp = 1002;
        }
        if (lensHelp != 0)
            actions.push_back(std::make_shared<TGameActionShowLenshelp>(lensHelp, object.position(), QRectF()));
    }

    // Quick loot: a loot corpse on the map or in the browse field window, and
    // a premium player's any object inside an open loot corpse.
    const bool lootCorpse = isLootCorpse(flags);
    if ((mapLocation && lootCorpse) || (container && lootCorpse && container->containerIsBrowseField())
        || (container && container->containerIsLootCorpse() && m_premiumTimeStorage && m_premiumTimeStorage->isPremium()))
        push(A::QUICK_LOOT);

    IGameActionPtr inner;
    if (!has(&TAppearanceType::hasAction)) {
        if (creatureId) {
            // v5 adds the creature's smart click itself.
            actions.push_back(creatureSmartClick(creatureId));
        } else {
            inner = has(&TAppearanceType::isUsable) ? use : look;
        }
    } else {
        switch (flags->getDefaultAction()) {
        case PLAYER_ACTION_NONE:
        case PLAYER_ACTION_LOOK: inner = look; break;
        case PLAYER_ACTION_USE:
        case PLAYER_ACTION_OPEN: inner = use; break;
        case PLAYER_ACTION_AUTOWALK_HIGHLIGHT:
            inner = mapLocation ? IGameActionPtr(makeMovementIntent(object.position())) : look;
            break;
        default: throw std::logic_error("missing case statement (AppearanceType.defaultAction())");
        }
    }
    if (inner)
        actions.push_back(makeSmartClick(inner));

    const container::TContainerStorage* containers = tibia::game::currentContainerStorage();
    if (containers && containers->isStashAvailable()) {
        if (has(&TAppearanceType::isMarketable) && TGenericObjectGameActionProvider::canStowObject(flags, instance)) {
            push(A::STASH_STOW_TYPE);
            auto stow = std::make_shared<TGameActionStashStowStack>(object, count);
            actions.push_back(std::make_shared<TGameActionStashOpenStowStackDialog>(stow));
        }
        if (isContainer && id != tibia::appearances::TAppearancesManager::instance().getSupplyStashId())
            push(A::STASH_STOW_CONTAINER_CONTENT);
    }
    if (containers && containers->isMarketAvailable() && has(&TAppearanceType::isMarketable))
        actions.push_back(std::make_shared<gameactions::TGameActionMarketBrowse>(
            object, flags->hasUpgradeClassification() ? fieldTier : uint8_t(0)));
    return actions;
}

bool TGenericObjectGameActionProvider::canStowObject(const TAppearanceType* type,
                                                     const appearances::TAppearanceInstance* instance)
{
    using namespace appearances;
    if (type && type->isStackable())
        return true;
    if (!instance || dynamic_cast<const TContainerObjectAppearanceInstance*>(instance))
        return false;
    if (const auto* expire = dynamic_cast<const TExpireObjectAppearanceInstance*>(instance))
        return expire->isBrandNew();
    if (dynamic_cast<const TLiquidObjectAppearanceInstance*>(instance))
        return false;
    if (const auto* upgradeable = dynamic_cast<const TUpgradeableObjectAppearanceInstance*>(instance))
        return upgradeable->tier() == 0;
    if (const auto* wearout = dynamic_cast<const TWearoutObjectAppearanceInstance*>(instance))
        return wearout->isBrandNew();
    if (dynamic_cast<const TPatternObjectAppearanceInstance*>(instance))
        return true;
    if (dynamic_cast<const TDecoItemKitAppearanceInstance*>(instance))
        return false;
    return dynamic_cast<const THangableObjectAppearanceInstance*>(instance) != nullptr;
}

TGameActionContextPtr TGenericObjectGameActionProvider::objectContext(const TAppearanceType* type, const uint16_t appearanceTypeId,
                                                                      const uint32_t liquidType, const uint8_t tier,
                                                                      const uint16_t decoItemId)
{
    if (type && type->isFluidContainer())
        return std::make_shared<TLiquidObjectApperanceTypeGameActionContext>(appearanceTypeId, liquidType);
    if (type && type->getClassification() > 0)
        return std::make_shared<TUpgradeableObjectApperanceTypeGameActionContext>(appearanceTypeId, tier);
    if (type && type->isDecoKit())
        return std::make_shared<TDecoItemKitObjectApperanceTypeGameActionContext>(appearanceTypeId, decoItemId);
    return std::make_shared<TObjectApperanceTypeGameActionContext>(appearanceTypeId);
}

TGameActionList TGenericObjectGameActionProvider::objectActions(const worldmap::TFieldObject& object,
                                                                const worldmap::TWorldMapField* field,
                                                                const Position& position) const
{
    (void)field;
    // A creature is the object of its outfit's appearance.
    appearances::TAppearanceInstancePtr appearance = object.appearance;
    if (!appearance && object.creatureId && m_creatureStorage) {
        if (const std::shared_ptr<tibia::creatures::TCreature> row = m_creatureStorage->creatureAt(object.creatureId))
            appearance = row->outfitAppearance();
    }
    if (!appearance)
        return {};
    TAppearanceType* type = appearance->thingType();
    const auto* instance = dynamic_cast<const appearances::TObjectAppearanceInstance*>(appearance.get());
    const auto* liquid = dynamic_cast<const appearances::TLiquidObjectAppearanceInstance*>(appearance.get());
    // The map instances keep no decoration kit item (render's TDecoItemKitAppearanceInstance).
    const TGameActionContextPtr context =
        objectContext(type, appearance->id(), liquid ? liquid->fluid() : 0, instance ? instance->tier() : 0, 0);
    return objectActionsImpl(locationOf(position, object.stackPosition), context, object.creatureId,
                             instance ? instance->tier() : 0, type, instance ? instance->count() : 0, appearance.get());
}

TGameActionList TGenericObjectGameActionProvider::objectActions(const container::TInventorySlot& slot,
                                                                const Position& position, const int stackPosition) const
{
    return objectActions(locationOf(position, stackPosition), slot);
}

TGameActionList TGenericObjectGameActionProvider::objectActions(const TGameActionContextPtr& location,
                                                                const container::TInventorySlot& slot) const
{
    if (slot.empty())
        return {};
    TAppearanceType* type = slot.appearance ? slot.appearance->thingType() : nullptr;
    if (!type)
        type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, slot.typeId);
    // Inventory and container slots own the count; appearance instances can keep a
    // stale shared one.
    const TGameActionContextPtr context = objectContext(type, slot.typeId, slot.fluid, slot.tier, slot.decoItemId);
    return objectActionsImpl(location, context, 0, slot.tier, type, slot.stackable ? slot.count : 0,
                             slot.appearance.get());
}

}
