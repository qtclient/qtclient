#pragma once

/// Auto-forward remaining Game hub methods for CipSoft controllers/storages.
/// Packs must not include this — use include/qtclient/session.h only.

#include <tibia/game/game.h>

// Classic braces — same MSVC C3083 rationale as gamesession.h (pack TUs).
namespace tibia {
namespace game {

inline void acceptMarketOffer(uint32_t timestamp, uint16_t counter, uint16_t amount)
{ g_game.acceptMarketOffer(timestamp, counter, amount); }

inline void acceptTrade()
{ g_game.acceptTrade(); }

inline void addVip(std::string_view name)
{ g_game.addVip(name); }

inline void answerModalDialog(uint32_t dialog, uint8_t button, uint8_t choice)
{ g_game.answerModalDialog(dialog, button, choice); }

inline void applyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm)
{ g_game.applyImbuement(slot, imbuementId, protectionCharm); }

inline void attack(uint32_t creatureId)
{ g_game.attack(creatureId); }

inline void bountyPreferredAction(uint8_t actionType, uint16_t slot, uint16_t raceId)
{ g_game.bountyPreferredAction(actionType, slot, raceId); }

inline void bountyTalismanUpgrade(uint8_t pathIndex)
{ g_game.bountyTalismanUpgrade(pathIndex); }

inline void bountyTaskAction(uint8_t actionType, uint16_t value = 0)
{ g_game.bountyTaskAction(actionType, value); }

inline void browseField(const Position& position)
{ g_game.browseField(position); }

inline void browseMarket(uint8_t browseId, uint16_t browseType, uint8_t tier = 0)
{ g_game.browseMarket(browseId, browseType, tier); }

inline void buyItem(uint16_t typeId, uint8_t countOrSubType, uint16_t amount, bool ignoreCapacity, bool buyWithBackpack)
{ g_game.buyItem(typeId, countOrSubType, amount, ignoreCapacity, buyWithBackpack); }

inline void buyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location)
{ g_game.buyStoreOffer(offerId, action, name, type, location); }

inline bool canPerformGameAction()
{ return g_game.canPerformGameAction(); }

inline void cancelAttack()
{ g_game.cancelAttack(); }

inline void cancelFollow()
{ g_game.cancelFollow(); }

inline void cancelMarketOffer(uint32_t timestamp, uint16_t counter)
{ g_game.cancelMarketOffer(timestamp, counter); }

inline void changeOutfitAndMount(const Outfit& outfit, bool mounted, bool randomMount)
{ g_game.changeOutfitAndMount(outfit, mounted, randomMount); }

inline void clearImbuement(uint8_t slot)
{ g_game.clearImbuement(slot); }

inline void closeById(int containerId)
{ g_game.closeById(containerId); }

inline void closeDepotSearchRequest()
{ g_game.closeDepotSearchRequest(); }

inline void closeImbuingWindow()
{ g_game.closeImbuingWindow(); }

inline void closeNpcChannel()
{ g_game.closeNpcChannel(); }

inline void closeNpcTrade()
{ g_game.closeNpcTrade(); }

inline void configureShowOffSocket(const Position& pos, uint16_t itemId, uint8_t stackpos)
{ g_game.configureShowOffSocket(pos, itemId, stackpos); }

inline void confirmCharacterTradeConfiguration(uint32_t price, uint32_t endTimestamp, const std::vector<std::tuple<uint16_t, uint8_t>>& items, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems, const std::vector<uint16_t>& highlights, bool createAuction)
{ g_game.confirmCharacterTradeConfiguration(price, endTimestamp, items, storeItems, highlights, createAuction); }

inline void createMarketOffer(uint8_t type, uint16_t itemId, uint8_t itemTier, uint16_t amount, uint64_t price, uint8_t anonymous)
{ g_game.createMarketOffer(type, itemId, itemTier, amount, price, anonymous); }

inline void editGuildMessage(std::string_view text)
{ g_game.editGuildMessage(text); }

