// Host builders of CIP client messages that callers outside CipSoft's handlers
// still send (game window controllers, the pack ABI): each fills a
// GameclientMessage and hands it to the session's protocol queue, whose client
// message processor (TProtobufClientMessageTranslator) makes its packet.

#include "protocol/clientmessages.h"

#include "protocol/gameactioncontexthelper.h"
#include "protocol/protocolciptypes.h"
#include <protocol.pb.h>

#include <QtGlobal>
#include <format>
#include <client/const.h>
#include <client/declarations.h>
#include <client/outfit.h>
#include <client/position.h>
#include <client/staticdata.h>
#include "container/containerciptypes.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/mapsession.h"
#include "game/objectreference.h"
#include <tibia/assets/things.h>
#include <tibia/appearances/appearancetype.h>

#include <algorithm>
#include <array>

using namespace ::tibia::protobuf::protocol;

namespace {

void setCoordinate(ProtocolCoordinate* out, const Position& position)
{
    out->set_x(position.x);
    out->set_y(position.y);
    out->set_z(position.z);
}

}

namespace tibia::protocol::gameclient {

/// A classic object (position, stack position, type) as the ObjectPosition and
/// ObjectIdentifier of a client message, written by CipSoft's
/// TGameActionContextHelper from the location the classic position names. The
/// classic stack position of an object "anywhere" is its liquid or tier.
void setClassicObject(ObjectPosition* objectPosition, ObjectIdentifier* objectIdentifier, const Position& position,
                      const int stackPosition, const uint16_t typeId)
{
    if (const tibia::input::TGameActionContextPtr location = tibia::input::locationContextFromPosition(position, stackPosition)) {
        tibia::protocol::TGameActionContextHelper().fillObjectPosition(location, objectPosition);
        if (objectIdentifier && stackPosition > 0
            && location->gameActionContextType() == tibia::input::EGameActionContextType::LOCATION_UNKNOWN)
            objectIdentifier->set_data(static_cast<uint32_t>(stackPosition));
    }
    if (objectIdentifier)
        objectIdentifier->set_type_id(typeId);
}

void sendLogout()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(20));
}

void sendTurnNorth()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(111));
}

void sendTurnEast()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(112));
}

void sendTurnSouth()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(113));
}

void sendTurnWest()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(114));
}

