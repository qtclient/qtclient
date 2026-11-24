#pragma once

/// Auto-forward remaining Game hub methods for CipSoft controllers/storages.
/// Packs must not include this — use include/qtclient/session.h only.

#include <array>
#include <client/declarations.h>
#include <client/staticdata.h>
#include <client/const.h>
#include <container/containerciptypes.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <memory>

namespace tibia::input {
class IGameActionContext;
}

namespace tibia {
namespace game {

void acceptTrade();

void applyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm);

void attack(uint32_t creatureId);

bool canPerformGameAction();

void clearImbuement(uint8_t slot);

void closeImbuingWindow();

void confirmCharacterTradeConfiguration(uint32_t price, uint32_t endTimestamp, const std::vector<std::tuple<uint16_t, uint8_t>>& items, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems, const std::vector<uint16_t>& highlights, bool createAuction);

void editGuildMessage(std::string_view text);

void editText(uint32_t id, std::string_view text);

void excludeFromOwnChannel(std::string_view name);

void follow(uint32_t creatureId);

void inspectTrade(bool counterOffer, uint8_t index);

void inspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount);

void inviteToOwnChannel(std::string_view name);

void openStore(uint8_t serviceType = 0, std::string_view category = "");

void openTransactionHistory(uint8_t entriesPerPage);


void preyAction(uint8_t slot, uint8_t actionType, uint16_t index);

void processInventoryChange(uint8_t slot, const ::tibia::container::TInventorySlot& item);

void rejectTrade();

void reportBugCip(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position);

void requestBestiary();

void requestBestiaryOverview(std::string_view catName, bool search = false, std::vector<uint16_t> raceIds = {});

void requestBossSlootInfo();

void requestBossSlotAction(uint8_t action, uint32_t raceId);

void requestBosstiaryInfo();

void requestCharacterTradeConfiguration(uint8_t action);


void requestQuestLine(uint16_t questId);

void requestQuestLog();

void requestSendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId);

void requestSendCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage = 0, uint16_t page = 0);

void requestSetCharacterTitle(uint8_t titleId);

void requestTransactionDetails(uint32_t transactionId);

void selectImbuementItem(uint16_t itemId, const Position& pos, uint8_t stackpos);

void selectImbuementScroll();


void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount);

void sendCyclopediaMapResetViewpoints(uint16_t subAreaId);

/// Asks the server for what the Cyclopedia map shows of a subarea (its viewpoints).
void sendCyclopediaMapRequestArea(uint16_t subAreaId);

void sendOpenRewardWall();

/// The party hunt loot type: 0 market prices, 1 the leader's prices.
void sendPartyAnalyzerPriceType(int lootType);

void sendPartyAnalyzerReset();

void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId = 0);

void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions);

void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex = 0);

void setCreaturePodiumConfiguration(uint32_t monsterRaceId, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible, uint8_t monsterVisible);

void setHirelingOutfit(const Outfit& outfit, uint32_t hirelingCreatureId);

void setShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible);

void soulsealFightAction(uint16_t raceId);

void talkChannel(tibia::client::MessageMode mode, uint16_t channelId, std::string_view message);

int getPing();

uint32_t getFollowingCreatureId();

void setCanReportBugs(const bool enable);

bool canExivaOptions();

void setCanExivaOptions(const bool enable);

std::string getWorldName();

int getBugReportState();

void setBugReportState(const int state);

void setExpertPvpMode(const bool enable);

bool getExpertPvpMode();

void setNextTalkAim(const Position& pos);

void setNextTalkAimMode(const uint8_t mode);

Position takeNextTalkAim();

uint8_t takeNextTalkAimMode();

void updateMapLatency();

} // namespace game
} // namespace tibia