inline void editList(uint32_t id, uint8_t doorId, std::string_view text)
{ g_game.editList(id, doorId, text); }

inline void editText(uint32_t id, std::string_view text)
{ g_game.editText(id, text); }

inline void editVip(uint32_t playerId, std::string_view description, uint32_t iconId, bool notifyLogin, const std::vector<uint8_t>& groupID = {})
{ g_game.editVip(playerId, description, iconId, notifyLogin, groupID); }

inline void editVipGroups(tibia::client::GroupsEditInfoType_t action, uint8_t groupId, std::string_view groupName)
{ g_game.editVipGroups(action, groupId, groupName); }

inline ItemPtr ephemeralItemForWire(uint16_t typeId, const Position& position, int stackPos, uint16_t count = 0, uint8_t tier = 0, uint8_t fluidOrSubtype = 0)
{ return g_game.ephemeralItemForWire(typeId, position, stackPos, count, tier, fluidOrSubtype); }

inline void equipItemId(const uint16_t itemId, const uint8_t tier)
{ g_game.equipItemId(itemId, tier); }

inline void excludeFromOwnChannel(std::string_view name)
{ g_game.excludeFromOwnChannel(name); }

inline void follow(uint32_t creatureId)
{ g_game.follow(creatureId); }

inline void forgeRequest(tibia::client::ForgeAction_t actionType, bool convergence = false, uint16_t firstItemid = 0, uint8_t firstItemTier = 0, uint16_t secondItemId = 0, bool improveChance = false, bool tierLoss = false)
{ g_game.forgeRequest(actionType, convergence, firstItemid, firstItemTier, secondItemId, improveChance, tierLoss); }

inline void gatherAll(const Position& pos, uint16_t itemId, uint8_t stackpos)
{ g_game.gatherAll(pos, itemId, stackpos); }

inline void gemAction(uint8_t actionType, uint16_t param, uint8_t pos)
{ g_game.gemAction(actionType, param, pos); }

inline void greetNpc(uint32_t npcId)
{ g_game.greetNpc(npcId); }

inline void inspectCharacter(const uint32_t creatureId, const uint8_t tab)
{ g_game.inspectCharacter(creatureId, tab); }

inline void inspectNpcTrade(uint16_t typeId, uint8_t count)
{ g_game.inspectNpcTrade(typeId, count); }

inline void inspectTrade(bool counterOffer, uint8_t index)
{ g_game.inspectTrade(counterOffer, index); }

inline void inspectionNormalObject(const Position& position)
{ g_game.inspectionNormalObject(position); }

inline void inspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount)
{ g_game.inspectionObject(inspectionType, itemId, itemCount); }

inline void inviteToOwnChannel(std::string_view name)
{ g_game.inviteToOwnChannel(name); }

inline bool isFollowing()
{ return g_game.isFollowing(); }

inline void joinAggression(uint32_t creatureId)
{ g_game.joinAggression(creatureId); }

inline void joinChannel(uint16_t channelId)
{ g_game.joinChannel(channelId); }

inline void leaveChannel(uint16_t channelId)
{ g_game.leaveChannel(channelId); }

inline void leaveMarket()
{ g_game.leaveMarket(); }

inline void lookCreature(uint32_t creatureId, bool isBattleList = false)
{ g_game.lookCreature(creatureId, isBattleList); }

inline void lookMapObject(const Position& position, uint16_t itemId, int stackPos)
{ g_game.lookMapObject(position, itemId, stackPos); }

inline void mount(bool mount)
{ g_game.mount(mount); }

inline void moveMapObject(const Position& from, uint16_t itemId, int stackPos, const Position& toPos, int count)
{ g_game.moveMapObject(from, itemId, stackPos, toPos, count); }

inline void openDepotSearch()
{ g_game.openDepotSearch(); }

inline void openOwnChannel()
{ g_game.openOwnChannel(); }

