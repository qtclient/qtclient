#include "genericobjectgameactionprovider.h"

#include "appearances/appearanceinstance.h"
#include "app/clientoptions.h"
#include "creatures/creaturestorage.h"
#include "cyclopedia/iteminfostorage.h"

#include <client/item.h>
#include "protocol/protocolcodes.h"
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QJsonArray>
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

bool isPartyLeaderShield(const uint8_t shield)
{
    using S = tibia::client::PlayerShields;
    return shield == S::ShieldWhiteYellow || shield == S::ShieldYellow || shield == S::ShieldYellowSharedExp
        || shield == S::ShieldYellowNoSharedExpBlink || shield == S::ShieldYellowNoSharedExp;
}

bool isSharedExpOn(const uint8_t shield)
{
    using S = tibia::client::PlayerShields;
    return shield == S::ShieldYellowSharedExp || shield == S::ShieldBlueSharedExp;
}

bool isVipName(const QString& name)
{
    for (const auto& [id, vip] : tibia::game::getVips()) {
        if (QString::fromStdString(std::get<0>(vip)).compare(name, Qt::CaseInsensitive) == 0)
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

TGameActionPtr namedCreatureAction(const EGameAction type, const QString& name)
{
    return std::make_shared<TGameActionCreatureWithName>(type, name);
}

/// qtclient party offers from local + target shields (storage snapshot at menu build).
void pushPartyActions(TGameActionList& actions, const tibia::creatures::TCreature& target, const uint32_t creatureId,
                      const tibia::creatures::TCreature* selfStored)
{
    if (!selfStored)
        return;
    const auto push = [&](const EGameAction type) { actions.push_back(makeCreatureAction(type, creatureId)); };
    const uint8_t localShield = selfStored->shield();
    const uint8_t targetShield = target.shield();
    using S = tibia::client::PlayerShields;

    if (target.isLocalPlayer()) {
        if (localShield != S::ShieldNone)
            push(A::LEAVE_PARTY);
        if (isPartyLeaderShield(localShield)) {
            auto shared = mutableClone(*makeAction(A::SET_SHARED_EXPERIENCE));
            shared->enable = !isSharedExpOn(localShield);
            actions.push_back(shared);
        }
        return;
    }

    if (!target.isPlayer())
        return;

    if (localShield == S::ShieldNone) {
        if (targetShield == S::ShieldWhiteYellow)
            push(A::JOIN_PARTY);
        else if (targetShield == S::ShieldNone)
            push(A::INVITE_TO_PARTY);
    } else if (isPartyLeaderShield(localShield)) {
        if (targetShield == S::ShieldNone)
            push(A::INVITE_TO_PARTY);
        else if (targetShield == S::ShieldWhiteBlue)
            push(A::REVOKE_PARTY_INVITATION);
        else if (targetShield >= S::ShieldBlue && targetShield <= S::ShieldYellowNoSharedExp)
            push(A::PASS_PARTY_LEADERSHIP);
    }
    if (localShield != S::ShieldNone && targetShield == S::ShieldGray)
        push(A::JOIN_AGGRESSION);
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
    const tibia::creatures::TCreature* selfStored = nullptr;
    if (tibia::game::isOnline() && storage) {
        const uint32_t selfId = tibia::game::localPlayerId();
        if (selfId)
            selfStored = storage->creatureAt(selfId).get();
    }
    const QString name = stored->name();

    const uint32_t attackingId = tibia::game::attackingCreatureId();
    const uint32_t followingId = tibia::game::getFollowingCreatureId();

    if (!own) {
        if (stored->isNpc()) {
            push(A::START_TALKING_TO_NPC);
            if (hireling) {
                push(A::REQUEST_OUTFITS_FROM_SERVER);
                push(A::REPORT_HIRELING_NAME);
                push(A::BUY_HIRELING_NAME_OR_SEX_CHANGE);
            }
        } else {
            push(attackingId == creatureId ? A::STOP_ATTACKING_CREATURE : A::ATTACK_CREATURE);
        }
        if (!hireling)
            push(followingId == creatureId ? A::STOP_FOLLOWING_CREATURE : A::FOLLOW_CREATURE);
    } else {
        push(A::REQUEST_OUTFITS_FROM_SERVER);
        push(stored->outfit().getMount() == 0 ? A::MOUNT_PLAYER : A::UNMOUNT_PLAYER);
        push(A::OPEN_PREY_DIALOG);
    }

    // qtclient social / party / inspect for players (and CipSoft leave/share on self).
    if (stored->isPlayer()) {
        if (!own) {
            actions.push_back(namedCreatureAction(A::OPEN_MESSAGE_TO_CHAT_CHANNEL, name));
            actions.push_back(namedCreatureAction(A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL, name));
            if (!isVipName(name))
                actions.push_back(namedCreatureAction(A::ADD_NEW_VIP, name));
            actions.push_back(namedCreatureAction(isIgnoredName(name) ? A::UNIGNORE_NAME : A::IGNORE_NAME, name));
            push(A::INSPECT_PLAYER);
            push(A::ASK_TO_INSPECT_PLAYER);
            push(A::INVITE_TO_INSPECT_PLAYER);
            push(A::ALLOW_INSPECT_PLAYER);
            push(A::REVOKE_INSPECT_PLAYER);
            actions.push_back(namedCreatureAction(A::REPORT_NAME, name));
            push(A::REPORT_BOT);
        }
        pushPartyActions(actions, *stored, creatureId, selfStored);
    }

    push(A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD);
    push(A::TARGET_SELECTED);
    push(A::LOOK_AT_CREATURE);
    actions.push_back(creatureSmartClick(creatureId));
    push(A::TEMPORARY_SELECT_CREATURE);
    return actions;
}

TGameActionPtr TGenericObjectGameActionProvider::creatureSmartClick(const uint32_t creatureId)
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

namespace {

TGameActionList objectActionsImpl(const uint16_t clientId, const appearances::TObjectAppearanceInstance* fieldObject,
                                  ThingType* flags, const Position& position, const int stackPosition,
                                  const int stackCountOverride = -1)
{
    TGameActionList actions;
    if (!clientId)
        return actions;
    const uint32_t id = clientId;
    const auto make = [&](const EGameAction type) -> TGameActionPtr {
        return makeObjectAction(type, position, stackPosition, clientId);
    };
    const auto push = [&](const EGameAction type) {
        TGameActionPtr action = make(type);
        actions.push_back(action);
        return action;
    };

    const auto has = [flags](bool (ThingType::*flag)() const) { return flags && (flags->*flag)(); };
    const bool isContainer = flags && flags->isContainer();
    const bool take = has(&ThingType::isPickupable);
    const bool unmove = has(&ThingType::isNotMoveable);
    const bool podium = has(&ThingType::isPodium);
    const uint8_t fieldTier = fieldObject ? fieldObject->tier() : 0;
    // Inventory/container slots own the authoritative count; appearance instances can
    // retain a stale shared count and open the split dialog for the wrong total.
    const uint16_t fieldCount = stackCountOverride >= 0
                                    ? static_cast<uint16_t>(stackCountOverride)
                                    : (fieldObject ? fieldObject->count() : 0);

    const TGameActionPtr look = push(A::LOOK);
    if (take && !has(&ThingType::isIgnoreLook))
        push(A::INSPECT_OBJECT);

    TGameActionPtr use;
    if (!isContainer) {
        use = push(has(&ThingType::isMultiUse) ? A::USE_WITH : A::USE);
    } else {
        use = push(A::OPEN);
        if (id != tibia::game::supplyStashId())
            use = push(A::OPEN_IN_NEW_WINDOW);
        push(A::OPEN_MANAGE_LOOT_CONTAINER);
    }
    if (id && id == tibia::game::standardRewardChestId())
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
        push(A::GET_TEXT_FOR_REPORT);
    if (take && !unmove)
        push(A::TRADE_ITEM);
    if (tibia::game::localPlayerIsSupplyStashAvailable()) {
        if (has(&ThingType::isMarketable) && (has(&ThingType::isStackable) || fieldTier == 0)) {
            push(A::STASH_STOW_TYPE);
            auto stow = mutableClone(*make(A::STASH_STOW_STACK));
            stow->amount = static_cast<int>(fieldCount);
            auto dialog = mutableClone(*make(A::STASH_OPEN_STOW_STACK_DIALOG));
            dialog->inner = stow;
            actions.push_back(dialog);
        }
        if (isContainer && id != tibia::game::supplyStashId())
            push(A::STASH_STOW_CONTAINER_CONTENT);
    }
    if (tibia::game::localPlayerIsMarketAvailable() && has(&ThingType::isMarketable))
        push(A::MARKET_BROWSE);
    if (has(&ThingType::hasCyclopediaItem))
        push(A::OPEN_CYCLOPEDIA);
    if (flags && flags->getProficiencyId() != 0)
        push(A::OPEN_WEAPON_PROFICIENCY_DIALOG);
    // qtclient loot black/whitelist: menus filter these; handler is ManageLoot.
    if (take && id != 0) {
        if (cyclopedia::TItemInfoStorage* loot = cyclopedia::TItemInfoStorage::instance()) {
            if (loot->isLootListed(static_cast<uint16_t>(id)))
                push(A::LOOT_REMOVE_FROM_BLACKWHITELIST);
            else if (!loot->isLootListFull())
                push(A::LOOT_ADD_TO_BLACKWHITELIST);
        }
    }
    {
        // Map stacks carry count on the appearance; inventory/container paths
        // resolve count at drop via storageSlotCount. Without amount here,
        // finalizeDrop / Shift-move-all / Ctrl-split treat every map stack as 1.
        if (fieldCount > 1 && has(&ThingType::isStackable)) {
            auto move = mutableClone(*make(A::MOVE_OBJECT));
            move->amount = static_cast<int>(fieldCount);
            actions.push_back(move);
        } else {
            push(A::MOVE_OBJECT);
        }
    }

    actions.push_back(make(A::TARGET_SELECTED));

    int lensHelp = flags ? flags->getLensHelp() : 0;
    if (isContainer && lensHelp == 0)
        lensHelp = 1002;
    if (lensHelp != 0) {
        auto help = mutableClone(*make(A::LENSHELP_SHOW));
        help->lensHelpId = lensHelp;
        actions.push_back(help);
    }

    if (isContainer && has(&ThingType::isCorpse) && !has(&ThingType::isPlayerCorpse))
        push(A::QUICK_LOOT);

    TGameActionPtr inner;
    if (!has(&ThingType::hasAction)) {
        inner = has(&ThingType::isUsable) ? use : look;
    } else {
        const auto defaultAction = flags->getDefaultAction();
        if (defaultAction == PLAYER_ACTION_AUTOWALK_HIGHLIGHT)
            inner = makeMovementIntent(position);
        else if (defaultAction >= PLAYER_ACTION_USE)
            inner = use;
        else
            inner = look;
    }
    actions.push_back(makeSmartClick(inner));
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
        auto move = std::make_shared<TGameActionMoveObject>(
            position, object.stackPosition, static_cast<uint16_t>(Proto::Creature));
        move->creatureId = object.creatureId;
        actions.push_back(move);
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
