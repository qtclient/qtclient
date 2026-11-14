#include "gamesession_forward.h"

#include "gamehub.h"
#include "gamehub_impl.h"
namespace tibia {
namespace game {

void acceptMarketOffer(uint32_t timestamp, uint16_t counter, uint16_t amount)
{
    qtclientGameHub().acceptMarketOffer(timestamp, counter, amount);
}

void acceptTrade()
{
    qtclientGameHub().acceptTrade();
}

void addVip(std::string_view name)
{
    qtclientGameHub().addVip(name);
}

void answerModalDialog(uint32_t dialog, uint8_t button, uint8_t choice)
{
    qtclientGameHub().answerModalDialog(dialog, button, choice);
}

void applyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm)
{
    qtclientGameHub().applyImbuement(slot, imbuementId, protectionCharm);
}

void attack(uint32_t creatureId)
{
    qtclientGameHub().attack(creatureId);
}

void bountyPreferredAction(uint8_t actionType, uint16_t slot, uint16_t raceId)
{
    qtclientGameHub().bountyPreferredAction(actionType, slot, raceId);
}

void bountyTalismanUpgrade(uint8_t pathIndex)
{
    qtclientGameHub().bountyTalismanUpgrade(pathIndex);
}

void bountyTaskAction(uint8_t actionType, uint16_t value)
{
    qtclientGameHub().bountyTaskAction(actionType, value);
}

void browseField(const Position& position)
{
    qtclientGameHub().browseField(position);
}

void browseMarket(uint8_t browseId, uint16_t browseType, uint8_t tier)
{
    qtclientGameHub().browseMarket(browseId, browseType, tier);
}

void buyItem(uint16_t typeId, uint8_t countOrSubType, uint16_t amount, bool ignoreCapacity, bool buyWithBackpack)
{
    qtclientGameHub().buyItem(typeId, countOrSubType, amount, ignoreCapacity, buyWithBackpack);
}

void buyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location)
{
    qtclientGameHub().buyStoreOffer(offerId, action, name, type, location);
}

bool canPerformGameAction()
{
    return qtclientGameHub().canPerformGameAction();
}

void cancelAttack()
{
    qtclientGameHub().cancelAttack();
}

void cancelFollow()
{
    qtclientGameHub().cancelFollow();
}

void cancelMarketOffer(uint32_t timestamp, uint16_t counter)
{
    qtclientGameHub().cancelMarketOffer(timestamp, counter);
}

void changeOutfitAndMount(const Outfit& outfit, bool mounted, bool randomMount)
{
    qtclientGameHub().changeOutfitAndMount(outfit, mounted, randomMount);
}

void clearImbuement(uint8_t slot)
{
    qtclientGameHub().clearImbuement(slot);
}

void closeById(int containerId)
{
    qtclientGameHub().closeById(containerId);
}

void closeDepotSearchRequest()
{
    qtclientGameHub().closeDepotSearchRequest();
}

void closeImbuingWindow()
{
    qtclientGameHub().closeImbuingWindow();
}

void closeNpcChannel()
{
    qtclientGameHub().closeNpcChannel();
}

void closeNpcTrade()
{
    qtclientGameHub().closeNpcTrade();
}

void configureShowOffSocket(const Position& pos, uint16_t itemId, uint8_t stackpos)
{
    qtclientGameHub().configureShowOffSocket(pos, itemId, stackpos);
}

void confirmCharacterTradeConfiguration(uint32_t price, uint32_t endTimestamp, const std::vector<std::tuple<uint16_t, uint8_t>>& items, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems, const std::vector<uint16_t>& highlights, bool createAuction)
{
    qtclientGameHub().confirmCharacterTradeConfiguration(price, endTimestamp, items, storeItems, highlights, createAuction);
}

void createMarketOffer(uint8_t type, uint16_t itemId, uint8_t itemTier, uint16_t amount, uint64_t price, uint8_t anonymous)
{
    qtclientGameHub().createMarketOffer(type, itemId, itemTier, amount, price, anonymous);
}

void editGuildMessage(std::string_view text)
{
    qtclientGameHub().editGuildMessage(text);
}

void editList(uint32_t id, uint8_t doorId, std::string_view text)
{
    qtclientGameHub().editList(id, doorId, text);
}

void editText(uint32_t id, std::string_view text)
{
    qtclientGameHub().editText(id, text);
}