inline void openParentById(int containerId)
{ g_game.openParentById(containerId); }

inline void openParentContainerFromDepotSearch(const Position& pos)
{ g_game.openParentContainerFromDepotSearch(pos); }

inline void openStore(uint8_t serviceType = 0, std::string_view category = "")
{ g_game.openStore(serviceType, category); }

inline void openTransactionHistory(uint8_t entriesPerPage)
{ g_game.openTransactionHistory(entriesPerPage); }

inline void openWheel(uint32_t playerId)
{ g_game.openWheel(playerId); }

inline void partyDisband()
{ g_game.partyDisband(); }

inline void partyInvite(uint32_t creatureId)
{ g_game.partyInvite(creatureId); }

inline void partyLeave()
{ g_game.partyLeave(); }

inline void partyPassLeadership(uint32_t creatureId)
{ g_game.partyPassLeadership(creatureId); }

inline void partyRevokeInvitation(uint32_t creatureId)
{ g_game.partyRevokeInvitation(creatureId); }

inline void partyShareExperience(bool active)
{ g_game.partyShareExperience(active); }

inline void preyAction(uint8_t slot, uint8_t actionType, uint16_t index)
{ g_game.preyAction(slot, actionType, index); }

inline void processInventoryChange(uint8_t slot, const ::tibia::container::TInventorySlot& item)
{ g_game.processInventoryChange(slot, item); }

inline void rejectTrade()
{ g_game.rejectTrade(); }

inline void removeVip(uint32_t playerId)
{ g_game.removeVip(playerId); }

inline void reportBugCip(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position)
{ g_game.reportBugCip(category, comment, typoSpeaker, typoText, position); }

inline void reportRuleViolationCip(uint8_t type, uint8_t reason, std::string_view name, std::string_view comment, std::string_view translation, uint32_t id)
{ g_game.reportRuleViolationCip(type, reason, name, comment, translation, id); }

inline void requestBestiary()
{ g_game.requestBestiary(); }

inline void requestBestiaryOverview(std::string_view catName, bool search = false, std::vector<uint16_t> raceIds = {})
{ g_game.requestBestiaryOverview(catName, search, raceIds); }

inline void requestBossSlootInfo()
{ g_game.requestBossSlootInfo(); }

inline void requestBossSlotAction(uint8_t action, uint32_t raceId)
{ g_game.requestBossSlotAction(action, raceId); }

inline void requestBosstiaryInfo()
{ g_game.requestBosstiaryInfo(); }

inline void requestChannels()
{ g_game.requestChannels(); }

inline void requestCharacterTradeConfiguration(uint8_t action)
{ g_game.requestCharacterTradeConfiguration(action); }

inline void requestDepotSearchItem(uint16_t itemId, uint8_t tier = 0)
{ g_game.requestDepotSearchItem(itemId, tier); }

inline void requestGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items)
{ g_game.requestGetRewardDaily(bonusShrine, items); }

inline void requestGuildMessage()
{ g_game.requestGuildMessage(); }

inline void requestHighscore(uint8_t action, uint8_t category, uint32_t vocation, std::string_view world, uint8_t worldType, uint8_t battlEye, uint16_t page, uint8_t totalPages)
{ g_game.requestHighscore(action, category, vocation, world, worldType, battlEye, page, totalPages); }

inline void requestItemInfo(uint16_t typeId, uint8_t subType, uint8_t index)
{ g_game.requestItemInfo(typeId, subType, index); }

inline void requestMarketStatistics(uint16_t itemId = 0, uint8_t tier = 0)
{ g_game.requestMarketStatistics(itemId, tier); }

inline void requestOpenRewardHistory()
{ g_game.requestOpenRewardHistory(); }

inline void requestOutfit()
{ g_game.requestOutfit(); }

inline void requestQuestLine(uint16_t questId)
{ g_game.requestQuestLine(questId); }

inline void requestQuestLog()
{ g_game.requestQuestLog(); }

