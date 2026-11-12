#pragma once

/// Auto-forward remaining Game hub methods for CipSoft controllers/storages.
/// Packs must not include this — use include/qtclient/session.h only.

#include <array>
#include <client/declarations.h>
#include <client/staticdata.h>
#include <client/const.h>
#include <container/containerciptypes.h>
#include <framework/net/declarations.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tibia {
namespace game {

void acceptMarketOffer(uint32_t timestamp, uint16_t counter, uint16_t amount);

void acceptTrade();

void addVip(std::string_view name);

void answerModalDialog(uint32_t dialog, uint8_t button, uint8_t choice);

void applyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm);

void attack(uint32_t creatureId);

void bountyPreferredAction(uint8_t actionType, uint16_t slot, uint16_t raceId);

void bountyTalismanUpgrade(uint8_t pathIndex);

void bountyTaskAction(uint8_t actionType, uint16_t value = 0);

void browseField(const Position& position);

void browseMarket(uint8_t browseId, uint16_t browseType, uint8_t tier = 0);

void buyItem(uint16_t typeId, uint8_t countOrSubType, uint16_t amount, bool ignoreCapacity, bool buyWithBackpack);

void buyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location);

bool canPerformGameAction();

void cancelAttack();

void cancelFollow();

void cancelMarketOffer(uint32_t timestamp, uint16_t counter);

void changeOutfitAndMount(const Outfit& outfit, bool mounted, bool randomMount);

void clearImbuement(uint8_t slot);

void closeById(int containerId);

void closeDepotSearchRequest();

void closeImbuingWindow();

void closeNpcChannel();

void closeNpcTrade();

void configureShowOffSocket(const Position& pos, uint16_t itemId, uint8_t stackpos);

void confirmCharacterTradeConfiguration(uint32_t price, uint32_t endTimestamp, const std::vector<std::tuple<uint16_t, uint8_t>>& items, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems, const std::vector<uint16_t>& highlights, bool createAuction);

void createMarketOffer(uint8_t type, uint16_t itemId, uint8_t itemTier, uint16_t amount, uint64_t price, uint8_t anonymous);

void editGuildMessage(std::string_view text);

void editList(uint32_t id, uint8_t doorId, std::string_view text);

void editText(uint32_t id, std::string_view text);

void editVip(uint32_t playerId, std::string_view description, uint32_t iconId, bool notifyLogin, const std::vector<uint8_t>& groupID = {});

void editVipGroups(tibia::client::GroupsEditInfoType_t action, uint8_t groupId, std::string_view groupName);

ItemPtr ephemeralItemForWire(uint16_t typeId, const Position& position, int stackPos, uint16_t count = 0, uint8_t tier = 0, uint8_t fluidOrSubtype = 0);

void equipItemId(const uint16_t itemId, const uint8_t tier);

void excludeFromOwnChannel(std::string_view name);

void follow(uint32_t creatureId);

void forgeRequest(tibia::client::ForgeAction_t actionType, bool convergence = false, uint16_t firstItemid = 0, uint8_t firstItemTier = 0, uint16_t secondItemId = 0, bool improveChance = false, bool tierLoss = false);

void gatherAll(const Position& pos, uint16_t itemId, uint8_t stackpos);

void gemAction(uint8_t actionType, uint16_t param, uint8_t pos);

void greetNpc(uint32_t npcId);

void inspectCharacter(const uint32_t creatureId, const uint8_t tab);

void inspectNpcTrade(uint16_t typeId, uint8_t count);

void inspectTrade(bool counterOffer, uint8_t index);

void inspectionNormalObject(const Position& position);

void inspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount);

void inviteToOwnChannel(std::string_view name);

bool isFollowing();

void joinAggression(uint32_t creatureId);

void joinChannel(uint16_t channelId);

void leaveChannel(uint16_t channelId);

void leaveMarket();

void lookCreature(uint32_t creatureId, bool isBattleList = false);

void lookMapObject(const Position& position, uint16_t itemId, int stackPos);

void mount(bool mount);