void editVip(uint32_t playerId, std::string_view description, uint32_t iconId, bool notifyLogin, const std::vector<uint8_t>& groupID)
{
    qtclientGameHub().editVip(playerId, description, iconId, notifyLogin, groupID);
}

void editVipGroups(tibia::client::GroupsEditInfoType_t action, uint8_t groupId, std::string_view groupName)
{
    qtclientGameHub().editVipGroups(action, groupId, groupName);
}

ItemPtr ephemeralItemForWire(uint16_t typeId, const Position& position, int stackPos, uint16_t count, uint8_t tier, uint8_t fluidOrSubtype)
{
    return qtclientGameHub().ephemeralItemForWire(typeId, position, stackPos, count, tier, fluidOrSubtype);
}

void equipItemId(const uint16_t itemId, const uint8_t tier)
{
    qtclientGameHub().equipItemId(itemId, tier);
}

void excludeFromOwnChannel(std::string_view name)
{
    qtclientGameHub().excludeFromOwnChannel(name);
}

void follow(uint32_t creatureId)
{
    qtclientGameHub().follow(creatureId);
}

void forgeRequest(tibia::client::ForgeAction_t actionType, bool convergence, uint16_t firstItemid, uint8_t firstItemTier, uint16_t secondItemId, bool improveChance, bool tierLoss)
{
    qtclientGameHub().forgeRequest(actionType, convergence, firstItemid, firstItemTier, secondItemId, improveChance, tierLoss);
}

void gatherAll(const Position& pos, uint16_t itemId, uint8_t stackpos)
{
    qtclientGameHub().gatherAll(pos, itemId, stackpos);
}

void gemAction(uint8_t actionType, uint16_t param, uint8_t pos)
{
    qtclientGameHub().gemAction(actionType, param, pos);
}

void greetNpc(uint32_t npcId)
{
    qtclientGameHub().greetNpc(npcId);
}

void inspectCharacter(const uint32_t creatureId, const uint8_t tab)
{
    qtclientGameHub().inspectCharacter(creatureId, tab);
}

void inspectNpcTrade(uint16_t typeId, uint8_t count)
{
    qtclientGameHub().inspectNpcTrade(typeId, count);
}

void inspectTrade(bool counterOffer, uint8_t index)
{
    qtclientGameHub().inspectTrade(counterOffer, index);
}

void inspectionNormalObject(const Position& position)
{
    qtclientGameHub().inspectionNormalObject(position);
}

void inspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount)
{
    qtclientGameHub().inspectionObject(inspectionType, itemId, itemCount);
}

void inviteToOwnChannel(std::string_view name)
{
    qtclientGameHub().inviteToOwnChannel(name);
}

bool isFollowing()
{
    return qtclientGameHub().isFollowing();
}

void joinAggression(uint32_t creatureId)
{
    qtclientGameHub().joinAggression(creatureId);
}

void joinChannel(uint16_t channelId)
{
    qtclientGameHub().joinChannel(channelId);
}

void leaveChannel(uint16_t channelId)
{
    qtclientGameHub().leaveChannel(channelId);
}

void leaveMarket()
{
    qtclientGameHub().leaveMarket();
}

void lookCreature(uint32_t creatureId, bool isBattleList)
{
    qtclientGameHub().lookCreature(creatureId, isBattleList);
}

void lookMapObject(const Position& position, uint16_t itemId, int stackPos)
{
    qtclientGameHub().lookMapObject(position, itemId, stackPos);
}

void mount(bool mount)
{
    qtclientGameHub().mount(mount);
}

void moveMapObject(const Position& from, uint16_t itemId, int stackPos, const Position& toPos, int count)
{
    qtclientGameHub().moveMapObject(from, itemId, stackPos, toPos, count);
}

void openDepotSearch()
{
    qtclientGameHub().openDepotSearch();
}

void openOwnChannel()
{
    qtclientGameHub().openOwnChannel();
}

void openParentById(int containerId)
{
    qtclientGameHub().openParentById(containerId);
}

void openParentContainerFromDepotSearch(const Position& pos)
{
    qtclientGameHub().openParentContainerFromDepotSearch(pos);
}

void openStore(uint8_t serviceType, std::string_view category)
{
    qtclientGameHub().openStore(serviceType, category);
}