inline void requestResourceBalance(uint8_t resourceType)
{ g_game.requestResourceBalance(resourceType); }

inline void requestSendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId)
{ g_game.requestSendBuyCharmRune(runeId, action, raceId); }

inline void requestSendCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage = 0, uint16_t page = 0)
{ g_game.requestSendCharacterInfo(playerId, characterInfoType, entriesPerPage, page); }

inline void requestSendCyclopediaHouseAuction(tibia::client::CyclopediaHouseAuctionType_t type, uint32_t houseId, uint32_t timestamp = 0, uint64_t bidValue = 0, std::string_view name = "")
{ g_game.requestSendCyclopediaHouseAuction(type, houseId, timestamp, bidValue, name); }

inline void requestSetCharacterTitle(uint8_t titleId)
{ g_game.requestSetCharacterTitle(titleId); }

inline void requestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder, const uint8_t serviceType)
{ g_game.requestStoreOffers(categoryName, subCategory, sortOrder, serviceType); }

inline void requestTextForReport(const Position& position, uint16_t typeId, uint8_t stackPosition)
{ g_game.requestTextForReport(position, typeId, stackPosition); }

inline void requestTradeAt(const Position& position, uint16_t itemId, int stackPos, uint32_t creatureId)
{ g_game.requestTradeAt(position, itemId, stackPos, creatureId); }

inline void requestTransactionDetails(uint32_t transactionId)
{ g_game.requestTransactionDetails(transactionId); }

inline void requestTransactionHistory(uint32_t page, uint32_t entriesPerPage)
{ g_game.requestTransactionHistory(page, entriesPerPage); }

inline void retrieveDepotSearch(uint16_t itemId, uint8_t tier, uint8_t type)
{ g_game.retrieveDepotSearch(itemId, tier, type); }

inline void rotateMapObject(const Position& position, uint16_t itemId, int stackPos)
{ g_game.rotateMapObject(position, itemId, stackPos); }

inline void seekInContainer(uint8_t containerId, uint16_t index)
{ g_game.seekInContainer(containerId, index); }

inline void selectImbuementItem(uint16_t itemId, const Position& pos, uint8_t stackpos)
{ g_game.selectImbuementItem(itemId, pos, stackpos); }

inline void selectImbuementScroll()
{ g_game.selectImbuementScroll(); }

inline void sellItem(uint16_t typeId, uint8_t subType, uint16_t amount, bool ignoreEquipped)
{ g_game.sellItem(typeId, subType, amount, ignoreEquipped); }

inline void sendApplyWheelPoints(const std::vector<uint16_t>& slotPoints,uint16_t greenGem,uint16_t redGem,uint16_t acquaGem,uint16_t purpleGem)
{ g_game.sendApplyWheelPoints(slotPoints, greenGem, redGem, acquaGem, purpleGem); }

inline void sendBossDifficultyAction(uint8_t action, uint16_t difficulty = 0)
{ g_game.sendBossDifficultyAction(action, difficulty); }

inline void sendContainerAction(uint8_t action, uint8_t containerId = 0)
{ g_game.sendContainerAction(action, containerId); }

inline void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount)
{ g_game.sendCyclopediaMapDonate(areaId, amount); }

inline void sendCyclopediaMapResetViewpoints(uint16_t subAreaId)
{ g_game.sendCyclopediaMapResetViewpoints(subAreaId); }

inline void sendExivaOptions(bool allowAll, bool allowOwnGuild, bool allowOwnParty, bool allowVipList, bool allowPlayerWhitelist, bool allowGuildWhitelist, const std::vector<std::string>& characterWhiteList, const std::vector<std::string>& removeCharacter, const std::vector<std::string>& guildWhiteList, const std::vector<std::string>& removeGuild)
{ g_game.sendExivaOptions(allowAll, allowOwnGuild, allowOwnParty, allowVipList, allowPlayerWhitelist, allowGuildWhitelist, characterWhiteList, removeCharacter, guildWhiteList, removeGuild); }

