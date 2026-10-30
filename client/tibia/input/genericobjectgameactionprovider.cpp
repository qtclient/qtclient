#include "genericobjectgameactionprovider.h"

#include "app/clientoptions.h"
#include "cyclopedia/iteminfostorage.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/map.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QJsonArray>
#include <QJsonObject>

namespace tibia::input {

namespace {

using A = EGameAction;

/// CIP's party shields: member or leader, with or without shared experience.
bool isPartyMember(const CreaturePtr& creature)
{
    const uint8_t shield = creature->getShield();
    return shield >= 3 && shield <= 10;
}

bool isPartyLeaderShield(const uint8_t shield)
{
    using S = Otc::PlayerShields;
    return shield == S::ShieldWhiteYellow || shield == S::ShieldYellow || shield == S::ShieldYellowSharedExp
        || shield == S::ShieldYellowNoSharedExpBlink || shield == S::ShieldYellowNoSharedExp;
}

bool isSharedExpOn(const uint8_t shield)
{
    using S = Otc::PlayerShields;
    return shield == S::ShieldYellowSharedExp || shield == S::ShieldBlueSharedExp;
}

bool isVipName(const QString& name)
{
    for (const auto& [id, vip] : g_game.getVips()) {
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

/// Soft party offers from local + target shields.
void pushPartyActions(TGameActionList& actions, const CreaturePtr& creature, const uint32_t creatureId,
                      const LocalPlayerPtr& self)
{
    if (!self)
        return;
    const auto push = [&](const EGameAction type) { actions.push_back(makeCreatureAction(type, creatureId)); };
    const uint8_t localShield = self->getShield();
    const uint8_t targetShield = creature->getShield();
    using S = Otc::PlayerShields;

    if (creature->isLocalPlayer()) {
        if (localShield != S::ShieldNone)
            push(A::LEAVE_PARTY);
        if (isPartyLeaderShield(localShield)) {
            auto shared = mutableClone(*makeAction(A::SET_SHARED_EXPERIENCE));
            shared->enable = !isSharedExpOn(localShield);
            actions.push_back(shared);
        }
        return;
    }

    if (!creature->isPlayer())
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

/// The object's appearance flags; creatures stand for themselves on a field
/// and have none.
ThingType* flagsOf(const ThingPtr& object)
{
    return object->isItem() ? object->getThingType() : nullptr;
}

}

bool isHireling(const CreaturePtr& creature)
{
    return creature && creature->isNpc() && creature->getInspectionType() == 7;
}

TGameActionList TGenericObjectGameActionProvider::creatureActions(const uint32_t creatureId)
{
    const CreaturePtr creature = creatureId ? g_map.getCreatureById(creatureId) : nullptr;
    if (!creature)
        return { makeCreatureAction(A::TEMPORARY_SELECT_CREATURE, 0) };

    TGameActionList actions;
    const auto push = [&](const EGameAction type) { actions.push_back(makeCreatureAction(type, creatureId)); };
    const bool own = creature->isLocalPlayer();
    const bool hireling = isHireling(creature);
    const LocalPlayerPtr& self = g_game.getLocalPlayer();
    const QString name = QString::fromStdString(creature->getName());

    if (!own) {
        if (creature->isNpc()) {
            push(A::START_TALKING_TO_NPC);
            if (hireling) {
                push(A::REQUEST_OUTFITS_FROM_SERVER);
                push(A::REPORT_HIRELING_NAME);
                push(A::BUY_HIRELING_NAME_OR_SEX_CHANGE);
            }
        } else {
            push(g_game.getAttackingCreature() == creature ? A::STOP_ATTACKING_CREATURE : A::ATTACK_CREATURE);
        }
        if (!hireling)
            push(g_game.getFollowingCreature() == creature ? A::STOP_FOLLOWING_CREATURE : A::FOLLOW_CREATURE);
    } else {
        push(A::REQUEST_OUTFITS_FROM_SERVER);
        push(creature->getOutfit().getMount() == 0 ? A::MOUNT_PLAYER : A::UNMOUNT_PLAYER);
        push(A::OPEN_PREY_DIALOG);
    }

    // Soft social / party / inspect for players (and CipSoft leave/share on self).
    if (creature->isPlayer()) {
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
        pushPartyActions(actions, creature, creatureId, self);
    }

    push(A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD);
    push(A::TARGET_SELECTED);
    push(A::LOOK_AT_CREATURE);
    actions.push_back(creatureSmartClick(creature));
    push(A::TEMPORARY_SELECT_CREATURE);
    return actions;
}

TGameActionPtr TGenericObjectGameActionProvider::creatureSmartClick(const CreaturePtr& creature)
{
    const uint32_t id = creature->getId();
    EGameAction inner = A::LOOK_AT_CREATURE;
    if (creature->isNpc())
        inner = A::START_TALKING_TO_NPC;
    else if (!creature->isLocalPlayer() && !isPartyMember(creature))
        inner = g_game.getAttackingCreature() == creature ? A::STOP_ATTACKING_CREATURE : A::ATTACK_CREATURE;
    return makeSmartClick(makeCreatureAction(inner, id));
}

TGameActionList TGenericObjectGameActionProvider::objectActions(const ThingPtr& object, const Position& position, const int stackPosition)
{
    TGameActionList actions;
    if (!object)
        return actions;
    const auto make = [&](const EGameAction type) { return makeObjectAction(type, object, position, stackPosition); };
    const auto push = [&](const EGameAction type) {
        TGameActionPtr action = make(type);
        actions.push_back(action);
        return action;
    };

    ThingType* flags = flagsOf(object);
    const auto has = [flags](bool (ThingType::*flag)() const) { return flags && (flags->*flag)(); };
    const uint32_t id = object->isItem() ? object->getId() : 0;
    const bool isContainer = flags && flags->isContainer();
    const bool take = has(&ThingType::isPickupable);
    const bool unmove = has(&ThingType::isNotMoveable);
    const bool podium = has(&ThingType::isPodium);

    const TGameActionPtr look = push(A::LOOK);
    if (take && !has(&ThingType::isIgnoreLook))
        push(A::INSPECT_OBJECT);

    TGameActionPtr use;
    if (!isContainer) {
        use = push(has(&ThingType::isMultiUse) ? A::USE_WITH : A::USE);
    } else {
        use = push(A::OPEN);
        if (id != g_things.getSupplyStashId())
            use = push(A::OPEN_IN_NEW_WINDOW);
        push(A::OPEN_MANGE_LOOT_CONTAINER);
    }
    if (id && id == g_things.getStandardRewardChestId())
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
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (player && player->isSupplyStashAvailable() && object->isItem()) {
        const ItemPtr item = object->static_self_cast<Item>();
        if (has(&ThingType::isMarketable) && (has(&ThingType::isStackable) || item->getTier() == 0)) {
            push(A::STASH_STOW_TYPE);
            auto stow = mutableClone(*make(A::STASH_STOW_STACK));
            stow->amount = item->getCount();
            auto dialog = mutableClone(*make(A::STASH_OPEN_STOW_STACK_DIALOG));
            dialog->inner = stow;
            actions.push_back(dialog);
        }
        if (isContainer && id != g_things.getSupplyStashId())
            push(A::STASH_STOW_CONTAINER_CONTENT);
    }
    if (player && player->isMarketAvailable() && has(&ThingType::isMarketable))
        push(A::MARKET_BROWSE);
    if (has(&ThingType::hasCyclopediaItem))
        push(A::OPEN_CYCLOPEDIA);
    if (flags && flags->getProficiencyId() != 0)
        push(A::OPEN_WEAPON_PROFICIENCY_DIALOG);
    // Soft loot black/whitelist: menus filter these; handler is ManageLoot.
    if (take && id != 0) {
        if (cyclopedia::TItemInfoStorage* loot = cyclopedia::TItemInfoStorage::instance()) {
            if (loot->isLootListed(static_cast<uint16_t>(id)))
                push(A::LOOT_REMOVE_FROM_BLACKWHITELIST);
            else if (!loot->isLootListFull())
                push(A::LOOT_ADD_TO_BLACKWHITELIST);
        }
    }
    push(A::MOVE_OBJECT);

    auto target = mutableClone(*make(A::TARGET_SELECTED));
    if (object->isCreature() && !object->isLocalPlayer())
        target->creatureId = object->getId();
    actions.push_back(target);

    int lensHelp = flags ? flags->getLensHelp() : 0;
    if (object->isCreature())
        lensHelp = 1000 | (object->isLocalPlayer() ? 1 : 0);
    else if (isContainer && lensHelp == 0)
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
        if (object->isCreature()) {
            actions.push_back(creatureSmartClick(object->static_self_cast<Creature>()));
            return actions;
        }
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

}