void openTransactionHistory(uint8_t entriesPerPage)
{
    qtclientGameHub().openTransactionHistory(entriesPerPage);
}

void openWheel(uint32_t playerId)
{
    qtclientGameHub().openWheel(playerId);
}

void partyDisband()
{
    qtclientGameHub().partyDisband();
}

void partyInvite(uint32_t creatureId)
{
    qtclientGameHub().partyInvite(creatureId);
}

void partyLeave()
{
    qtclientGameHub().partyLeave();
}

void partyPassLeadership(uint32_t creatureId)
{
    qtclientGameHub().partyPassLeadership(creatureId);
}

void partyRevokeInvitation(uint32_t creatureId)
{
    qtclientGameHub().partyRevokeInvitation(creatureId);
}

void partyShareExperience(bool active)
{
    qtclientGameHub().partyShareExperience(active);
}

void preyAction(uint8_t slot, uint8_t actionType, uint16_t index)
{
    qtclientGameHub().preyAction(slot, actionType, index);
}

void processInventoryChange(uint8_t slot, const ::tibia::container::TInventorySlot& item)
{
    qtclientGameHub().processInventoryChange(slot, item);
}

void rejectTrade()
{
    qtclientGameHub().rejectTrade();
}

void removeVip(uint32_t playerId)
{
    qtclientGameHub().removeVip(playerId);
}

void reportBugCip(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position)
{
    qtclientGameHub().reportBugCip(category, comment, typoSpeaker, typoText, position);
}

void reportRuleViolationCip(uint8_t type, uint8_t reason, std::string_view name, std::string_view comment, std::string_view translation, uint32_t id)
{
    qtclientGameHub().reportRuleViolationCip(type, reason, name, comment, translation, id);
}

void requestBestiary()
{
    qtclientGameHub().requestBestiary();
}

void requestBestiaryOverview(std::string_view catName, bool search, std::vector<uint16_t> raceIds)
{
    qtclientGameHub().requestBestiaryOverview(catName, search, raceIds);
}

void requestBossSlootInfo()
{
    qtclientGameHub().requestBossSlootInfo();
}

void requestBossSlotAction(uint8_t action, uint32_t raceId)
{
    qtclientGameHub().requestBossSlotAction(action, raceId);
}

void requestBosstiaryInfo()
{
    qtclientGameHub().requestBosstiaryInfo();
}

void requestChannels()
{
    qtclientGameHub().requestChannels();
}

void requestCharacterTradeConfiguration(uint8_t action)
{
    qtclientGameHub().requestCharacterTradeConfiguration(action);
}

void requestDepotSearchItem(uint16_t itemId, uint8_t tier)
{
    qtclientGameHub().requestDepotSearchItem(itemId, tier);
}

void requestGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items)
{
    qtclientGameHub().requestGetRewardDaily(bonusShrine, items);
}

void requestGuildMessage()
{
    qtclientGameHub().requestGuildMessage();
}

void requestHighscore(uint8_t action, uint8_t category, uint32_t vocation, std::string_view world, uint8_t worldType, uint8_t battlEye, uint16_t page, uint8_t totalPages)
{
    qtclientGameHub().requestHighscore(action, category, vocation, world, worldType, battlEye, page, totalPages);
}

void requestItemInfo(uint16_t typeId, uint8_t subType, uint8_t index)
{
    qtclientGameHub().requestItemInfo(typeId, subType, index);
}

void requestMarketStatistics(uint16_t itemId, uint8_t tier)
{
    qtclientGameHub().requestMarketStatistics(itemId, tier);
}

void requestOpenRewardHistory()
{
    qtclientGameHub().requestOpenRewardHistory();
}

void requestOutfit()
{
    qtclientGameHub().requestOutfit();
}

void requestQuestLine(uint16_t questId)
{
    qtclientGameHub().requestQuestLine(questId);
}

void requestQuestLog()
{
    qtclientGameHub().requestQuestLog();
}

void requestResourceBalance(uint8_t resourceType)
{
    qtclientGameHub().requestResourceBalance(resourceType);
}

void requestSendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId)
{
    qtclientGameHub().requestSendBuyCharmRune(runeId, action, raceId);
}

void requestSendCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage, uint16_t page)
{
    qtclientGameHub().requestSendCharacterInfo(playerId, characterInfoType, entriesPerPage, page);
}

