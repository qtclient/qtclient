// CIP client messages: each builder fills a GameclientMessage and hands it to
// the protocol queue, which encodes it (TProtobufClientMessageTranslator) and
// writes it to the game connection.

#include "protocol/clientmessages.h"

#include "network/gameserverpacketprocessor.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolclassicwire.h"
#include "protocol/protocolcodes.h"
#include <protocol.pb.h>

#include <QtGlobal>
#include <format>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>
#include "tibia/assets/thingtype.h"

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

void sendExtendedOpcode(const uint8_t opcode, const std::string& buffer)
{
    auto* processor = network::TGameserverNetworkPacketProcessor::live();
    if (!processor || !processor->productTransportEnabled()) {
        qCritical("%s", std::format("Unable to send extended opcode {}, extended opcodes are not enabled", opcode).c_str());
        return;
    }
    ClassicWriter w;
    w.u8(Proto::ClientExtendedOpcode);
    w.u8(opcode);
    w.string(buffer);
    processor->writeProductFrame(w.out);
}

void sendCharacterSwitch(const std::string& characterName)
{
    auto* processor = network::TGameserverNetworkPacketProcessor::live();
    if (!processor)
        return;
    ClassicWriter w;
    w.u8(Proto::ClientCharacterSwitch);
    w.string(characterName);
    processor->writeProductFrame(w.out);
}

void sendSecondaryLogin(const std::string_view token)
{
    auto message = tibia::protocol::newGameclientMessage(Proto::ClientSecondaryLogin);
    auto* m = message->MutableExtension(GameclientMessageSecondaryLogin::gameclient_message_secondary_login);
    m->set_token(std::string(token));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEnterGame()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientEnterGame));
}

void sendLogout()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientLeaveGame));
}

void sendPing()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientPing));
}

void sendPingBack()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientPingBack));
}

void sendConnectionPingBack()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientConnectionPingBack));
}

void sendAutoWalk(const std::vector<tibia::client::Direction>& path)
{
    auto message = tibia::protocol::newGameclientMessage(100);
    auto* m = message->MutableExtension(GameclientMessageGoPath::gameclient_message_go_path);
    for (const tibia::client::Direction dir : path) {
        uint8_t step = 0;
        switch (dir) {
            case tibia::client::East: step = 1; break;
            case tibia::client::NorthEast: step = 2; break;
            case tibia::client::North: step = 3; break;
            case tibia::client::NorthWest: step = 4; break;
            case tibia::client::West: step = 5; break;
            case tibia::client::SouthWest: step = 6; break;
            case tibia::client::South: step = 7; break;
            case tibia::client::SouthEast: step = 8; break;
            default: break;
        }
        m->add_directions(step);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendWalkNorth()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkNorth));
}

void sendWalkEast()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkEast));
}

void sendWalkSouth()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkSouth));
}

void sendWalkWest()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkWest));
}

void sendStop()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientStop));
}

void sendWalkNorthEast()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkNorthEast));
}

void sendWalkSouthEast()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkSouthEast));
}

void sendWalkSouthWest()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkSouthWest));
}

void sendWalkNorthWest()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientWalkNorthWest));
}

void sendTurnNorth()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientTurnNorth));
}

void sendTurnEast()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientTurnEast));
}

void sendTurnSouth()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientTurnSouth));
}

void sendTurnWest()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientTurnWest));
}

