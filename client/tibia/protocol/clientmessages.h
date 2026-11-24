#pragma once

// CIP client messages (see clientmessages.cpp): one builder per message.

#include "client/const.h"
#include "client/outfit.h"
#include "client/position.h"
#include "client/staticdata.h"
#include "teamfinder/teamfinder_types.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tibia::input {
class IGameActionContext;
}

namespace tibia::protobuf::protocol {
class ObjectIdentifier;
class ObjectPosition;
}

namespace tibia::protocol::gameclient {

/// A classic object (position, stack position, type) as the ObjectPosition and
/// ObjectIdentifier of a client message (CipSoft's TGameActionContextHelper
/// fills them from the location the classic position names). The identifier
/// may be null.
void setClassicObject(::tibia::protobuf::protocol::ObjectPosition* objectPosition,
                      ::tibia::protobuf::protocol::ObjectIdentifier* objectIdentifier, const Position& position,
                      int stackPosition, uint16_t typeId);

void sendLogout();
void sendTurnNorth();
void sendTurnEast();
void sendTurnSouth();
void sendTurnWest();
void sendUseItem(const Position& position, uint16_t itemId, uint8_t stackpos, uint8_t index);
void sendUseItemWith(const Position& fromPos, uint16_t itemId, uint8_t fromStackPos, const Position& toPos, uint16_t toThingId, uint8_t toStackPos);
void sendUseOnCreature(const Position& pos, uint16_t thingId, uint8_t stackpos, uint32_t creatureId);
/// `mode` is CIP's chat message mode (the protocol's).
void sendTalk(uint32_t mode, uint16_t channelId, std::string_view receiver, std::string_view message);
void sendOpenPrivateChannel(std::string_view receiver);
void sendJoinParty(uint32_t creatureId);
void sendDisbandParty();
void sendTeamFinderLeaderAction(uint8_t action, uint32_t memberId = 0, uint8_t memberStatus = 0, const TeamFinderAssemble& assemble = {});
void sendTeamFinderMemberAction(uint8_t action, uint32_t leaderId = 0);
void sendInviteToOwnChannel(std::string_view name);
void sendExcludeFromOwnChannel(std::string_view name);
void sendRequestBless();
void sendBugReport(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position);
void sendRequestQuestLog();
void sendRequestQuestLine(uint16_t questId);
void sendApplyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm);
void sendClearImbuement(uint8_t slot);
void sendCloseImbuingWindow();
void sendImbuementWindowAction(uint8_t type, uint16_t itemId = 0, const Position& pos = Position(), uint8_t stackpos = 0);
void sendImbuementDurations(bool isOpen = false);
void sendSoulSealsAction(const uint16_t raceId);
void sendFriendSystemAction(uint8_t action, uint8_t value = 0);
/// CIP FriendSystemAction with an account: 3 invite, 4 accept into group `value`, 5 decline,
/// 6 cancel invite, 7 blacklist, 8 unblacklist, 9 remove friend, 10 move to group `value`.
void sendFriendSystemAccountAction(uint8_t action, uint32_t accountId, uint8_t value = 0);
/// CIP FriendSystemAction 11: search an account by character name.
void sendFriendSystemSearch(const std::string& characterName);
/// CIP FriendSystemAction 13: show or hide a badge.
void sendFriendSystemBadgeVisibility(uint32_t badgeId, bool visible);
/// CIP FriendSystemAction 16: what one friend group may see.
void sendFriendSystemSetting(uint8_t settingType, bool showCharacterInfo, bool showAccountInfo, bool allowInspect);
void sendCharacterTradeConfigurationAction(uint8_t action, uint32_t price = 0, uint32_t endTimestamp = 0, const std::vector<std::tuple<uint16_t, uint8_t>>& items = {}, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems = {}, const std::vector<uint16_t>& highlights = {});
void sendClientCheck(const std::vector<uint8_t>& data = {});
void sendRequestBosstiaryInfo();
void sendRequestBossSlootInfo();
void sendRequestBossSlotAction(uint8_t action, uint32_t raceId);
void sendStatusTrackerBestiary(uint16_t raceId, bool status);
void requestQuickLootBlackWhiteList(uint8_t filter, uint16_t size, const std::vector<uint16_t>& listedItems);
void openContainerQuickLoot(uint8_t action, uint8_t category, const Position& pos, uint16_t itemId, uint8_t stackpos, bool useMainAsFallback);
void sendInspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount);
void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId = 0);
void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions);
void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex = 0);

}