void requestSendCyclopediaHouseAuction(tibia::client::CyclopediaHouseAuctionType_t type, uint32_t houseId, uint32_t timestamp, uint64_t bidValue, std::string_view name)
{
    qtclientGameHub().requestSendCyclopediaHouseAuction(type, houseId, timestamp, bidValue, name);
}

void requestSetCharacterTitle(uint8_t titleId)
{
    qtclientGameHub().requestSetCharacterTitle(titleId);
}

void requestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder, const uint8_t serviceType)
{
    qtclientGameHub().requestStoreOffers(categoryName, subCategory, sortOrder, serviceType);
}

void requestTextForReport(const Position& position, uint16_t typeId, uint8_t stackPosition)
{
    qtclientGameHub().requestTextForReport(position, typeId, stackPosition);
}

void requestTradeAt(const Position& position, uint16_t itemId, int stackPos, uint32_t creatureId)
{
    qtclientGameHub().requestTradeAt(position, itemId, stackPos, creatureId);
}

void requestTransactionDetails(uint32_t transactionId)
{
    qtclientGameHub().requestTransactionDetails(transactionId);
}

void requestTransactionHistory(uint32_t page, uint32_t entriesPerPage)
{
    qtclientGameHub().requestTransactionHistory(page, entriesPerPage);
}

void retrieveDepotSearch(uint16_t itemId, uint8_t tier, uint8_t type)
{
    qtclientGameHub().retrieveDepotSearch(itemId, tier, type);
}

void rotateMapObject(const Position& position, uint16_t itemId, int stackPos)
{
    qtclientGameHub().rotateMapObject(position, itemId, stackPos);
}

void seekInContainer(uint8_t containerId, uint16_t index)
{
    qtclientGameHub().seekInContainer(containerId, index);
}

void selectImbuementItem(uint16_t itemId, const Position& pos, uint8_t stackpos)
{
    qtclientGameHub().selectImbuementItem(itemId, pos, stackpos);
}

void selectImbuementScroll()
{
    qtclientGameHub().selectImbuementScroll();
}

void sellItem(uint16_t typeId, uint8_t subType, uint16_t amount, bool ignoreEquipped)
{
    qtclientGameHub().sellItem(typeId, subType, amount, ignoreEquipped);
}

void sendApplyWheelPoints(const std::vector<uint16_t>& slotPoints, const std::array<uint16_t, 8>& vesselGems)
{
    qtclientGameHub().sendApplyWheelPoints(slotPoints, vesselGems);
}

void sendBossDifficultyAction(uint8_t action, uint16_t difficulty)
{
    qtclientGameHub().sendBossDifficultyAction(action, difficulty);
}

void sendContainerAction(uint8_t action, uint8_t containerId)
{
    qtclientGameHub().sendContainerAction(action, containerId);
}

void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount)
{
    qtclientGameHub().sendCyclopediaMapDonate(areaId, amount);
}

void sendCyclopediaMapResetViewpoints(uint16_t subAreaId)
{
    qtclientGameHub().sendCyclopediaMapResetViewpoints(subAreaId);
}

void sendExivaOptions(bool allowAll, bool allowOwnGuild, bool allowOwnParty, bool allowVipList, bool allowPlayerWhitelist, bool allowGuildWhitelist, const std::vector<std::string>& characterWhiteList, const std::vector<std::string>& removeCharacter, const std::vector<std::string>& guildWhiteList, const std::vector<std::string>& removeGuild)
{
    qtclientGameHub().sendExivaOptions(allowAll, allowOwnGuild, allowOwnParty, allowVipList, allowPlayerWhitelist, allowGuildWhitelist, characterWhiteList, removeCharacter, guildWhiteList, removeGuild);
}

void sendForgeBrowseHistoryRequest(uint16_t page)
{
    qtclientGameHub().sendForgeBrowseHistoryRequest(page);
}

void sendOpenRewardWall()
{
    qtclientGameHub().sendOpenRewardWall();
}

void sendPartyAnalyzerPriceType()
{
    qtclientGameHub().sendPartyAnalyzerPriceType();
}

void sendPartyAnalyzerReset()
{
    qtclientGameHub().sendPartyAnalyzerReset();
}

void sendQuickLootAt(uint8_t variant, const Position& position, uint16_t itemId, int stackPos)
{
    qtclientGameHub().sendQuickLootAt(variant, position, itemId, stackPos);
}