void sendTeleport(const Position& pos)
{
    auto message = tibia::protocol::newGameclientMessage(115);
    auto* m = message->MutableExtension(GameclientMessageTeleport::gameclient_message_teleport);
    setCoordinate(m->mutable_position(), pos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendStartOfflineTraining(const uint8_t skillType)
{
    if (std::cmp_greater(skillType, static_cast<uint8_t>(tibia::client::Fishing))) {
        return;
    }

    auto message = tibia::protocol::newGameclientMessage(116);
    auto* m = message->MutableExtension(GameclientMessageStartOfflineTraining::gameclient_message_start_offline_training);
    m->set_skill(skillType);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendTutorialChangeVocation(uint8_t vocationClientId)
{
    auto message = tibia::protocol::newGameclientMessage(110);
    auto* m = message->MutableExtension(GameclientMessageSetVocation::gameclient_message_set_vocation);
    m->set_vocation(vocationClientId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEquipItemWithTier(const uint16_t itemId, const uint8_t tier)
{
    auto message = tibia::protocol::newGameclientMessage(119);
    auto* m = message->MutableExtension(GameclientMessageEquipObject::gameclient_message_equip_object);
    m->set_object_id(itemId);
    m->set_tier(tier);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEquipItemWithCountOrSubType(const uint16_t itemId, const uint16_t countOrSubType)
{
    auto message = tibia::protocol::newGameclientMessage(119);
    auto* m = message->MutableExtension(GameclientMessageEquipObject::gameclient_message_equip_object);
    m->set_object_id(itemId);
    m->set_tier(static_cast<uint8_t>(countOrSubType));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendMove(const Position& fromPos, const uint16_t thingId, const uint8_t stackpos, const Position& toPos, const uint16_t count)
{
    auto message = tibia::protocol::newGameclientMessage(120);
    auto* m = message->MutableExtension(GameclientMessageMoveObject::gameclient_message_move_object);
    setCoordinate(m->mutable_from_position(), fromPos);
    m->set_object_id(thingId);
    m->set_from_stack_position(stackpos);
    setCoordinate(m->mutable_to_position(), toPos);
    m->set_count(static_cast<uint8_t>(count));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendInspectNpcTrade(const uint16_t itemId, const uint16_t count)
{
    auto message = tibia::protocol::newGameclientMessage(121);
    auto* m = message->MutableExtension(GameclientMessageLookNPCTrade::gameclient_message_look_n_p_c_trade);
    m->set_object_id(itemId);
    m->set_count(static_cast<uint8_t>(count));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendBuyItem(const uint16_t itemId, const uint8_t subType, const uint16_t amount, const bool ignoreCapacity, const bool buyWithBackpack)
{
    auto message = tibia::protocol::newGameclientMessage(122);
    auto* m = message->MutableExtension(GameclientMessageBuyObject::gameclient_message_buy_object);
    m->set_object_id(itemId);
    m->set_subtype(subType);
    m->set_amount(amount);
    m->set_ignore_capacity(ignoreCapacity);
    m->set_in_backpacks(buyWithBackpack);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendSellItem(const uint16_t itemId, const uint8_t subType, const uint16_t amount, const bool ignoreEquipped)
{
    auto message = tibia::protocol::newGameclientMessage(123);
    auto* m = message->MutableExtension(GameclientMessageSellObject::gameclient_message_sell_object);
    m->set_object_id(itemId);
    m->set_subtype(subType);
    m->set_amount(amount);
    m->set_ignore_equipped(ignoreEquipped);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCloseNpcTrade()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientCloseNpcTrade));
}

void sendRequestTrade(const Position& pos, const uint16_t thingId, const uint8_t stackpos, const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(125);
    auto* m = message->MutableExtension(GameclientMessageTradeObject::gameclient_message_trade_object);
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(thingId);
    m->set_stack_position(stackpos);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendInspectTrade(const bool counterOffer, const uint8_t index)
{
    auto message = tibia::protocol::newGameclientMessage(126);
    auto* m = message->MutableExtension(GameclientMessageLookTrade::gameclient_message_look_trade);
    m->set_counter_offer(counterOffer ? 1u : 0u);
    m->set_index(index);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendAcceptTrade()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientAcceptTrade));
}

void sendRejectTrade()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientRejectTrade));
}

void sendUseItem(const Position& position, const uint16_t itemId, const uint8_t stackpos, const uint8_t index)
{
    auto message = tibia::protocol::newGameclientMessage(130);
    auto* m = message->MutableExtension(GameclientMessageUseObject::gameclient_message_use_object);
    setCoordinate(m->mutable_position(), position);
    m->set_object_id(itemId);
    m->set_stack_position(stackpos);
    m->set_container_index(index);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendUseItemWith(const Position& fromPos, const uint16_t itemId, const uint8_t fromStackPos, const Position& toPos, const uint16_t toThingId, const uint8_t toStackPos)
{
    auto message = tibia::protocol::newGameclientMessage(131);
    auto* m = message->MutableExtension(GameclientMessageUseTwoObjects::gameclient_message_use_two_objects);
    setCoordinate(m->mutable_from_position(), fromPos);
    m->set_object_id(itemId);
    m->set_from_stack_position(fromStackPos);
    setCoordinate(m->mutable_to_position(), toPos);
    m->set_to_object_id(toThingId);
    m->set_to_stack_position(toStackPos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendUseOnCreature(const Position& pos, const uint16_t thingId, const uint8_t stackpos, const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(132);
    auto* m = message->MutableExtension(GameclientMessageUseOnCreature::gameclient_message_use_on_creature);
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(thingId);
    m->set_stack_position(stackpos);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRotateItem(const Position& pos, const uint16_t thingId, const uint8_t stackpos)
{
    auto message = tibia::protocol::newGameclientMessage(133);
    auto* m = message->MutableExtension(GameclientMessageTurnObject::gameclient_message_turn_object);
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(thingId);
    m->set_stack_position(stackpos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOnWrapItem(const Position& pos, const uint16_t thingId, const uint8_t stackpos)
{
    auto message = tibia::protocol::newGameclientMessage(139);
    auto* m = message->MutableExtension(GameclientMessageToggleWrapState::gameclient_message_toggle_wrap_state);
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(thingId);
    m->set_stack_position(stackpos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCloseContainer(const uint8_t containerId)
{
    auto message = tibia::protocol::newGameclientMessage(135);
    auto* m = message->MutableExtension(GameclientMessageCloseContainer::gameclient_message_close_container);
    m->set_container_id(containerId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendUpContainer(const uint8_t containerId)
{
    auto message = tibia::protocol::newGameclientMessage(136);
    auto* m = message->MutableExtension(GameclientMessageUpContainer::gameclient_message_up_container);
    m->set_container_id(containerId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEditText(const uint32_t id, const std::string_view text)
{
    auto message = tibia::protocol::newGameclientMessage(137);
    auto* m = message->MutableExtension(GameclientMessageEditText::gameclient_message_edit_text);
    m->set_window_id(id);
    m->set_text(std::string(text));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEditList(const uint32_t id, const uint8_t doorId, const std::string_view text)
{
    auto message = tibia::protocol::newGameclientMessage(138);
    auto* m = message->MutableExtension(GameclientMessageEditList::gameclient_message_edit_list);
    m->set_list_type(doorId);
    m->set_window_id(id);
    m->set_text(std::string(text));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendLook(const Position& position, const uint16_t itemId, const uint8_t stackpos)
{
    auto message = tibia::protocol::newGameclientMessage(140);
    auto* m = message->MutableExtension(GameclientMessageLook::gameclient_message_look);
    setCoordinate(m->mutable_position(), position);
    m->set_object_id(itemId);
    m->set_stack_position(stackpos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendLookCreature(const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(141);
    auto* m = message->MutableExtension(GameclientMessageLookAtCreature::gameclient_message_look_at_creature);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendTalk(const tibia::client::MessageMode mode, const uint16_t channelId, const std::string_view receiver, const std::string_view message)
{
    // Consume any staged crosshair aim up front so an aborted/empty say never leaks it to the next one.
    const Position talkAimPos = tibia::game::takeNextTalkAim();
    const uint8_t talkAimMode = tibia::game::takeNextTalkAimMode();
    if (message.empty())
        return;
    if (message.length() > UINT8_MAX) {
        qCritical("%s", std::format("message too large").c_str());
        return;
    }
    auto talk = tibia::protocol::newGameclientMessage(150);
    auto* m = talk->MutableExtension(GameclientMessageTalk::gameclient_message_talk);
    m->set_mode(Proto::translateMessageModeToServer(mode));
    switch (mode) {
        case tibia::client::MessagePrivateTo:
        case tibia::client::MessageGamemasterPrivateTo:
            m->set_receiver(std::string(receiver));
            break;
        case tibia::client::MessageChannel:
        case tibia::client::MessageChannelHighlight:
        case tibia::client::MessageChannelManagement:
        case tibia::client::MessageGamemasterChannel:
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
        setCoordinate(m->mutable_aim_position(), talkAimPos);
    }
    tibia::protocol::sendGameclientMessage(std::move(talk));
}

void sendRequestChannels()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientRequestChannels));
}

void sendJoinChannel(const uint16_t channelId)
{
    auto message = tibia::protocol::newGameclientMessage(152);
    auto* m = message->MutableExtension(GameclientMessageJoinChannel::gameclient_message_join_channel);
    m->set_channel_id(channelId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendLeaveChannel(const uint16_t channelId)
{
    auto message = tibia::protocol::newGameclientMessage(153);
    auto* m = message->MutableExtension(GameclientMessageLeaveChannel::gameclient_message_leave_channel);
    m->set_channel_id(channelId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOpenPrivateChannel(const std::string_view receiver)
{
    auto message = tibia::protocol::newGameclientMessage(154);
    auto* m = message->MutableExtension(GameclientMessagePrivateChannel::gameclient_message_private_channel);
    m->set_receiver(std::string(receiver));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEditGuildMessage(const std::string_view text)
{
    // CIP EditGuildMessage (C 0x9C).
    auto message = tibia::protocol::newGameclientMessage(156);
    auto* m = message->MutableExtension(GameclientMessageEditGuildMessage::gameclient_message_edit_guild_message);
    m->set_text(std::string(text));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCloseNpcChannel()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientCloseNpcChannel));
}

void sendChangeFightModes(const tibia::client::FightModes fightMode, const tibia::client::ChaseModes chaseMode, const bool safeFight, const tibia::client::PVPModes pvpMode)
{
    auto message = tibia::protocol::newGameclientMessage(160);
    auto* m = message->MutableExtension(GameclientMessageSetTactics::gameclient_message_set_tactics);
    m->set_chase_opponent(chaseMode != 0);
    m->set_secure_mode(safeFight);
    m->set_pvp_mode(pvpMode);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendAttack(const uint32_t creatureId, const uint32_t seq)
{
    auto message = tibia::protocol::newGameclientMessage(161);
    auto* m = message->MutableExtension(GameclientMessageAttack::gameclient_message_attack);
    m->set_creature_id(creatureId);
    m->set_sequence(seq);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendFollow(const uint32_t creatureId, const uint32_t seq)
{
    auto message = tibia::protocol::newGameclientMessage(162);
    auto* m = message->MutableExtension(GameclientMessageFollow::gameclient_message_follow);
    m->set_creature_id(creatureId);
    m->set_sequence(seq);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendInviteToParty(const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(163);
    auto* m = message->MutableExtension(GameclientMessageInviteToParty::gameclient_message_invite_to_party);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendJoinParty(const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(164);
    auto* m = message->MutableExtension(GameclientMessageJoinParty::gameclient_message_join_party);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRevokeInvitation(const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(165);
    auto* m = message->MutableExtension(GameclientMessageRevokeInvitation::gameclient_message_revoke_invitation);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendPassLeadership(const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(166);
    auto* m = message->MutableExtension(GameclientMessagePassLeadership::gameclient_message_pass_leadership);
    m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendLeaveParty()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientLeaveParty));
}

void sendDisbandParty()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientDisbandParty));
}

void sendShareExperience(const bool active)
{
    auto message = tibia::protocol::newGameclientMessage(168);
    auto* m = message->MutableExtension(GameclientMessageShareExperience::gameclient_message_share_experience);
    m->set_active(active);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendPartyAnalyzerAction(const uint8_t action, const std::vector<std::tuple<uint16_t, uint64_t>>& items)
{
    auto message = tibia::protocol::newGameclientMessage(43);
    auto* m = message->MutableExtension(GameclientMessagePartyHuntAnalyser::gameclient_message_party_hunt_analyser);
    m->set_action(action);
    if (action == tibia::client::PARTYANALYZERACTION_PRICEVALUE) {
        for (const auto& [itemId, price] : items) {
            auto* entry = m->add_prices();
            entry->set_object_id(itemId);
            entry->set_price(price);
        }
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
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

void sendOpenDepotSearch()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientOpenDepotSearch));
}

void sendCloseDepotSearch()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientCloseDepotSearch));
}

void sendDepotSearchType(const uint16_t itemId, const uint8_t tier)
{
    auto message = tibia::protocol::newGameclientMessage(148);
    auto* m = message->MutableExtension(GameclientMessageDepotSearchType::gameclient_message_depot_search_type);
    m->set_object_id(itemId);
    if (tibia::game::rawThingType(itemId, ThingCategoryItem) && tibia::game::rawThingType(itemId, ThingCategoryItem)->getClassification() > 0)
        m->set_tier(tier);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRetrieveDepotSearch(const uint16_t itemId, const uint8_t tier, const uint8_t type)
{
    auto message = tibia::protocol::newGameclientMessage(41);
    auto* m = message->MutableExtension(GameclientMessageDepotSearchRetrieve::gameclient_message_depot_search_retrieve);
    m->set_object_id(itemId);
    if (tibia::game::rawThingType(itemId, ThingCategoryItem) && tibia::game::rawThingType(itemId, ThingCategoryItem)->getClassification() > 0)
        m->set_tier(tier);
    m->set_retrieve_type(type);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOpenParentContainer(const Position& pos)
{
    auto message = tibia::protocol::newGameclientMessage(149);
    auto* m = message->MutableExtension(GameclientMessageOpenParentContainer::gameclient_message_open_parent_container);
    setCoordinate(m->mutable_position(), pos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendGatherAll(const Position& pos, const uint16_t itemId, const uint8_t stackpos)
{
    // Crystal parseRewardChestCollect (C 0xFF)
    auto message = tibia::protocol::newGameclientMessage(255);
    auto* m = message->MutableExtension(GameclientMessageGatherAll::gameclient_message_gather_all);
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(itemId);
    m->set_stack_position(stackpos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOpenOwnChannel()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientOpenOwnChannel));
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

void sendCancelAttackAndFollow()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientCancelAttackAndFollow));
}

void sendRequestBless()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientRequestBless));
}

void sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, const bool autoTrackNewQuests, const bool autoUntrackCompletedQuests, const uint8_t extra)
{
    (void)extra;
    auto message = tibia::protocol::newGameclientMessage(208);
    auto* m = message->MutableExtension(GameclientMessageTrackQuestFlags::gameclient_message_track_quest_flags);
    const auto missionCount = std::min(missionIds.size(), static_cast<size_t>(255));
    for (size_t i = 0; i < missionCount; ++i)
        m->add_mission_ids(missionIds[i]);
    m->set_auto_track_new_quests(autoTrackNewQuests);
    m->set_auto_untrack_completed_quests(autoUntrackCompletedQuests);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestOutfit()
{
    auto message = tibia::protocol::newGameclientMessage(210);
    auto* m = message->MutableExtension(GameclientMessageGetOutfit::gameclient_message_get_outfit);
    m->set_outfit_type(0); // the player's own outfit
    tibia::protocol::sendGameclientMessage(std::move(message));
}

namespace {

}

void sendChangeOutfit(const Outfit& outfit, const bool mounted, const bool randomMount)
{
    auto message = tibia::protocol::newGameclientMessage(211);
    auto* m = message->MutableExtension(GameclientMessageSetOutfit::gameclient_message_set_outfit);
    m->set_outfit_type(0);
    auto* look = m->mutable_look();
    look->set_look_type(outfit.getId());
    look->set_head(outfit.getHead());
    look->set_body(outfit.getBody());
    look->set_legs(outfit.getLegs());
    look->set_feet(outfit.getFeet());
    look->set_addons(outfit.getAddons());
    auto* mount = m->mutable_mount();
    mount->set_look_type(outfit.getMount());
    mount->set_head(outfit.getMountHead());
    mount->set_body(outfit.getMountBody());
    mount->set_legs(outfit.getMountLegs());
    mount->set_feet(outfit.getMountFeet());
    m->set_mounted(mounted);
    m->set_familiar(outfit.getFamiliar());
    m->set_random_mount(randomMount);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendSetShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, const uint16_t itemId,
                                                     const uint8_t stackpos, const uint8_t direction,
                                                     const uint8_t podiumVisible)
{
    auto message = tibia::protocol::newGameclientMessage(211);
    auto* m = message->MutableExtension(GameclientMessageSetOutfit::gameclient_message_set_outfit);
    m->set_outfit_type(2);
    auto* look = m->mutable_look();
    look->set_look_type(outfit.getId());
    look->set_head(outfit.getHead());
    look->set_body(outfit.getBody());
    look->set_legs(outfit.getLegs());
    look->set_feet(outfit.getFeet());
    look->set_addons(outfit.getAddons());
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(itemId);
    m->set_stack_position(stackpos);
    auto* mount = m->mutable_mount();
    mount->set_look_type(outfit.getMount());
    mount->set_head(outfit.getMountHead());
    mount->set_body(outfit.getMountBody());
    mount->set_legs(outfit.getMountLegs());
    mount->set_feet(outfit.getMountFeet());
    m->set_direction(direction);
    m->set_podium_visible(podiumVisible != 0);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendSetHirelingOutfit(const Outfit& outfit, const uint32_t hirelingCreatureId)
{
    auto message = tibia::protocol::newGameclientMessage(211);
    auto* m = message->MutableExtension(GameclientMessageSetOutfit::gameclient_message_set_outfit);
    m->set_outfit_type(1);
    auto* look = m->mutable_look();
    look->set_look_type(outfit.getId());
    look->set_head(outfit.getHead());
    look->set_body(outfit.getBody());
    look->set_legs(outfit.getLegs());
    look->set_feet(outfit.getFeet());
    look->set_addons(outfit.getAddons());
    m->set_hireling_id(hirelingCreatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendMountStatus(const bool mount)
{
    auto message = tibia::protocol::newGameclientMessage(212);
    auto* m = message->MutableExtension(GameclientMessageMount::gameclient_message_mount);
    m->set_mounted(mount);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendAddVip(const std::string_view name)
{
    auto message = tibia::protocol::newGameclientMessage(220);
    auto* m = message->MutableExtension(GameclientMessageAddBuddy::gameclient_message_add_buddy);
    m->set_name(std::string(name));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRemoveVip(const uint32_t playerId)
{
    auto message = tibia::protocol::newGameclientMessage(221);
    auto* m = message->MutableExtension(GameclientMessageRemoveBuddy::gameclient_message_remove_buddy);
    m->set_character_id(playerId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEditVip(const uint32_t playerId, const std::string_view description, const uint32_t iconId, const bool notifyLogin, const std::vector<uint8_t>& groupIDs)
{
    auto message = tibia::protocol::newGameclientMessage(222);
    auto* m = message->MutableExtension(GameclientMessageEditBuddy::gameclient_message_edit_buddy);
    m->set_character_id(playerId);
    m->set_description(std::string(description));
    m->set_icon_id(iconId);
    m->set_notify_login(notifyLogin);
    for (const uint8_t groupID : groupIDs)
        m->add_group_ids(groupID);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendEditVipGroups(const tibia::client::GroupsEditInfoType_t action, const uint8_t groupId, const std::string_view groupName)
{
    auto message = tibia::protocol::newGameclientMessage(223);
    auto* m = message->MutableExtension(GameclientMessageBuddyGroup::gameclient_message_buddy_group);
    m->set_action(action);
    m->set_group_id(groupId);
    m->set_name(std::string(groupName));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendWheelGemAction(uint8_t actionType, uint16_t param, uint8_t pos)
{
    auto message = tibia::protocol::newGameclientMessage(231);
    auto* m = message->MutableExtension(GameclientMessageGemAtelier::gameclient_message_gem_atelier);
    m->set_action(actionType);
    if (actionType == 0 || actionType == 2 || actionType == 3)
        m->set_gem_index(param);
    else
        m->set_first_value(static_cast<uint8_t>(param));
    m->set_second_value(pos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRuleViolationReport(const uint8_t type, const uint8_t reason, const std::string_view name, const std::string_view comment,
                                           const std::string_view translation, const uint32_t id)
{
    auto message = tibia::protocol::newGameclientMessage(242);
    auto* m = message->MutableExtension(GameclientMessageRuleViolationReport::gameclient_message_rule_violation_report);
    m->set_report_type(type);
    m->set_reason(reason);
    m->set_name(std::string(name));
    m->set_comment(std::string(comment));
    m->set_translation(std::string(translation));
    m->set_statement_id(id);
    tibia::protocol::sendGameclientMessage(std::move(message));
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

void sendGetGuildMessage()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientGuildMessage));
}

void sendGetTextForReport(const Position& position, const uint16_t typeId, const uint8_t stackPosition)
{
    // CIP GetTextForReport (C 0x9D).
    auto message = tibia::protocol::newGameclientMessage(157);
    auto* m = message->MutableExtension(GameclientMessageGetTextForReport::gameclient_message_get_text_for_report);
    setCoordinate(m->mutable_position(), position);
    m->set_object_id(typeId);
    m->set_stack_position(stackPosition);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendGetOfferDescription(const uint32_t offerId)
{
    // CIP GetOfferDescription (C 0xE8). Server OfferDescription answers with u32 + string.
    auto message = tibia::protocol::newGameclientMessage(232);
    auto* m = message->MutableExtension(GameclientMessageGetOfferDescription::gameclient_message_get_offer_description);
    m->set_offer_id(offerId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestQuestLog()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientRequestQuestLog));
}

void sendRequestQuestLine(const uint16_t questId)
{
    auto message = tibia::protocol::newGameclientMessage(241);
    auto* m = message->MutableExtension(GameclientMessageGetQuestLine::gameclient_message_get_quest_line);
    m->set_quest_id(questId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestItemInfo(const uint16_t itemId, const uint8_t subType, const uint8_t index)
{
    (void)index;
    auto message = tibia::protocol::newGameclientMessage(243);
    auto* m = message->MutableExtension(GameclientMessageGetObjectInfo::gameclient_message_get_object_info);
    auto* object = m->add_objects();
    object->set_object_id(itemId);
    object->set_data(subType);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendAnswerModalDialog(const uint32_t dialog, const uint8_t button, const uint8_t choice)
{
    auto message = tibia::protocol::newGameclientMessage(249);
    auto* m = message->MutableExtension(GameclientMessageAnswerModalDialog::gameclient_message_answer_modal_dialog);
    m->set_dialog_id(dialog);
    m->set_button(button);
    m->set_choice(choice);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendBrowseField(const Position& position)
{

    auto message = tibia::protocol::newGameclientMessage(203);
    auto* m = message->MutableExtension(GameclientMessageBrowseField::gameclient_message_browse_field);
    setCoordinate(m->mutable_position(), position);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendSeekInContainer(const uint8_t containerId, const uint16_t index)
{

    auto message = tibia::protocol::newGameclientMessage(204);
    auto* m = message->MutableExtension(GameclientMessageSeekInContainer::gameclient_message_seek_in_container);
    m->set_container_id(containerId);
    m->set_index(index);
    m->set_primary_type(0);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendInspectionNormalObject(const Position& position)
{
    auto message = tibia::protocol::newGameclientMessage(205);
    auto* m = message->MutableExtension(GameclientMessageInspectObject::gameclient_message_inspect_object);
    m->set_inspection_type(tibia::client::INSPECT_NORMALOBJECT);
    setCoordinate(m->mutable_position(), position);
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
    m->set_object_id(itemId);
    m->set_count(itemCount);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestBestiary()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientBestiaryRequest));
}

void sendInspectCharacter(const uint32_t creatureId, const uint8_t tab)
{
    auto message = tibia::protocol::newGameclientMessage(206);
    auto* m = message->MutableExtension(GameclientMessageInspectPlayer::gameclient_message_inspect_player);
    m->set_tab(tab);
    if (tab >= 1 && tab <= 5)
        m->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestBestiaryOverview(const std::string_view catName, bool search, std::vector<uint16_t> raceIds)
{
    auto message = tibia::protocol::newGameclientMessage(226);
    auto* m = message->MutableExtension(GameclientMessageOpenMonsterCyclopediaMonsters::gameclient_message_open_monster_cyclopedia_monsters);
    m->set_request_type(search ? 1 : 0);
    if (search) {
        for (const uint16_t raceId : raceIds)
            m->add_race_ids(raceId);
    } else {
        m->set_category(std::string(catName));
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestBestiarySearch(const uint16_t raceId)
{
    auto message = tibia::protocol::newGameclientMessage(227);
    auto* m = message->MutableExtension(GameclientMessageOpenMonsterCyclopediaRace::gameclient_message_open_monster_cyclopedia_race);
    m->set_race_id(raceId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendBuyCharmRune(const uint8_t runeId, const uint8_t action, const uint16_t raceId)
{
    auto message = tibia::protocol::newGameclientMessage(228);
    auto* m = message->MutableExtension(GameclientMessageMonsterBonusEffectAction::gameclient_message_monster_bonus_effect_action);
    m->set_action(action);
    m->set_charm_id(runeId);
    m->set_race_id(raceId);
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
                if (tibia::game::rawThingType(itemId, ThingCategoryItem) && tibia::game::rawThingType(itemId, ThingCategoryItem)->getClassification() > 0)
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

void sendConfigureShowOffSocket(const Position& pos, const uint16_t itemId, const uint8_t stackpos)
{
    // Crystal parseConfigureShowOffSocket (C 0x86): Position, u16 itemId, u8 stackpos.
    auto message = tibia::protocol::newGameclientMessage(134);
    auto* m = message->MutableExtension(GameclientMessageConfigureShowOffSocket::gameclient_message_configure_show_off_socket);
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(itemId);
    m->set_stack_position(stackpos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendJoinAggression(const uint32_t creatureId)
{
    auto message = tibia::protocol::newGameclientMessage(Proto::ClientJoinAggression);
    message->MutableExtension(GameclientMessageJoinAggression::gameclient_message_join_aggression)->set_creature_id(creatureId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendSetCreaturePodiumConfiguration(const uint32_t monsterRaceId, const Position& pos,
                                                      const uint16_t itemId, const uint8_t stackpos,
                                                      const uint8_t direction, const uint8_t podiumVisible,
                                                      const uint8_t monsterVisible)
{
    // Crystal parseSetMonsterPodium (C 0x9F).
    auto message = tibia::protocol::newGameclientMessage(159);
    auto* m = message->MutableExtension(GameclientMessageSetCreaturePodiumConfiguration::gameclient_message_set_creature_podium_configuration);
    m->set_race_id(monsterRaceId);
    setCoordinate(m->mutable_position(), pos);
    m->set_object_id(itemId);
    m->set_stack_position(stackpos);
    m->set_direction(direction);
    m->set_podium_visible(podiumVisible != 0);
    m->set_monster_visible(monsterVisible != 0);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendGetTransactionDetails(const uint32_t transactionId)
{
    // CIP GetTransactionDetails (C 0xC9). Crystal currently no-ops the opcode.
    auto message = tibia::protocol::newGameclientMessage(201);
    auto* m = message->MutableExtension(GameclientMessageGetTransactionDetails::gameclient_message_get_transaction_details);
    m->set_transaction_id(transactionId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendStoreEvent(const uint8_t eventType)
{
    auto message = tibia::protocol::newGameclientMessage(233);
    auto* m = message->MutableExtension(GameclientMessageStoreEvent::gameclient_message_store_event);
    m->set_event_type(eventType);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendClientCheck(const std::vector<uint8_t>& data)
{
    auto message = tibia::protocol::newGameclientMessage(Proto::ClientCheck);
    message->MutableExtension(GameclientMessageClientCheck::gameclient_message_client_check)->set_data(std::string(data.begin(), data.end()));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendContainerAction(const uint8_t action, const uint8_t containerId)
{
    auto message = tibia::protocol::newGameclientMessage(117);
    auto* m = message->MutableExtension(GameclientMessageContainerAction::gameclient_message_container_action);
    m->set_container_id(containerId);
    if (action == 1) {
        m->set_move_to_managed_containers(true);
    } else {
        auto* sort = m->mutable_sort();
        sort->set_sort_type(0);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendClientDetails(const std::string_view details)
{
    // CIP ClientDetails (C 0xC1); conservative string payload.
    auto message = tibia::protocol::newGameclientMessage(193);
    auto* m = message->MutableExtension(GameclientMessageClientDetails::gameclient_message_client_details);
    m->set_details(std::string(details));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendMarketStatistics(const uint16_t itemId, const uint8_t tier)
{
    // CIP sends the request without the item.
    (void)itemId;
    (void)tier;
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientMarketStatistics));
}

void sendSetHirelingName(const std::string_view name)
{
    // CIP SetHirelingName (C 0xEC); GameStore C_ParseHirelingName.
    auto message = tibia::protocol::newGameclientMessage(236);
    auto* m = message->MutableExtension(GameclientMessageSetHirelingName::gameclient_message_set_hireling_name);
    m->set_name(std::string(name));
    m->set_creature_id(0);
    m->set_object_id(0);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCyclopediaRequestCharacterInfo(const uint32_t playerId, const tibia::client::CyclopediaCharacterInfoType_t characterInfoType, const uint16_t entriesPerPage, const uint16_t page)
{
    auto message = tibia::protocol::newGameclientMessage(229);
    auto* m = message->MutableExtension(GameclientMessageOpenCyclopediaCharacterInfo::gameclient_message_open_cyclopedia_character_info);
    m->set_character_id(playerId);
    m->set_info_type(characterInfoType);
    if (characterInfoType == 3 || characterInfoType == 4)
        m->set_entries_per_page(entriesPerPage);
    if (characterInfoType == 3 || characterInfoType == 4)
        m->set_page(page);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCyclopediaHouseAuction(const tibia::client::CyclopediaHouseAuctionType_t type, const uint32_t houseId, const uint32_t timestamp, const uint64_t bidValue, const std::string_view name)
{
    auto message = tibia::protocol::newGameclientMessage(173);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action);
    m->set_action(type);
    m->set_name(std::string(name));
    m->set_house_id(houseId);
    m->set_bid(bidValue);
    m->set_timestamp(timestamp);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCyclopediaMapDonate(const uint16_t areaId, const uint32_t amount)
{
    auto message = tibia::protocol::newGameclientMessage(219);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaMapAction::gameclient_message_cyclopedia_map_action);
    auto* donation = m->mutable_donation();
    donation->set_area_id(areaId);
    donation->set_amount(amount);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendCyclopediaMapResetViewpoints(const uint16_t subAreaId)
{
    auto message = tibia::protocol::newGameclientMessage(219);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaMapAction::gameclient_message_cyclopedia_map_action);
    m->set_reset_area_id(subAreaId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestBosstiaryInfo()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientBosstiaryRequestInfo));
}

void sendRequestBossSlootInfo()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientBosstiaryRequestSlotInfo));
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

void sendBuyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location)
{
    (void)location;
    auto message = tibia::protocol::newGameclientMessage(252);
    auto* m = message->MutableExtension(GameclientMessageBuyStoreOffer::gameclient_message_buy_store_offer);
    m->set_offer_id(offerId);
    m->set_action(action);
    m->set_name(std::string(name));
    m->set_type(type);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestTransactionHistory(const uint32_t page, const uint32_t entriesPerPage)
{
    auto message = tibia::protocol::newGameclientMessage(254);
    auto* m = message->MutableExtension(GameclientMessageGetTransactionHistory::gameclient_message_get_transaction_history);
    m->set_page(page);
    m->set_entries_per_page(static_cast<uint8_t>(entriesPerPage));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder, const uint8_t serviceType)
{
    auto message = tibia::protocol::newGameclientMessage(251);
    auto* m = message->MutableExtension(GameclientMessageRequestStoreOffers::gameclient_message_request_store_offers);
    m->set_request_type(tibia::client::Store_Type_Actions_t::OPEN_CATEGORY);
    m->set_category(std::string(categoryName));
    m->set_subcategory(std::string(subCategory));
    m->set_sort_order(sortOrder);
    m->set_service_type(serviceType);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestStoreHome()
{
    auto message = tibia::protocol::newGameclientMessage(251);
    auto* m = message->MutableExtension(GameclientMessageRequestStoreOffers::gameclient_message_request_store_offers);
    m->set_request_type(tibia::client::Store_Type_Actions_t::OPEN_HOME);
    tibia::protocol::sendGameclientMessage(std::move(message));
}
void sendRequestStorePremiumBoost()
{
    auto message = tibia::protocol::newGameclientMessage(251);
    auto* m = message->MutableExtension(GameclientMessageRequestStoreOffers::gameclient_message_request_store_offers);
    m->set_request_type(tibia::client::Store_Type_Actions_t::OPEN_PREMIUM_BOOST);
    m->set_value(1);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestUsefulThings(const uint8_t offerId)
{
    auto message = tibia::protocol::newGameclientMessage(251);
    auto* m = message->MutableExtension(GameclientMessageRequestStoreOffers::gameclient_message_request_store_offers);
    m->set_request_type(tibia::client::Store_Type_Actions_t::OPEN_USEFUL_THINGS);
    m->set_value(offerId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestStoreOfferById(uint32_t offerId, const uint8_t sortOrder, const uint8_t serviceType)
{
    auto message = tibia::protocol::newGameclientMessage(251);
    auto* m = message->MutableExtension(GameclientMessageRequestStoreOffers::gameclient_message_request_store_offers);
    m->set_request_type(tibia::client::Store_Type_Actions_t::OPEN_OFFER);
    m->set_value(offerId);
    m->set_sort_order(sortOrder);
    m->set_service_type(serviceType);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestStoreSearch(const std::string_view searchText, const uint8_t sortOrder, const uint8_t serviceType)
{
    auto message = tibia::protocol::newGameclientMessage(251);
    auto* m = message->MutableExtension(GameclientMessageRequestStoreOffers::gameclient_message_request_store_offers);
    m->set_request_type(tibia::client::Store_Type_Actions_t::OPEN_SEARCH);
    m->set_search_text(std::string(searchText));
    m->set_sort_order(sortOrder);
    m->set_service_type(serviceType);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOpenStore(const uint8_t serviceType, const std::string_view category)
{
    (void)serviceType;
    (void)category;
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientOpenStore));
}

void sendTransferCoins(const std::string_view recipient, const uint32_t amount)
{
    auto message = tibia::protocol::newGameclientMessage(239);
    auto* m = message->MutableExtension(GameclientMessageTransferCurrency::gameclient_message_transfer_currency);
    m->set_recipient(std::string(recipient));
    m->set_amount(amount);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOpenTransactionHistory(const uint8_t entriesPerPage)
{
    auto message = tibia::protocol::newGameclientMessage(253);
    auto* m = message->MutableExtension(GameclientMessageOpenTransactionHistory::gameclient_message_open_transaction_history);
    m->set_entries_per_page(entriesPerPage);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendMarketLeave()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientMarketLeave));
}

void sendMarketBrowse(const uint8_t browseId, const uint16_t browseType, const uint8_t tier)
{
    auto message = tibia::protocol::newGameclientMessage(245);
    auto* m = message->MutableExtension(GameclientMessageMarketBrowse::gameclient_message_market_browse);
    m->set_browse_type(browseId);
    if (browseId == 3) {
        m->set_object_id(browseType);
        if (tibia::game::rawThingType(browseType, ThingCategoryItem) && tibia::game::rawThingType(browseType, ThingCategoryItem)->getClassification() > 0)
            m->set_tier(tier);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendMarketCreateOffer(const uint8_t type, const uint16_t itemId, const uint8_t itemTier, const uint16_t amount, const uint64_t price, const uint8_t anonymous)
{
    auto message = tibia::protocol::newGameclientMessage(246);
    auto* m = message->MutableExtension(GameclientMessageMarketCreate::gameclient_message_market_create);
    m->set_offer_type(type);
    m->set_object_id(itemId);
    if (tibia::game::rawThingType(itemId, ThingCategoryItem) && tibia::game::rawThingType(itemId, ThingCategoryItem)->getClassification() > 0)
        m->set_tier(itemTier);
    m->set_amount(amount);
    m->set_price(price);
    m->set_anonymous(anonymous != 0);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendMarketCancelOffer(const uint32_t timestamp, const uint16_t counter)
{
    auto message = tibia::protocol::newGameclientMessage(247);
    auto* m = message->MutableExtension(GameclientMessageMarketCancel::gameclient_message_market_cancel);
    m->set_timestamp(timestamp);
    m->set_counter(counter);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendMarketAcceptOffer(const uint32_t timestamp, const uint16_t counter, const uint16_t amount)
{
    auto message = tibia::protocol::newGameclientMessage(248);
    auto* m = message->MutableExtension(GameclientMessageMarketAccept::gameclient_message_market_accept);
    m->set_timestamp(timestamp);
    m->set_counter(counter);
    m->set_amount(amount);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendPreyAction(const uint8_t slot, const uint8_t actionType, const uint16_t index)
{
    auto message = tibia::protocol::newGameclientMessage(235);
    auto* m = message->MutableExtension(GameclientMessagePreyAction::gameclient_message_prey_action);
    m->set_slot(slot);
    m->set_action(actionType);
    if (actionType == 4)
        m->set_race_id(index);
    else
        m->set_index(static_cast<uint8_t>(index));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendRequestResourceBalance(const uint8_t resourceType)
{
    auto message = tibia::protocol::newGameclientMessage(237);
    auto* m = message->MutableExtension(GameclientMessageRequestResourceBalance::gameclient_message_request_resource_balance);
    m->set_resource_type(resourceType);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendSetClientOptions(const uint8_t option, const bool value)
{
    auto message = tibia::protocol::newGameclientMessage(46);
    auto* m = message->MutableExtension(GameclientMessageSetClientOptions::gameclient_message_set_client_options);
    if (option == 1) // the only client option CIP sends
        m->set_turn_towards_move_direction(value);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendForgeRequest(tibia::client::ForgeAction_t actionType, bool convergence, uint16_t firstItemid, uint8_t firstItemTier, uint16_t secondItemId, bool improveChance, bool tierLoss) {
    auto message = tibia::protocol::newGameclientMessage(191);
    auto* m = message->MutableExtension(GameclientMessageExaltationAction::gameclient_message_exaltation_action);
    m->set_action(static_cast<uint8_t>(actionType));
    m->set_convergence(convergence);
    m->set_first_object_id(firstItemid);
    m->set_tier(firstItemTier);
    m->set_second_object_id(secondItemId);
    m->set_improve_chance(improveChance);
    m->set_reduce_tier_loss(tierLoss);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendForgeBrowseHistoryRequest(uint16_t page) {
    auto message = tibia::protocol::newGameclientMessage(192);
    auto* m = message->MutableExtension(GameclientMessageOpenExaltationHistory::gameclient_message_open_exaltation_history);
    m->set_page(page);
    m->set_entries_per_page(9);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendExivaRestrictions(
    const bool allowAll, const bool allowOwnGuild, const bool allowOwnParty,
    const bool allowVipList, const bool allowPlayerWhitelist, const bool allowGuildWhitelist,
    const std::vector<std::string>& characterWhiteList,
    const std::vector<std::string>& removeCharacter,
    const std::vector<std::string>& guildWhiteList,
    const std::vector<std::string>& removeGuild)
{
    if (!tibia::game::canExivaOptions())
        return;
    auto message = tibia::protocol::newGameclientMessage(202);
    auto* m = message->MutableExtension(GameclientMessageUpdateExivaOptions::gameclient_message_update_exiva_options);
    m->set_allow_all(allowAll);
    m->set_allow_own_guild(allowOwnGuild);
    m->set_allow_own_party(allowOwnParty);
    m->set_allow_vip_list(allowVipList);
    m->set_allow_player_whitelist(allowPlayerWhitelist);
    m->set_allow_guild_whitelist(allowGuildWhitelist);
    for (const auto& name : characterWhiteList)
        m->add_add_characters(name);
    for (const auto& name : removeCharacter)
        m->add_remove_characters(name);
    for (const auto& name : guildWhiteList)
        m->add_add_guilds(name);
    for (const auto& name : removeGuild)
        m->add_remove_guilds(name);
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
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientCloseImbuingWindow));
}

void sendImbuementWindowAction(const uint8_t type, const uint16_t itemId, const Position& pos, const uint8_t stackpos)
{
    auto message = tibia::protocol::newGameclientMessage(178);
    auto* m = message->MutableExtension(GameclientMessagePickObjectToImbue::gameclient_message_pick_object_to_imbue);
    m->set_action(type);
    if (type == tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM)
        setCoordinate(m->mutable_position(), pos);
    if (type == tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM)
        m->set_object_id(itemId);
    if (type == tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM)
        m->set_stack_position(stackpos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOpenRewardWall()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientOpenRewardWall));
}

void sendOpenRewardHistory()
{
    tibia::protocol::sendGameclientMessage(tibia::protocol::newGameclientMessage(Proto::ClientOpenRewardHistory));
}

void sendGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items)
{
    auto message = tibia::protocol::newGameclientMessage(218);
    auto* m = message->MutableExtension(GameclientMessageCollectDailyReward::gameclient_message_collect_daily_reward);
    m->set_from_shrine(bonusShrine != 0);
    for (const auto& [itemId, count] : items) {
        auto* choice = m->add_choices();
        choice->set_object_id(itemId);
        choice->set_count(count);
    }
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendStashWithdraw(const uint16_t itemId, const uint32_t count, const uint8_t stackpos)
{
    auto message = tibia::protocol::newGameclientMessage(40);
    auto* m = message->MutableExtension(GameclientMessageStashAction::gameclient_message_stash_action);
    m->set_action(tibia::client::Supply_Stash_Actions_t::SUPPLY_STASH_ACTION_WITHDRAW);
    m->set_object_id(itemId);
    m->set_count(count);
    m->set_stack_position(stackpos);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendGreet(const uint32_t npcId)
{
    auto message = tibia::protocol::newGameclientMessage(238);
    auto* m = message->MutableExtension(GameclientMessageGreet::gameclient_message_greet);
    m->set_creature_id(npcId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendStashStow(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action)
{
    auto message = tibia::protocol::newGameclientMessage(40);
    auto* m = message->MutableExtension(GameclientMessageStashAction::gameclient_message_stash_action);
    m->set_action(action);
    setCoordinate(m->mutable_position(), position);
    m->set_object_id(itemId);
    m->set_stack_position(stackpos);
    m->set_count(std::min<uint32_t>(count, 0xff));
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendTaskBoardAction(const uint8_t option, const uint16_t value, const uint16_t extraValue)
{
    auto message = tibia::protocol::newGameclientMessage(95);
    auto* m = message->MutableExtension(GameclientMessageTaskboard::gameclient_message_taskboard);
    m->set_option(option);
    m->set_slot_index(value);
    m->set_race_id(extraValue);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendImbuementDurations(const bool isOpen)
{
    auto message = tibia::protocol::newGameclientMessage(96);
    auto* m = message->MutableExtension(GameclientMessageTrackImbuements::gameclient_message_track_imbuements);
    m->set_open(isOpen);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendQuickLoot(const uint8_t variant, const Position& pos, const uint16_t itemId, const uint8_t stackpos)
{
    auto message = tibia::protocol::newGameclientMessage(143);
    auto* m = message->MutableExtension(GameclientMessageQuickLoot::gameclient_message_quick_loot);
    m->set_variant(variant);
    if (variant < 2)
        setCoordinate(m->mutable_position(), pos);
    if (variant < 2)
        m->set_object_id(itemId);
    if (variant < 2)
        m->set_stack_position(stackpos);
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
    if (action == 0 || action == 4) {
        setCoordinate(m->mutable_position(), pos);
        m->set_object_id(itemId);
        m->set_stack_position(stackpos);
    }
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

void sendOpenWheel(const uint32_t playerId)
{
    auto message = tibia::protocol::newGameclientMessage(97);
    auto* m = message->MutableExtension(GameclientMessageRequestSkillWheel::gameclient_message_request_skill_wheel);
    m->set_character_id(playerId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendApplyWheelPoints(const std::vector<uint16_t>& slotPoints,
                                        const std::array<uint16_t, 8>& vesselGems)
{
    auto message = tibia::protocol::newGameclientMessage(98);
    auto* m = message->MutableExtension(GameclientMessageApplySkillWheel::gameclient_message_apply_skill_wheel);
    for (int i = 0; i < 36; ++i)
        m->add_slot_points(i < static_cast<int>(slotPoints.size()) ? slotPoints[i] : 0);
    for (const uint16_t gemId : vesselGems)
        m->add_vessel_gems(gemId);
    tibia::protocol::sendGameclientMessage(std::move(message));
}

}