void moveMapObject(const Position& from, uint16_t itemId, int stackPos, const Position& toPos, int count);

void openDepotSearch();

void openOwnChannel();

void openParentById(int containerId);

void openParentContainerFromDepotSearch(const Position& pos);

void openStore(uint8_t serviceType = 0, std::string_view category = "");

void openTransactionHistory(uint8_t entriesPerPage);

void openWheel(uint32_t playerId);

void partyDisband();

void partyInvite(uint32_t creatureId);

void partyLeave();

void partyPassLeadership(uint32_t creatureId);

void partyRevokeInvitation(uint32_t creatureId);

void partyShareExperience(bool active);

void preyAction(uint8_t slot, uint8_t actionType, uint16_t index);

void processInventoryChange(uint8_t slot, const ::tibia::container::TInventorySlot& item);

void rejectTrade();

void removeVip(uint32_t playerId);

void reportBugCip(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position);

void reportRuleViolationCip(uint8_t type, uint8_t reason, std::string_view name, std::string_view comment, std::string_view translation, uint32_t id);

void requestBestiary();

void requestBestiaryOverview(std::string_view catName, bool search = false, std::vector<uint16_t> raceIds = {});

void requestBossSlootInfo();

void requestBossSlotAction(uint8_t action, uint32_t raceId);

void requestBosstiaryInfo();

void requestChannels();

void requestCharacterTradeConfiguration(uint8_t action);

void requestDepotSearchItem(uint16_t itemId, uint8_t tier = 0);

void requestGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items);

void requestGuildMessage();

void requestHighscore(uint8_t action, uint8_t category, uint32_t vocation, std::string_view world, uint8_t worldType, uint8_t battlEye, uint16_t page, uint8_t totalPages);

void requestItemInfo(uint16_t typeId, uint8_t subType, uint8_t index);

void requestMarketStatistics(uint16_t itemId = 0, uint8_t tier = 0);

void requestOpenRewardHistory();

void requestOutfit();

void requestQuestLine(uint16_t questId);

void requestQuestLog();

void requestResourceBalance(uint8_t resourceType);

void requestSendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId);

void requestSendCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage = 0, uint16_t page = 0);

void requestSendCyclopediaHouseAuction(tibia::client::CyclopediaHouseAuctionType_t type, uint32_t houseId, uint32_t timestamp = 0, uint64_t bidValue = 0, std::string_view name = "");

void requestSetCharacterTitle(uint8_t titleId);

void requestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder, const uint8_t serviceType);

void requestTextForReport(const Position& position, uint16_t typeId, uint8_t stackPosition);

void requestTradeAt(const Position& position, uint16_t itemId, int stackPos, uint32_t creatureId);

void requestTransactionDetails(uint32_t transactionId);

void requestTransactionHistory(uint32_t page, uint32_t entriesPerPage);

void retrieveDepotSearch(uint16_t itemId, uint8_t tier, uint8_t type);

void rotateMapObject(const Position& position, uint16_t itemId, int stackPos);

void seekInContainer(uint8_t containerId, uint16_t index);

void selectImbuementItem(uint16_t itemId, const Position& pos, uint8_t stackpos);

void selectImbuementScroll();

void sellItem(uint16_t typeId, uint8_t subType, uint16_t amount, bool ignoreEquipped);

void sendApplyWheelPoints(const std::vector<uint16_t>& slotPoints, const std::array<uint16_t, 8>& vesselGems);

void sendBossDifficultyAction(uint8_t action, uint16_t difficulty = 0);

void sendContainerAction(uint8_t action, uint8_t containerId = 0);

void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount);

void sendCyclopediaMapResetViewpoints(uint16_t subAreaId);

void sendExivaOptions(bool allowAll, bool allowOwnGuild, bool allowOwnParty, bool allowVipList, bool allowPlayerWhitelist, bool allowGuildWhitelist, const std::vector<std::string>& characterWhiteList, const std::vector<std::string>& removeCharacter, const std::vector<std::string>& guildWhiteList, const std::vector<std::string>& removeGuild);