void sendRequestStoreHome()
{
    qtclientGameHub().sendRequestStoreHome();
}

void sendRequestStoreOfferById(const uint32_t offerId, const uint8_t sortOrder, const uint8_t serviceType)
{
    qtclientGameHub().sendRequestStoreOfferById(offerId, sortOrder, serviceType);
}

void sendRequestStorePremiumBoost()
{
    qtclientGameHub().sendRequestStorePremiumBoost();
}

void sendRequestStoreSearch(const std::string_view searchText, const uint8_t sortOrder, const uint8_t serviceType)
{
    qtclientGameHub().sendRequestStoreSearch(searchText, sortOrder, serviceType);
}

void sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, bool autoTrackNewQuests, bool autoUntrackCompletedQuests, uint8_t extra)
{
    qtclientGameHub().sendRequestTrackerQuestLog(missionIds, autoTrackNewQuests, autoUntrackCompletedQuests, extra);
}

void sendRequestUsefulThings(const uint8_t serviceType)
{
    qtclientGameHub().sendRequestUsefulThings(serviceType);
}

void sendSelectSpellAim(const std::vector<uint16_t>& spellIds, bool enabled)
{
    qtclientGameHub().sendSelectSpellAim(spellIds, enabled);
}

void sendSetHirelingName(std::string_view name)
{
    qtclientGameHub().sendSetHirelingName(name);
}

void sendStartOfflineTraining(const uint8_t skillType)
{
    qtclientGameHub().sendStartOfflineTraining(skillType);
}

void sendTeleport(const Position& pos)
{
    qtclientGameHub().sendTeleport(pos);
}

void sendTutorialChangeVocation(uint8_t vocationClientId)
{
    qtclientGameHub().sendTutorialChangeVocation(vocationClientId);
}

void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId)
{
    qtclientGameHub().sendWeaponProficiencyAction(actionType, itemId);
}

void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions)
{
    qtclientGameHub().sendWeaponProficiencyApply(itemId, levels, perkPositions);
}

void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex)
{
    qtclientGameHub().sendWeaponProficiencySlotAction(actionType, itemId, level, perkPosition, offerIndex);
}

void setClientOption(uint8_t option, bool value)
{
    qtclientGameHub().setClientOption(option, value);
}

void setCreaturePodiumConfiguration(uint32_t monsterRaceId, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible, uint8_t monsterVisible)
{
    qtclientGameHub().setCreaturePodiumConfiguration(monsterRaceId, pos, itemId, stackpos, direction, podiumVisible, monsterVisible);
}

void setHirelingOutfit(const Outfit& outfit, uint32_t hirelingCreatureId)
{
    qtclientGameHub().setHirelingOutfit(outfit, hirelingCreatureId);
}

void setShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible)
{
    qtclientGameHub().setShowOffSocketConfiguration(outfit, pos, itemId, stackpos, direction, podiumVisible);
}

void setTactics(tibia::client::ChaseModes chaseMode, bool safeFight, tibia::client::PVPModes pvpMode)
{
    qtclientGameHub().setTactics(chaseMode, safeFight, pvpMode);
}

tibia::client::ChaseModes getChaseMode()
{
    return qtclientGameHub().getChaseMode();
}

bool isSafeFight()
{
    return qtclientGameHub().isSafeFight();
}

tibia::client::PVPModes getPVPMode()
{
    return qtclientGameHub().getPVPMode();
}

void soulsealFightAction(uint16_t raceId)
{
    qtclientGameHub().soulsealFightAction(raceId);
}

void stashStowItem(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action)
{
    qtclientGameHub().stashStowItem(position, itemId, count, stackpos, action);
}

void stashWithdraw(uint16_t itemId, uint32_t count, uint8_t stackpos)
{
    qtclientGameHub().stashWithdraw(itemId, count, stackpos);
}

void stop()
{
    qtclientGameHub().stop();
}

void talkChannel(tibia::client::MessageMode mode, uint16_t channelId, std::string_view message)
{
    qtclientGameHub().talkChannel(mode, channelId, message);
}

void talkPrivate(tibia::client::MessageMode mode, std::string_view receiver, std::string_view message)
{
    qtclientGameHub().talkPrivate(mode, receiver, message);
}