inline void sendForgeBrowseHistoryRequest(uint16_t page)
{ g_game.sendForgeBrowseHistoryRequest(page); }

inline void sendOpenRewardWall()
{ g_game.sendOpenRewardWall(); }

inline void sendPartyAnalyzerPriceType()
{ g_game.sendPartyAnalyzerPriceType(); }

inline void sendPartyAnalyzerReset()
{ g_game.sendPartyAnalyzerReset(); }

inline void sendQuickLootAt(uint8_t variant, const Position& position, uint16_t itemId, int stackPos)
{ g_game.sendQuickLootAt(variant, position, itemId, stackPos); }

inline void sendRequestStoreHome()
{ g_game.sendRequestStoreHome(); }

inline void sendRequestStoreOfferById(const uint32_t offerId, const uint8_t sortOrder, const uint8_t serviceType)
{ g_game.sendRequestStoreOfferById(offerId, sortOrder, serviceType); }

inline void sendRequestStorePremiumBoost()
{ g_game.sendRequestStorePremiumBoost(); }

inline void sendRequestStoreSearch(const std::string_view searchText, const uint8_t sortOrder, const uint8_t serviceType)
{ g_game.sendRequestStoreSearch(searchText, sortOrder, serviceType); }

inline void sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, bool autoTrackNewQuests, bool autoUntrackCompletedQuests, uint8_t extra = 0x2A)
{ g_game.sendRequestTrackerQuestLog(missionIds, autoTrackNewQuests, autoUntrackCompletedQuests, extra); }

inline void sendRequestUsefulThings(const uint8_t serviceType)
{ g_game.sendRequestUsefulThings(serviceType); }

inline void sendSelectSpellAim(const std::vector<uint16_t>& spellIds, bool enabled)
{ g_game.sendSelectSpellAim(spellIds, enabled); }

inline void sendSetHirelingName(std::string_view name)
{ g_game.sendSetHirelingName(name); }

inline void sendStartOfflineTraining(const uint8_t skillType)
{ g_game.sendStartOfflineTraining(skillType); }

inline void sendTeleport(const Position& pos)
{ g_game.sendTeleport(pos); }

inline void sendTutorialChangeVocation(uint8_t vocationClientId)
{ g_game.sendTutorialChangeVocation(vocationClientId); }

inline void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId = 0)
{ g_game.sendWeaponProficiencyAction(actionType, itemId); }

inline void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions)
{ g_game.sendWeaponProficiencyApply(itemId, levels, perkPositions); }

inline void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex = 0)
{ g_game.sendWeaponProficiencySlotAction(actionType, itemId, level, perkPosition, offerIndex); }

inline void setClientOption(uint8_t option, bool value)
{ g_game.setClientOption(option, value); }

inline void setCreaturePodiumConfiguration(uint32_t monsterRaceId, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible, uint8_t monsterVisible)
{ g_game.setCreaturePodiumConfiguration(monsterRaceId, pos, itemId, stackpos, direction, podiumVisible, monsterVisible); }

inline void setHirelingOutfit(const Outfit& outfit, uint32_t hirelingCreatureId)
{ g_game.setHirelingOutfit(outfit, hirelingCreatureId); }

inline void setShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible)
{ g_game.setShowOffSocketConfiguration(outfit, pos, itemId, stackpos, direction, podiumVisible); }

inline void setTactics(tibia::client::ChaseModes chaseMode, bool safeFight, tibia::client::PVPModes pvpMode)
{ g_game.setTactics(chaseMode, safeFight, pvpMode); }

inline void soulsealFightAction(uint16_t raceId)
{ g_game.soulsealFightAction(raceId); }

inline void stashStowItem(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action)
{ g_game.stashStowItem(position, itemId, count, stackpos, action); }

inline void stashWithdraw(uint16_t itemId, uint32_t count, uint8_t stackpos)
{ g_game.stashWithdraw(itemId, count, stackpos); }