void sendForgeBrowseHistoryRequest(uint16_t page);

void sendOpenRewardWall();

void sendPartyAnalyzerPriceType();

void sendPartyAnalyzerReset();

void sendQuickLootAt(uint8_t variant, const Position& position, uint16_t itemId, int stackPos);

void sendRequestStoreHome();

void sendRequestStoreOfferById(const uint32_t offerId, const uint8_t sortOrder, const uint8_t serviceType);

void sendRequestStorePremiumBoost();

void sendRequestStoreSearch(const std::string_view searchText, const uint8_t sortOrder, const uint8_t serviceType);

void sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, bool autoTrackNewQuests, bool autoUntrackCompletedQuests, uint8_t extra = 0x2A);

void sendRequestUsefulThings(const uint8_t serviceType);

void sendSelectSpellAim(const std::vector<uint16_t>& spellIds, bool enabled);

void sendSetHirelingName(std::string_view name);

void sendStartOfflineTraining(const uint8_t skillType);

void sendTeleport(const Position& pos);

void sendTyping(const bool typing);

void sendTutorialChangeVocation(uint8_t vocationClientId);

void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId = 0);

void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions);

void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex = 0);

void setClientOption(uint8_t option, bool value);

void setCreaturePodiumConfiguration(uint32_t monsterRaceId, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible, uint8_t monsterVisible);

void setHirelingOutfit(const Outfit& outfit, uint32_t hirelingCreatureId);

void setShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible);

void setTactics(tibia::client::ChaseModes chaseMode, bool safeFight, tibia::client::PVPModes pvpMode);

tibia::client::ChaseModes getChaseMode();

bool isSafeFight();

tibia::client::PVPModes getPVPMode();

void soulsealFightAction(uint16_t raceId);

void stashStowItem(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action);

void stashWithdraw(uint16_t itemId, uint32_t count, uint8_t stackpos);

void stop();

void talkChannel(tibia::client::MessageMode mode, uint16_t channelId, std::string_view message);

void talkPrivate(tibia::client::MessageMode mode, std::string_view receiver, std::string_view message);

void taskHuntingShopPurchase(uint8_t offerIndex);

void taskHuntingShopRequest();

void transferCoins(std::string_view recipient, uint32_t amount);

void useInventoryItem(uint16_t itemId);

void useMapObjectOnCreature(const Position& from, uint16_t fromId, int fromStack, uint32_t creatureId);

void useWithMapObject(const Position& from, uint16_t fromId, int fromStack, const Position& to, uint16_t toId, int toStack);

void weeklyTaskAction(uint8_t actionType, uint16_t value = 0);

void wrapMapObject(const Position& position, uint16_t itemId, int stackPos);

int getProtocolVersion();

bool isUsingProtobuf();

int getPing();

stdext::map<int, Vip> getVips();

uint32_t getFollowingCreatureId();

void setCanReportBugs(const bool enable);

bool canExivaOptions();

void setCanExivaOptions(const bool enable);

std::string getWorldName();

uint8_t getWalkMaxSteps();

void setWalkMaxSteps(const uint8_t v);

int getBugReportState();

void setBugReportState(const int state);

UnjustifiedPoints getUnjustifiedPoints();

void setUnjustifiedPoints(const UnjustifiedPoints unjustifiedPoints);

uint32_t getOpenPvpSituations();

void setOpenPvpSituations(const uint8_t openPvpSituations);

ProtocolGamePtr getProtocolGame();

uint16_t getServerBeat();

void setServerBeat(const uint16_t beat);

void setExpertPvpMode(const bool enable);

bool getExpertPvpMode();

void setNextTalkAim(const Position& pos);

void setNextTalkAimMode(const uint8_t mode);

Position takeNextTalkAim();

uint8_t takeNextTalkAimMode();

void resetMapUpdatedAt();

void updateMapLatency();

void requestStop();

void wireAutoWalk(const std::vector<tibia::client::Direction>& dirs, const Position& startPos);

} // namespace game
} // namespace tibia
