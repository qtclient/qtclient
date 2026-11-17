#include "creatures/vipstorage.h"
#include "genericobjectgameactionprovider.h"

#include "appearances/appearanceinstance.h"
#include "app/clientoptions.h"
#include "creatures/creaturestorage.h"
#include "creatures/inspectplayergameactionprovider.h"
#include "creatures/partygameactionprovider.h"
#include "cyclopedia/iteminfostorage.h"

#include "protocol/protocolcodes.h"
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QJsonArray>
#include <stdexcept>
#include <QJsonObject>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::input {

namespace {

using A = EGameAction;

/// CIP's party shields: member or leader, with or without shared experience.
bool isPartyMemberShield(const uint8_t shield)
{
    return shield >= 3 && shield <= 10;
}

bool isVipName(const QString& name)
{
    for (const auto& [id, vip] : tibia::creatures::TVipStorage::entries()) {
        if (vip.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool isIgnoredName(const QString& name)
{
    const QJsonArray names = app::ClientOptions::load()
                                 .root()
                                 .value(QLatin1String("whiteBlacklistOptions"))
                                 .toObject()
                                 .value(QLatin1String("blacklistedNames"))
                                 .toArray();
    for (const QJsonValue& entry : names) {
        if (entry.toString().compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

}

bool isHireling(const tibia::creatures::TCreature& stored)
{
    return stored.isNpc() && stored.inspectionType() == 7;
}

TGameActionList TGenericObjectGameActionProvider::creatureActions(const uint32_t creatureId)
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> stored =
        creatureId && storage ? storage->creatureAt(creatureId) : nullptr;
    if (!stored)
        return { makeCreatureAction(A::TEMPORARY_SELECT_CREATURE, 0) };

    TGameActionList actions;
    const auto push = [&](const EGameAction type) { actions.push_back(makeCreatureAction(type, creatureId)); };
    const bool own = stored->isLocalPlayer();
    const bool hireling = isHireling(*stored);
    const QString name = stored->name();

    const uint32_t attackingId = tibia::game::attackingCreatureId();
    const uint32_t followingId = tibia::game::getFollowingCreatureId();

    if (!own) {
        if (stored->isNpc()) {
            push(A::START_TALKING_TO_NPC);
            if (hireling) {
                actions.push_back(TGameActionRequestOutfitsFromServer::forHireling(creatureId));
                actions.push_back(std::make_shared<TGameActionReportHireling>(A::REPORT_HIRELING_NAME, name, creatureId));
                actions.push_back(std::make_shared<TGameActionBuyHirelingNameOrSexChange>(creatureId));
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
        // TGenericChatGameActionProvider, then the bot report by name.
        actions.push_back(std::make_shared<TGameActionOpenMessageToChatChannel>(name));
        actions.push_back(std::make_shared<TGameActionInvitePlayerToPrivateChatChannel>(0, name));
        actions.push_back(std::make_shared<TGameActionReportCharacter>(A::REPORT_NAME, name));
        if (isIgnoredName(name))
            actions.push_back(std::make_shared<TGameActionUnignoreName>(name));
        else
            actions.push_back(std::make_shared<TGameActionIgnoreName>(name));
        if (!isVipName(name))
            actions.push_back(std::make_shared<TGameActionAddNewVip>(name));
        actions.push_back(std::make_shared<TGameActionReportCharacter>(A::REPORT_BOT, name));
    }
    if (stored->isPlayer()) {
        creatures::TPartyGameActionProvider::appendActions(actions, creatureId);
        creatures::TInspectPlayerGameActionProvider::appendActions(actions, creatureId);
    }

    push(A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD);
    push(A::TARGET_SELECTED);
    push(A::LOOK_AT_CREATURE);
    actions.push_back(creatureSmartClick(creatureId));
    push(A::TEMPORARY_SELECT_CREATURE);
    return actions;
}

IGameActionPtr TGenericObjectGameActionProvider::creatureSmartClick(const uint32_t creatureId)
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> stored =
        creatureId && storage ? storage->creatureAt(creatureId) : nullptr;
    EGameAction inner = A::LOOK_AT_CREATURE;
    if (stored) {
        if (stored->isNpc())
            inner = A::START_TALKING_TO_NPC;
        else if (!stored->isLocalPlayer() && !isPartyMemberShield(stored->shield())) {
            const uint32_t attackingId = tibia::game::attackingCreatureId();
            inner = attackingId == creatureId ? A::STOP_ATTACKING_CREATURE : A::ATTACK_CREATURE;
        }
    }
    return makeSmartClick(makeCreatureAction(inner, creatureId));
}

IGameActionPtr TGenericObjectGameActionProvider::openCyclopediaAction(const uint16_t clientId)
{
    auto action = std::make_shared<TGameActionOpenCyclopedia>(0);
    if (ThingType* type = clientId ? tibia::game::thingType(clientId, ThingCategoryItem).get() : nullptr)
        action->setTypeId(static_cast<uint16_t>(type->getCyclopediaType() ? type->getCyclopediaType() : type->getId()));
    return action;
}

namespace {

/// CIP's loot corpse (sub_85bb787de76f): a container corpse that is not a
/// player's, of the object bank.
bool isLootCorpse(const ThingType* type)
{
    return type && type->isContainer() && type->isCorpse() && !type->isPlayerCorpse() && type->isItem();
}

/// The CIP location kinds of qtclient's object positions: the world map (11),
/// a container slot (1) or the inventory.
bool isMapLocation(const Position& position)
{
    return position.x != UINT16_MAX;
}

bool isContainerLocation(const Position& position)
{
    return position.x == UINT16_MAX && (position.y & 0x40);
}

/// CIP TLocationContainerWithSlotGameActionContext +0x10 / +0x11, set by
/// TContainerGameActionProvider::v3: the container is a loot corpse, the
/// container is the "Browse Field" one (TContainer +0x54).
struct TContainerLocationFlags
{
    bool containerIsLootCorpse = false;
    bool containerIsBrowseField = false;
};

TContainerLocationFlags containerLocationFlags(const Position& position)
{
    TContainerLocationFlags flags;
    if (!isContainerLocation(position))
        return flags;
    const container::TContainerStorage* storage = container::TContainerStorage::instance();
    const container::TStoredContainer* row = storage ? storage->containerAt(position.y & 0x3f) : nullptr;
    if (!row)
        return flags;
    const container::TInventorySlot& item = row->containerItem;
    const ThingType* type = item.appearance ? item.appearance->thingType()
                                            : (item.typeId ? tibia::game::rawThingType(item.typeId, ThingCategoryItem) : nullptr);
    flags.containerIsLootCorpse = isLootCorpse(type);
    flags.containerIsBrowseField = row->name == QLatin1String("Browse Field");
    return flags;
}

/// CIP v6: what can be done to an object, in CipSoft's order.
TGameActionList objectActionsImpl(const uint16_t clientId, const appearances::TObjectAppearanceInstance* fieldObject,
                                  ThingType* flags, const Position& position, const int stackPosition,
                                  const int stackCountOverride = -1)
{
    TGameActionList actions;
    if (!clientId)
        return actions;
    const uint32_t id = clientId;
    const auto make = [&](const EGameAction type) -> IGameActionPtr {
        return makeObjectGameAction(type, position, stackPosition, clientId);
    };
    const auto push = [&](const EGameAction type) {
        IGameActionPtr action = make(type);
        actions.push_back(action);
        return action;
    };
    const TObjectGameActionContext object{ position, stackPosition, clientId };

    const auto has = [flags](bool (ThingType::*flag)() const) { return flags && (flags->*flag)(); };
    const bool isContainer = has(&ThingType::isContainer);
    const bool take = has(&ThingType::isPickupable);
    const bool unmove = has(&ThingType::isNotMoveable);
    const bool podium = has(&ThingType::isPodium);
    const uint8_t fieldTier = fieldObject ? fieldObject->tier() : 0;
    // Inventory/container slots own the authoritative count; appearance instances can
    // retain a stale shared count and open the split dialog for the wrong total.
    const uint16_t fieldCount = stackCountOverride >= 0
                                    ? static_cast<uint16_t>(stackCountOverride)
                                    : (fieldObject ? fieldObject->count() : 0);
    // The object's count: a cumulative one's stack, 1 for any other.
    const int count = has(&ThingType::isStackable) ? static_cast<int>(fieldCount) : 1;

    const IGameActionPtr look = push(A::LOOK);
    if (take && !has(&ThingType::isIgnoreLook))
        push(A::INSPECT_OBJECT);

    IGameActionPtr use;
    if (!isContainer) {
        use = push(has(&ThingType::isMultiUse) ? A::USE_WITH : A::USE);
    } else {
        use = push(A::OPEN);
        if (id != tibia::game::supplyStashId()) {
            // A container inside a container opens in its window by default;
            // anywhere else in a new one.
            const IGameActionPtr openInNewWindow = push(A::OPEN_IN_NEW_WINDOW);
            if (!isContainerLocation(position))
                use = openInNewWindow;
        }
        push(A::OPEN_MANAGE_LOOT_CONTAINER);
    }
    if (id == tibia::game::standardRewardChestId())
        push(A::GATHER_ALL);
    if (has(&ThingType::isWrapable))
        push(A::WRAP);
    else if (has(&ThingType::isUnwrapable))
        push(A::UNWRAP);
    if (podium)
        push(A::CONFIGURE_SHOW_OFF_SOCKET);
    if (has(&ThingType::isRotateable) || podium)
        push(A::ROTATE_OBJECT);
    if (has(&ThingType::isReportable))
        actions.push_back(std::make_shared<TGameActionGetTextForReport>(object));
    if (take && !unmove)
        push(A::TRADE_ITEM);
    // The loot list offers a pickupable object that is not a corpse.
    if (cyclopedia::TItemInfoStorage* loot = cyclopedia::TItemInfoStorage::instance();
        loot && take && !unmove && !has(&ThingType::isCorpse) && !has(&ThingType::isPlayerCorpse)) {
        actions.push_back(std::make_shared<TGameActionToggleBlackWhitelistStatus>(
            loot->isLootListed(clientId) ? A::LOOT_REMOVE_FROM_BLACKWHITELIST : A::LOOT_ADD_TO_BLACKWHITELIST, clientId));
    }
    if (has(&ThingType::hasCyclopediaItem)) {
        auto cyclopedia = std::make_shared<TGameActionOpenCyclopedia>(0);
        cyclopedia->setTypeId(static_cast<uint16_t>(flags->getCyclopediaType()));
        actions.push_back(cyclopedia);
    }
    if (has(&ThingType::hasProficiency))
        actions.push_back(std::make_shared<gameactions::TGameActionOpenWeaponProficiencyDialog>(clientId));
    actions.push_back(std::make_shared<TGameActionMoveObject>(object, std::nullopt, false, count));
    actions.push_back(make(A::TARGET_SELECTED));

    // The lens help of a world map object; a container without one has 1002.
    if (isMapLocation(position)) {
        int lensHelp = flags ? flags->getLensHelp() : 0;
        if (lensHelp == 0 && isContainer)
            lensHelp = 1002;
        if (lensHelp != 0)
            actions.push_back(std::make_shared<TGameActionShowLenshelp>(lensHelp, position, QRectF()));
    }

    // Quick loot: a loot corpse on the map or in the browse field window.
    // CipSoft also offers it inside an open loot corpse when a provider member
    // (+0x58, set through v2) agrees; that member is not identified yet.
    const bool lootCorpse = isLootCorpse(flags);
    const TContainerLocationFlags container = containerLocationFlags(position);
    if ((isMapLocation(position) && lootCorpse) || (isContainerLocation(position) && lootCorpse && container.containerIsBrowseField))
        push(A::QUICK_LOOT);

    IGameActionPtr inner;
    if (!has(&ThingType::hasAction)) {
        inner = has(&ThingType::isUsable) ? use : look;
    } else {
        switch (flags->getDefaultAction()) {
        case PLAYER_ACTION_NONE:
        case PLAYER_ACTION_LOOK: inner = look; break;
        case PLAYER_ACTION_USE:
        case PLAYER_ACTION_OPEN: inner = use; break;
        case PLAYER_ACTION_AUTOWALK_HIGHLIGHT:
            inner = isMapLocation(position) ? IGameActionPtr(makeMovementIntent(position)) : look;
            break;
        default: throw std::logic_error("missing case statement (AppearanceType.defaultAction())");
        }
    }
    if (inner)
        actions.push_back(makeSmartClick(inner));

    if (tibia::game::localPlayerIsSupplyStashAvailable()) {
        if (has(&ThingType::isMarketable) && (has(&ThingType::isStackable) || fieldTier == 0)) {
            push(A::STASH_STOW_TYPE);
            auto stow = std::make_shared<TGameActionStashStowStack>(object, count);
            actions.push_back(std::make_shared<TGameActionStashOpenStowStackDialog>(stow));
        }
        if (isContainer && id != tibia::game::supplyStashId())
            push(A::STASH_STOW_CONTAINER_CONTENT);
    }
    if (tibia::game::localPlayerIsMarketAvailable() && has(&ThingType::isMarketable))
        actions.push_back(std::make_shared<gameactions::TGameActionMarketBrowse>(
            object, flags->getClassification() != 0 ? fieldTier : uint8_t(0)));
    return actions;
}

} // namespace

TGameActionList TGenericObjectGameActionProvider::objectActions(const worldmap::TFieldObject& object,
                                                                const worldmap::TWorldMapField* field,
                                                                const Position& position)
{
    (void)field;
    // CipSoft map creatures are moved as MOVE_OBJECT with wire id Proto::Creature (99 /
    // 0x63) — see Crystal AddCreature / playerMoveThing STACKPOS_MOVE. Previously
    // clientId stayed 0 so objectActionsImpl returned nothing and map drag never
    // offered MOVE_OBJECT (no crosshair / no startDrag) for creatures.
    if (object.creatureId) {
        TGameActionList actions;
        actions.push_back(std::make_shared<TGameActionMoveObject>(
            TObjectGameActionContext{ position, object.stackPosition, static_cast<uint16_t>(Proto::Creature), object.creatureId }));
        return actions;
    }
    uint16_t clientId = 0;
    const appearances::TObjectAppearanceInstance* objectInst = nullptr;
    ThingType* type = nullptr;
    if (object.appearance) {
        clientId = object.appearance->id();
        type = object.appearance->thingType();
        objectInst = dynamic_cast<const appearances::TObjectAppearanceInstance*>(object.appearance.get());
    }
    return objectActionsImpl(clientId, objectInst, type, position, object.stackPosition);
}

TGameActionList TGenericObjectGameActionProvider::objectActions(const container::TInventorySlot& slot,
                                                                const Position& position, const int stackPosition)
{
    if (slot.empty())
        return {};
    ThingType* type = slot.appearance ? slot.appearance->thingType() : nullptr;
    if (!type)
        type = tibia::game::rawThingType(slot.typeId, ThingCategoryItem);
    const appearances::TObjectAppearanceInstance* objectInst =
        slot.appearance ? dynamic_cast<const appearances::TObjectAppearanceInstance*>(slot.appearance.get()) : nullptr;
    const int stackCount = slot.stackable ? static_cast<int>(slot.count) : 0;
    return objectActionsImpl(slot.typeId, objectInst, type, position, stackPosition, stackCount);
}

}