void sendUseItem(const Position& position, const uint16_t itemId, const uint8_t stackpos, const uint8_t index)
{
    auto message = tibia::protocol::newGameclientMessage(130);
    auto* m = message->MutableExtension(GameclientMessageUseObject::gameclient_message_use_object);
    setClassicObject(m->mutable_object_position(), m->mutable_object_identifier(), position, stackpos, itemId);
    m->set_container_index(index);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendUseItemWith(const Position& fromPos, const uint16_t itemId, const uint8_t fromStackPos, const Position& toPos, const uint16_t toThingId, const uint8_t toStackPos)
{
    auto message = tibia::protocol::newGameclientMessage(131);
    auto* m = message->MutableExtension(GameclientMessageUseTwoObjects::gameclient_message_use_two_objects);
    setClassicObject(m->mutable_source_object_position(), m->mutable_source_object_identifier(), fromPos, fromStackPos,
                     itemId);
    setClassicObject(m->mutable_destination_object_position(), m->mutable_destination_object_identifier(), toPos,
                     toStackPos, toThingId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendUseOnCreature(const Position& pos, const uint16_t thingId, const uint8_t stackpos, const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(132);
    auto* m = message->MutableExtension(GameclientMessageUseOnCreature::gameclient_message_use_on_creature);
    setClassicObject(m->mutable_object_position(), m->mutable_object_identifier(), pos, stackpos, thingId);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendTalk(const uint32_t mode, const uint16_t channelId, const std::string_view receiver, const std::string_view message)
{
    // Consume any staged crosshair aim up front so an aborted/empty say never leaks it to the next one.
    tibia::game::TGameSessionBase* session = tibia::game::currentGameSession();
    const Position talkAimPos = session ? session->takeNextTalkAim()
                                        : Position(static_cast<int32_t>(UINT16_MAX), static_cast<int32_t>(UINT16_MAX), UINT8_MAX);
    const uint8_t talkAimMode = session ? session->takeNextTalkAimMode() : 0;
    if (message.empty())
        return;
    if (message.length() > UINT8_MAX) {
        return;
    }
    auto talk = tibia::protocol::newGameclientMessage(150);
    auto* m = talk->MutableExtension(GameclientMessageTalk::gameclient_message_talk);
    m->set_mode(mode);
    // CIP's chat message modes (the protocol's): private to (5) and gamemaster
    // private to (16) name a receiver; channel management (6), channel (7),
    // channel highlight (8) and gamemaster channel (14) a channel.
    switch (mode) {
        case 5:
        case 16:
            m->set_receiver(std::string(receiver));
            break;
        case 6:
        case 7:
        case 8:
        case 14:
            m->set_channel_id(channelId);
            break;
        default:
            break;
    }
    m->set_text(std::string(message));
    // Spell aim: 3 at target or self, 1 crosshair / 2 mouse with the tile.
    if (talkAimMode == 3) {
        m->set_aim_mode(3);
    } else if (talkAimPos.isValid()) {
        m->set_aim_mode(talkAimMode == 2 ? 2 : 1);
        setClassicObject(m->mutable_aim_position(), nullptr, talkAimPos, -1, 0);
    }
    tibia::protocol::sendGameclientMessage(std::move(talk));
}

void sendOpenPrivateChannel(const std::string_view receiver)
{
    auto message = tibia::protocol::newGameclientMessage(154);
    auto* m = message->MutableExtension(GameclientMessagePrivateChannel::gameclient_message_private_channel);
    m->set_receiver(std::string(receiver));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendJoinParty(const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(164);
    auto* m = message->MutableExtension(GameclientMessageJoinParty::gameclient_message_join_party);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendDisbandParty()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(169));
}

void sendTeamFinderLeaderAction(const uint8_t action, const uint32_t memberId, const uint8_t memberStatus, const TeamFinderAssemble& assemble)
{
    auto message = tibia::protocol::newGameclientMessage(44);
    auto* m = message->MutableExtension(GameclientMessageTeamFinderAssembleTeam::gameclient_message_team_finder_assemble_team);
    m->set_action(action);
    if (action == 2) {
        m->set_member_id(memberId);
        m->set_member_status(memberStatus);
    } else if (action == 3) {
        auto* a = m->mutable_assembly();
        a->set_min_level(assemble.minLevel);
        a->set_max_level(assemble.maxLevel);
        a->set_vocations(assemble.vocationIDs);
        a->set_team_slots(assemble.teamSlots);
        a->set_free_slots(assemble.freeSlots);
        a->set_party_only(assemble.partyBool);
        a->set_timestamp(assemble.timestamp);
        a->set_team_type(assemble.teamType);
        // boss / hunt type + area / quest
        if (assemble.teamType == 1)
            a->set_first_id(assemble.bossID);
        else if (assemble.teamType == 2) {
            a->set_first_id(assemble.huntType);
            a->set_second_id(assemble.huntArea);
        } else if (assemble.teamType == 3)
            a->set_first_id(assemble.questID);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendTeamFinderMemberAction(const uint8_t action, const uint32_t leaderId)
{
    auto message = tibia::protocol::newGameclientMessage(45);
    auto* m = message->MutableExtension(GameclientMessageTeamFinderJoinTeam::gameclient_message_team_finder_join_team);
    m->set_action(action);
    m->set_leader_id(leaderId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendInviteToOwnChannel(const std::string_view name)
{
    auto message = tibia::protocol::newGameclientMessage(171);
    auto* m = message->MutableExtension(GameclientMessageInviteToChannel::gameclient_message_invite_to_channel);
    m->set_name(std::string(name));
    m->set_channel_id(0);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendExcludeFromOwnChannel(const std::string_view name)
{
    auto message = tibia::protocol::newGameclientMessage(172);
    auto* m = message->MutableExtension(GameclientMessageExcludeFromChannel::gameclient_message_exclude_from_channel);
    m->set_name(std::string(name));
    m->set_channel_id(0);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendSoulSealsAction(const uint16_t raceId)
{
    if (std::cmp_equal(raceId, 0)) {
        return;
    }

    auto message = tibia::protocol::newGameclientMessage(186);
    auto* m = message->MutableExtension(GameclientMessageSoulSealsFightMonster::gameclient_message_soul_seals_fight_monster);
    m->set_race_id(raceId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestBless()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(207));
}

void sendBugReport(const uint8_t category, const std::string_view comment, const std::string_view typoSpeaker,
                                 const std::string_view typoText, const Position& position)
{
    auto message = tibia::protocol::newGameclientMessage(230);
    auto* m = message->MutableExtension(GameclientMessageBugReport::gameclient_message_bug_report);
    m->set_category(category);
    m->set_comment(std::string(comment));
    if (category == 1) {
        m->set_typo_speaker(std::string(typoSpeaker));
        m->set_typo_text(std::string(typoText));
    } else if (category == 0) {
        setCoordinate(m->mutable_position(), position);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestQuestLog()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(240));
}

void sendRequestQuestLine(const uint16_t questId)
{
    auto message = tibia::protocol::newGameclientMessage(241);
    auto* m = message->MutableExtension(GameclientMessageGetQuestLine::gameclient_message_get_quest_line);
    m->set_quest_id(questId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendInspectionObject(const tibia::client::InspectObjectTypes inspectionType, const uint16_t itemId, const uint8_t itemCount)
{
    if (inspectionType != tibia::client::INSPECT_NPCTRADE &&
        inspectionType != tibia::client::INSPECT_CYCLOPEDIA &&
        inspectionType != tibia::client::INSPECT_PROFICIENCY) {
        return;
    }
    auto message = tibia::protocol::newGameclientMessage(205);
    auto* m = message->MutableExtension(GameclientMessageInspectObject::gameclient_message_inspect_object);
    m->set_inspection_type(inspectionType);
    m->mutable_object_identifier()->set_type_id(itemId);
    m->mutable_object_identifier()->set_data(itemCount);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendFriendSystemAction(const uint8_t action, const uint8_t value)
{
    auto message = tibia::protocol::newGameclientMessage(129);
    auto* m = message->MutableExtension(GameclientMessageFriendSystemAction::gameclient_message_friend_system_action);
    m->set_action(action);
    m->set_value(value);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendFriendSystemAccountAction(const uint8_t action, const uint32_t accountId, const uint8_t value)
{
    auto message = tibia::protocol::newGameclientMessage(129);
    auto* m = message->MutableExtension(GameclientMessageFriendSystemAction::gameclient_message_friend_system_action);
    m->set_action(action);
    m->set_account_id(accountId);
    m->set_value(value);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendFriendSystemSearch(const std::string& characterName)
{
    auto message = tibia::protocol::newGameclientMessage(129);
    auto* m = message->MutableExtension(GameclientMessageFriendSystemAction::gameclient_message_friend_system_action);
    m->set_action(11);
    m->set_text(characterName);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendFriendSystemBadgeVisibility(const uint32_t badgeId, const bool visible)
{
    auto message = tibia::protocol::newGameclientMessage(129);
    auto* m = message->MutableExtension(GameclientMessageFriendSystemAction::gameclient_message_friend_system_action);
    m->set_action(13);
    m->set_badge_id(badgeId);
    m->set_visible(visible);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendFriendSystemSetting(const uint8_t settingType, const bool showCharacterInfo, const bool showAccountInfo, const bool allowInspect)
{
    auto message = tibia::protocol::newGameclientMessage(129);
    auto* m = message->MutableExtension(GameclientMessageFriendSystemAction::gameclient_message_friend_system_action);
    m->set_action(16);
    auto* setting = m->mutable_setting();
    setting->set_setting_type(settingType);
    setting->set_show_character_info(showCharacterInfo);
    setting->set_show_account_info(showAccountInfo);
    setting->set_allow_inspect(allowInspect);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCharacterTradeConfigurationAction(
    const uint8_t action,
    const uint32_t price,
    const uint32_t endTimestamp,
    const std::vector<std::tuple<uint16_t, uint8_t>>& items,
    const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems,
    const std::vector<uint16_t>& highlights)
{
    auto message = tibia::protocol::newGameclientMessage(118);
    auto* m = message->MutableExtension(GameclientMessageCharacterTradeConfigurationAction::gameclient_message_character_trade_configuration_action);
    m->set_action(action);
    if (action == 2 || action == 3) {
        m->set_price(price);
        m->set_end_time(endTimestamp);
        const auto fill = [](auto* list, const std::vector<std::tuple<uint16_t, uint8_t>>& source) {
            for (const auto& [itemId, tier] : source) {
                auto* object = list->Add();
                object->set_object_id(itemId);
                if (tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, itemId) && tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, itemId)->getClassification() > 0)
                    object->set_tier(tier);
            }
        };
        fill(m->mutable_objects(), items);
        fill(m->mutable_store_objects(), storeItems);
        for (const uint16_t id : highlights)
            m->add_highlights(id);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendClientCheck(const std::vector<uint8_t>& data)
{
    auto message = tibia::protocol::newGameclientMessage(99);
    message->MutableExtension(GameclientMessageClientCheck::gameclient_message_client_check)->set_data(std::string(data.begin(), data.end()));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestBosstiaryInfo()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(174));
}

void sendRequestBossSlootInfo()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(175));
}

void sendRequestBossSlotAction(const uint8_t action, const uint32_t raceId)
{
    auto message = tibia::protocol::newGameclientMessage(176);
    auto* m = message->MutableExtension(GameclientMessageEditBossSlot::gameclient_message_edit_boss_slot);
    m->set_action(action);
    m->set_race_id(raceId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendStatusTrackerBestiary(const uint16_t raceId, const bool status)
{
    auto message = tibia::protocol::newGameclientMessage(42);
    auto* m = message->MutableExtension(GameclientMessageTrackBestiaryRace::gameclient_message_track_bestiary_race);
    m->set_race_id(raceId);
    m->set_tracked(status);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendApplyImbuement(const uint8_t slot, const uint32_t imbuementId, const bool protectionCharm)
{
    auto message = tibia::protocol::newGameclientMessage(213);
    auto* m = message->MutableExtension(GameclientMessageApplyImbuement::gameclient_message_apply_imbuement);
    m->set_slot(slot);
    m->set_imbuement_id(imbuementId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendClearImbuement(const uint8_t slot)
{
    auto message = tibia::protocol::newGameclientMessage(214);
    auto* m = message->MutableExtension(GameclientMessageApplyClearingCharm::gameclient_message_apply_clearing_charm);
    m->set_slot(slot);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCloseImbuingWindow()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(215));
}

void sendImbuementWindowAction(const uint8_t type, const uint16_t itemId, const Position& pos, const uint8_t stackpos)
{
    auto message = tibia::protocol::newGameclientMessage(178);
    auto* m = message->MutableExtension(GameclientMessagePickObjectToImbue::gameclient_message_pick_object_to_imbue);
    m->set_action(type);
    if (type == tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM)
        setClassicObject(m->mutable_object_position(), m->mutable_object_identifier(), pos, stackpos, itemId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendImbuementDurations(const bool isOpen)
{
    auto message = tibia::protocol::newGameclientMessage(96);
    auto* m = message->MutableExtension(GameclientMessageTrackImbuements::gameclient_message_track_imbuements);
    m->set_open(isOpen);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t size, const std::vector<uint16_t>& listedItems)
{
    (void)size;
    auto message = tibia::protocol::newGameclientMessage(145);
    auto* m = message->MutableExtension(GameclientMessageQuickLootBlackWhitelist::gameclient_message_quick_loot_black_whitelist);
    m->set_filter(filter);
    for (const uint16_t item : listedItems)
        m->add_object_ids(item);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void openContainerQuickLoot(const uint8_t action, const uint8_t category, const Position& pos, const uint16_t itemId, const uint8_t stackpos, const bool useMainAsFallback)
{
    auto message = tibia::protocol::newGameclientMessage(144);
    auto* m = message->MutableExtension(GameclientMessageManagedContainer::gameclient_message_managed_container);
    m->set_action(action);
    m->set_category(category);
    if (action == 0 || action == 4)
        setClassicObject(m->mutable_object_position(), m->mutable_object_identifier(), pos, stackpos, itemId);
    m->set_use_main_as_fallback(useMainAsFallback);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendWeaponProficiencyAction(const uint8_t actionType, const uint16_t itemId)
{
    auto message = tibia::protocol::newGameclientMessage(179);
    auto* m = message->MutableExtension(GameclientMessageWeaponProficiencyCommand::gameclient_message_weapon_proficiency_command);
    m->set_action(actionType);
    m->set_object_id(itemId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendWeaponProficiencyApply(const uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions)
{
    auto message = tibia::protocol::newGameclientMessage(179);
    auto* m = message->MutableExtension(GameclientMessageWeaponProficiencyCommand::gameclient_message_weapon_proficiency_command);
    m->set_action(tibia::client::WEAPON_PROFICIENCY_APPLY_PERKS);
    m->set_object_id(itemId);
    const size_t count = std::min(levels.size(), perkPositions.size());
    for (size_t i = 0; i < count; ++i) {
        auto* perk = m->add_perks();
        perk->set_level(levels[i]);
        perk->set_perk_position(perkPositions[i]);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendWeaponProficiencySlotAction(const uint8_t actionType, const uint16_t itemId, const uint8_t level, const uint8_t perkPosition, const int offerIndex)
{
    auto message = tibia::protocol::newGameclientMessage(179);
    auto* m = message->MutableExtension(GameclientMessageWeaponProficiencyCommand::gameclient_message_weapon_proficiency_command);
    m->set_action(actionType);
    m->set_object_id(itemId);
    auto* slot = m->mutable_slot();
    slot->set_level(level);
    slot->set_perk_position(perkPosition);
    m->set_offer_index(static_cast<uint8_t>(offerIndex));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

}