void taskHuntingShopPurchase(uint8_t offerIndex)
{
    qtclientGameHub().taskHuntingShopPurchase(offerIndex);
}

void taskHuntingShopRequest()
{
    qtclientGameHub().taskHuntingShopRequest();
}

void transferCoins(std::string_view recipient, uint32_t amount)
{
    qtclientGameHub().transferCoins(recipient, amount);
}

void useInventoryItem(uint16_t itemId)
{
    qtclientGameHub().useInventoryItem(itemId);
}

void useMapObjectOnCreature(const Position& from, uint16_t fromId, int fromStack, uint32_t creatureId)
{
    qtclientGameHub().useMapObjectOnCreature(from, fromId, fromStack, creatureId);
}

void useWithMapObject(const Position& from, uint16_t fromId, int fromStack, const Position& to, uint16_t toId, int toStack)
{
    qtclientGameHub().useWithMapObject(from, fromId, fromStack, to, toId, toStack);
}

void weeklyTaskAction(uint8_t actionType, uint16_t value)
{
    qtclientGameHub().weeklyTaskAction(actionType, value);
}

void wrapMapObject(const Position& position, uint16_t itemId, int stackPos)
{
    qtclientGameHub().wrapMapObject(position, itemId, stackPos);
}

int getProtocolVersion()
{
    return qtclientGameHub().getProtocolVersion();
}

bool isUsingProtobuf()
{
    return qtclientGameHub().isUsingProtobuf();
}

int getPing()
{
    return qtclientGameHub().getPing();
}

stdext::map<int, Vip> getVips()
{
    return qtclientGameHub().getVips();
}

uint32_t getFollowingCreatureId()
{
    return qtclientGameHub().getFollowingCreatureId();
}

void setCanReportBugs(const bool enable)
{
    qtclientGameHub().setCanReportBugs(enable);
}

bool canExivaOptions()
{
    return qtclientGameHub().canExivaOptions();
}

void setCanExivaOptions(const bool enable)
{
    qtclientGameHub().setCanExivaOptions(enable);
}

std::string getWorldName()
{
    return qtclientGameHub().getWorldName();
}

uint8_t getWalkMaxSteps()
{
    return qtclientGameHub().getWalkMaxSteps();
}

void setWalkMaxSteps(const uint8_t v)
{
    qtclientGameHub().setWalkMaxSteps(v);
}

int getBugReportState()
{
    return qtclientGameHub().getBugReportState();
}

void setBugReportState(const int state)
{
    qtclientGameHub().setBugReportState(state);
}

UnjustifiedPoints getUnjustifiedPoints()
{
    return qtclientGameHub().getUnjustifiedPoints();
}

void setUnjustifiedPoints(const UnjustifiedPoints unjustifiedPoints)
{
    qtclientGameHub().setUnjustifiedPoints(unjustifiedPoints);
}

uint32_t getOpenPvpSituations()
{
    return qtclientGameHub().getOpenPvpSituations();
}

void setOpenPvpSituations(const uint8_t openPvpSituations)
{
    qtclientGameHub().setOpenPvpSituations(openPvpSituations);
}

ProtocolGamePtr getProtocolGame()
{
    return qtclientGameHub().getProtocolGame();
}

uint16_t getServerBeat()
{
    return qtclientGameHub().getServerBeat();
}

void setServerBeat(const uint16_t beat)
{
    qtclientGameHub().setServerBeat(beat);
}

void setExpertPvpMode(const bool enable)
{
    qtclientGameHub().setExpertPvpMode(enable);
}

bool getExpertPvpMode()
{
    return qtclientGameHub().getExpertPvpMode();
}

void setNextTalkAim(const Position& pos)
{
    qtclientGameHub().setNextTalkAim(pos);
}

void setNextTalkAimMode(const uint8_t mode)
{
    qtclientGameHub().setNextTalkAimMode(mode);
}

Position takeNextTalkAim()
{
    return qtclientGameHub().takeNextTalkAim();
}

uint8_t takeNextTalkAimMode()
{
    return qtclientGameHub().takeNextTalkAimMode();
}

void resetMapUpdatedAt()
{
    qtclientGameHub().resetMapUpdatedAt();
}

void updateMapLatency()
{
    qtclientGameHub().updateMapLatency();
}

void requestStop()
{
    qtclientGameHub().requestStop();
}


} // namespace game
} // namespace tibia