inline void stop()
{ g_game.stop(); }

inline void talkChannel(tibia::client::MessageMode mode, uint16_t channelId, std::string_view message)
{ g_game.talkChannel(mode, channelId, message); }

inline void talkPrivate(tibia::client::MessageMode mode, std::string_view receiver, std::string_view message)
{ g_game.talkPrivate(mode, receiver, message); }

inline void taskHuntingShopPurchase(uint8_t offerIndex)
{ g_game.taskHuntingShopPurchase(offerIndex); }

inline void taskHuntingShopRequest()
{ g_game.taskHuntingShopRequest(); }

inline void transferCoins(std::string_view recipient, uint32_t amount)
{ g_game.transferCoins(recipient, amount); }

inline void useInventoryItem(uint16_t itemId)
{ g_game.useInventoryItem(itemId); }

inline void useMapObjectOnCreature(const Position& from, uint16_t fromId, int fromStack, uint32_t creatureId)
{ g_game.useMapObjectOnCreature(from, fromId, fromStack, creatureId); }

inline void useWithMapObject(const Position& from, uint16_t fromId, int fromStack, const Position& to, uint16_t toId, int toStack)
{ g_game.useWithMapObject(from, fromId, fromStack, to, toId, toStack); }

inline void weeklyTaskAction(uint8_t actionType, uint16_t value = 0)
{ g_game.weeklyTaskAction(actionType, value); }

inline void wrapMapObject(const Position& position, uint16_t itemId, int stackPos)
{ g_game.wrapMapObject(position, itemId, stackPos); }


// --- Inline Game accessors (bodies live in game.h; not parsed by generator) ---

inline int getProtocolVersion()
{
    return g_game.getProtocolVersion();
}

inline bool isUsingProtobuf()
{
    return g_game.isUsingProtobuf();
}

inline int getPing()
{
    return g_game.getPing();
}

inline auto getVips()
{
    return g_game.getVips();
}

inline uint32_t getFollowingCreatureId()
{
    return g_game.getFollowingCreatureId();
}

inline void setCanReportBugs(const bool enable)
{
    g_game.setCanReportBugs(enable);
}

inline bool canExivaOptions()
{
    return g_game.canExivaOptions();
}

inline void setCanExivaOptions(const bool enable)
{
    g_game.setCanExivaOptions(enable);
}

inline std::string getWorldName()
{
    return g_game.getWorldName();
}

inline auto getWalkMaxSteps()
{
    return g_game.getWalkMaxSteps();
}

inline void setWalkMaxSteps(const uint8_t v)
{
    g_game.setWalkMaxSteps(v);
}

inline int getBugReportState()
{
    return g_game.getBugReportState();
}

inline void setBugReportState(const int state)
{
    g_game.setBugReportState(state);
}

inline UnjustifiedPoints getUnjustifiedPoints()
{
    return g_game.getUnjustifiedPoints();
}

inline uint32_t getOpenPvpSituations()
{
    return g_game.getOpenPvpSituations();
}

inline ProtocolGamePtr getProtocolGame()
{
    return g_game.getProtocolGame();
}

inline uint16_t getServerBeat()
{
    return g_game.getServerBeat();
}

inline void setServerBeat(const uint16_t beat)
{
    g_game.setServerBeat(beat);
}

inline void setExpertPvpMode(const bool enable)
{
    g_game.setExpertPvpMode(enable);
}

inline void setNextTalkAim(const Position& pos)
{
    g_game.setNextTalkAim(pos);
}

inline void setNextTalkAimMode(const uint8_t mode)
{
    g_game.setNextTalkAimMode(mode);
}

inline Position takeNextTalkAim()
{
    return g_game.takeNextTalkAim();
}

inline void resetMapUpdatedAt()
{
    g_game.resetMapUpdatedAt();
}

inline void updateMapLatency()
{
    g_game.updateMapLatency();
}

} // namespace game
} // namespace tibia
