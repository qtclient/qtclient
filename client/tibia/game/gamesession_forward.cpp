#include "gamesession_forward.h"

#include "gamehub.h"
#include "gamehub_impl.h"
namespace tibia {
namespace game {

void acceptMarketOffer(uint32_t timestamp, uint16_t counter, uint16_t amount)
{
    tipGameHub().acceptMarketOffer(timestamp, counter, amount);
}

void acceptTrade()
{
    tipGameHub().acceptTrade();
}

void addVip(std::string_view name)
{
    tipGameHub().addVip(name);
}

void answerModalDialog(uint32_t dialog, uint8_t button, uint8_t choice)
{
    tipGameHub().answerModalDialog(dialog, button, choice);
}

void applyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm)
{
    tipGameHub().applyImbuement(slot, imbuementId, protectionCharm);
}

void attack(uint32_t creatureId)
{
    tipGameHub().attack(creatureId);
}

void bountyPreferredAction(uint8_t actionType, uint16_t slot, uint16_t raceId)
{
    tipGameHub().bountyPreferredAction(actionType, slot, raceId);
}

void bountyTalismanUpgrade(uint8_t pathIndex)
{
    tipGameHub().bountyTalismanUpgrade(pathIndex);
}

void bountyTaskAction(uint8_t actionType, uint16_t value)
{
    tipGameHub().bountyTaskAction(actionType, value);
}

void browseField(const Position& position)
{
    tipGameHub().browseField(position);
}

void browseMarket(uint8_t browseId, uint16_t browseType, uint8_t tier)
{
    tipGameHub().browseMarket(browseId, browseType, tier);
}

void buyItem(uint16_t typeId, uint8_t countOrSubType, uint16_t amount, bool ignoreCapacity, bool buyWithBackpack)
{
    tipGameHub().buyItem(typeId, countOrSubType, amount, ignoreCapacity, buyWithBackpack);
}

void buyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location)
{
    tipGameHub().buyStoreOffer(offerId, action, name, type, location);
}

bool canPerformGameAction()
{
    return tipGameHub().canPerformGameAction();
}

void cancelAttack()
{
    tipGameHub().cancelAttack();
}

void cancelFollow()
{
    tipGameHub().cancelFollow();
}

void cancelMarketOffer(uint32_t timestamp, uint16_t counter)
{
    tipGameHub().cancelMarketOffer(timestamp, counter);
}

void changeOutfitAndMount(const Outfit& outfit, bool mounted, bool randomMount)
{
    tipGameHub().changeOutfitAndMount(outfit, mounted, randomMount);
}

void clearImbuement(uint8_t slot)
{
    tipGameHub().clearImbuement(slot);
}

void closeById(int containerId)
{
    tipGameHub().closeById(containerId);
}

void closeDepotSearchRequest()
{
    tipGameHub().closeDepotSearchRequest();
}

void closeImbuingWindow()
{
    tipGameHub().closeImbuingWindow();
}

void closeNpcChannel()
{
    tipGameHub().closeNpcChannel();
}

void closeNpcTrade()
{
    tipGameHub().closeNpcTrade();
}

void configureShowOffSocket(const Position& pos, uint16_t itemId, uint8_t stackpos)
{
    tipGameHub().configureShowOffSocket(pos, itemId, stackpos);
}

void confirmCharacterTradeConfiguration(uint32_t price, uint32_t endTimestamp, const std::vector<std::tuple<uint16_t, uint8_t>>& items, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems, const std::vector<uint16_t>& highlights, bool createAuction)
{
    tipGameHub().confirmCharacterTradeConfiguration(price, endTimestamp, items, storeItems, highlights, createAuction);
}

void createMarketOffer(uint8_t type, uint16_t itemId, uint8_t itemTier, uint16_t amount, uint64_t price, uint8_t anonymous)
{
    tipGameHub().createMarketOffer(type, itemId, itemTier, amount, price, anonymous);
}

void editGuildMessage(std::string_view text)
{
    tipGameHub().editGuildMessage(text);
}

void editList(uint32_t id, uint8_t doorId, std::string_view text)
{
    tipGameHub().editList(id, doorId, text);
}

void editText(uint32_t id, std::string_view text)
{
    tipGameHub().editText(id, text);
}

void editVip(uint32_t playerId, std::string_view description, uint32_t iconId, bool notifyLogin, const std::vector<uint8_t>& groupID)
{
    tipGameHub().editVip(playerId, description, iconId, notifyLogin, groupID);
}

void editVipGroups(tibia::client::GroupsEditInfoType_t action, uint8_t groupId, std::string_view groupName)
{
    tipGameHub().editVipGroups(action, groupId, groupName);
}

ItemPtr ephemeralItemForWire(uint16_t typeId, const Position& position, int stackPos, uint16_t count, uint8_t tier, uint8_t fluidOrSubtype)
{
    return tipGameHub().ephemeralItemForWire(typeId, position, stackPos, count, tier, fluidOrSubtype);
}

void equipItemId(const uint16_t itemId, const uint8_t tier)
{
    tipGameHub().equipItemId(itemId, tier);
}

void excludeFromOwnChannel(std::string_view name)
{
    tipGameHub().excludeFromOwnChannel(name);
}

void follow(uint32_t creatureId)
{
    tipGameHub().follow(creatureId);
}

void forgeRequest(tibia::client::ForgeAction_t actionType, bool convergence, uint16_t firstItemid, uint8_t firstItemTier, uint16_t secondItemId, bool improveChance, bool tierLoss)
{
    tipGameHub().forgeRequest(actionType, convergence, firstItemid, firstItemTier, secondItemId, improveChance, tierLoss);
}

void gatherAll(const Position& pos, uint16_t itemId, uint8_t stackpos)
{
    tipGameHub().gatherAll(pos, itemId, stackpos);
}

void gemAction(uint8_t actionType, uint16_t param, uint8_t pos)
{
    tipGameHub().gemAction(actionType, param, pos);
}

void greetNpc(uint32_t npcId)
{
    tipGameHub().greetNpc(npcId);
}

void inspectCharacter(const uint32_t creatureId, const uint8_t tab)
{
    tipGameHub().inspectCharacter(creatureId, tab);
}

void inspectNpcTrade(uint16_t typeId, uint8_t count)
{
    tipGameHub().inspectNpcTrade(typeId, count);
}

void inspectTrade(bool counterOffer, uint8_t index)
{
    tipGameHub().inspectTrade(counterOffer, index);
}

void inspectionNormalObject(const Position& position)
{
    tipGameHub().inspectionNormalObject(position);
}

void inspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount)
{
    tipGameHub().inspectionObject(inspectionType, itemId, itemCount);
}

void inviteToOwnChannel(std::string_view name)
{
    tipGameHub().inviteToOwnChannel(name);
}

bool isFollowing()
{
    return tipGameHub().isFollowing();
}

void joinAggression(uint32_t creatureId)
{
    tipGameHub().joinAggression(creatureId);
}

void joinChannel(uint16_t channelId)
{
    tipGameHub().joinChannel(channelId);
}

void leaveChannel(uint16_t channelId)
{
    tipGameHub().leaveChannel(channelId);
}

void leaveMarket()
{
    tipGameHub().leaveMarket();
}

void lookCreature(uint32_t creatureId, bool isBattleList)
{
    tipGameHub().lookCreature(creatureId, isBattleList);
}

void lookMapObject(const Position& position, uint16_t itemId, int stackPos)
{
    tipGameHub().lookMapObject(position, itemId, stackPos);
}

void mount(bool mount)
{
    tipGameHub().mount(mount);
}

void moveMapObject(const Position& from, uint16_t itemId, int stackPos, const Position& toPos, int count)
{
    tipGameHub().moveMapObject(from, itemId, stackPos, toPos, count);
}

void openDepotSearch()
{
    tipGameHub().openDepotSearch();
}

void openOwnChannel()
{
    tipGameHub().openOwnChannel();
}

void openParentById(int containerId)
{
    tipGameHub().openParentById(containerId);
}

void openParentContainerFromDepotSearch(const Position& pos)
{
    tipGameHub().openParentContainerFromDepotSearch(pos);
}

void openStore(uint8_t serviceType, std::string_view category)
{
    tipGameHub().openStore(serviceType, category);
}

void openTransactionHistory(uint8_t entriesPerPage)
{
    tipGameHub().openTransactionHistory(entriesPerPage);
}

void openWheel(uint32_t playerId)
{
    tipGameHub().openWheel(playerId);
}

void partyDisband()
{
    tipGameHub().partyDisband();
}

void partyInvite(uint32_t creatureId)
{
    tipGameHub().partyInvite(creatureId);
}

void partyLeave()
{
    tipGameHub().partyLeave();
}

void partyPassLeadership(uint32_t creatureId)
{
    tipGameHub().partyPassLeadership(creatureId);
}

void partyRevokeInvitation(uint32_t creatureId)
{
    tipGameHub().partyRevokeInvitation(creatureId);
}

void partyShareExperience(bool active)
{
    tipGameHub().partyShareExperience(active);
}

void preyAction(uint8_t slot, uint8_t actionType, uint16_t index)
{
    tipGameHub().preyAction(slot, actionType, index);
}

void processInventoryChange(uint8_t slot, const ::tibia::container::TInventorySlot& item)
{
    tipGameHub().processInventoryChange(slot, item);
}

void rejectTrade()
{
    tipGameHub().rejectTrade();
}

void removeVip(uint32_t playerId)
{
    tipGameHub().removeVip(playerId);
}

void reportBugCip(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position)
{
    tipGameHub().reportBugCip(category, comment, typoSpeaker, typoText, position);
}

void reportRuleViolationCip(uint8_t type, uint8_t reason, std::string_view name, std::string_view comment, std::string_view translation, uint32_t id)
{
    tipGameHub().reportRuleViolationCip(type, reason, name, comment, translation, id);
}

void requestBestiary()
{
    tipGameHub().requestBestiary();
}

void requestBestiaryOverview(std::string_view catName, bool search, std::vector<uint16_t> raceIds)
{
    tipGameHub().requestBestiaryOverview(catName, search, raceIds);
}

void requestBossSlootInfo()
{
    tipGameHub().requestBossSlootInfo();
}

void requestBossSlotAction(uint8_t action, uint32_t raceId)
{
    tipGameHub().requestBossSlotAction(action, raceId);
}

void requestBosstiaryInfo()
{
    tipGameHub().requestBosstiaryInfo();
}

void requestChannels()
{
    tipGameHub().requestChannels();
}

void requestCharacterTradeConfiguration(uint8_t action)
{
    tipGameHub().requestCharacterTradeConfiguration(action);
}

void requestDepotSearchItem(uint16_t itemId, uint8_t tier)
{
    tipGameHub().requestDepotSearchItem(itemId, tier);
}

void requestGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items)
{
    tipGameHub().requestGetRewardDaily(bonusShrine, items);
}

void requestGuildMessage()
{
    tipGameHub().requestGuildMessage();
}

void requestHighscore(uint8_t action, uint8_t category, uint32_t vocation, std::string_view world, uint8_t worldType, uint8_t battlEye, uint16_t page, uint8_t totalPages)
{
    tipGameHub().requestHighscore(action, category, vocation, world, worldType, battlEye, page, totalPages);
}

void requestItemInfo(uint16_t typeId, uint8_t subType, uint8_t index)
{
    tipGameHub().requestItemInfo(typeId, subType, index);
}

void requestMarketStatistics(uint16_t itemId, uint8_t tier)
{
    tipGameHub().requestMarketStatistics(itemId, tier);
}

void requestOpenRewardHistory()
{
    tipGameHub().requestOpenRewardHistory();
}

void requestOutfit()
{
    tipGameHub().requestOutfit();
}

void requestQuestLine(uint16_t questId)
{
    tipGameHub().requestQuestLine(questId);
}

void requestQuestLog()
{
    tipGameHub().requestQuestLog();
}

void requestResourceBalance(uint8_t resourceType)
{
    tipGameHub().requestResourceBalance(resourceType);
}

void requestSendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId)
{
    tipGameHub().requestSendBuyCharmRune(runeId, action, raceId);
}

void requestSendCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage, uint16_t page)
{
    tipGameHub().requestSendCharacterInfo(playerId, characterInfoType, entriesPerPage, page);
}

void requestSendCyclopediaHouseAuction(tibia::client::CyclopediaHouseAuctionType_t type, uint32_t houseId, uint32_t timestamp, uint64_t bidValue, std::string_view name)
{
    tipGameHub().requestSendCyclopediaHouseAuction(type, houseId, timestamp, bidValue, name);
}

void requestSetCharacterTitle(uint8_t titleId)
{
    tipGameHub().requestSetCharacterTitle(titleId);
}

void requestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder, const uint8_t serviceType)
{
    tipGameHub().requestStoreOffers(categoryName, subCategory, sortOrder, serviceType);
}

void requestTextForReport(const Position& position, uint16_t typeId, uint8_t stackPosition)
{
    tipGameHub().requestTextForReport(position, typeId, stackPosition);
}

void requestTradeAt(const Position& position, uint16_t itemId, int stackPos, uint32_t creatureId)
{
    tipGameHub().requestTradeAt(position, itemId, stackPos, creatureId);
}

void requestTransactionDetails(uint32_t transactionId)
{
    tipGameHub().requestTransactionDetails(transactionId);
}

void requestTransactionHistory(uint32_t page, uint32_t entriesPerPage)
{
    tipGameHub().requestTransactionHistory(page, entriesPerPage);
}

void retrieveDepotSearch(uint16_t itemId, uint8_t tier, uint8_t type)
{
    tipGameHub().retrieveDepotSearch(itemId, tier, type);
}

void rotateMapObject(const Position& position, uint16_t itemId, int stackPos)
{
    tipGameHub().rotateMapObject(position, itemId, stackPos);
}

void seekInContainer(uint8_t containerId, uint16_t index)
{
    tipGameHub().seekInContainer(containerId, index);
}

void selectImbuementItem(uint16_t itemId, const Position& pos, uint8_t stackpos)
{
    tipGameHub().selectImbuementItem(itemId, pos, stackpos);
}

void selectImbuementScroll()
{
    tipGameHub().selectImbuementScroll();
}

void sellItem(uint16_t typeId, uint8_t subType, uint16_t amount, bool ignoreEquipped)
{
    tipGameHub().sellItem(typeId, subType, amount, ignoreEquipped);
}

void sendApplyWheelPoints(const std::vector<uint16_t>& slotPoints, const std::array<uint16_t, 8>& vesselGems)
{
    tipGameHub().sendApplyWheelPoints(slotPoints, vesselGems);
}

void sendBossDifficultyAction(uint8_t action, uint16_t difficulty)
{
    tipGameHub().sendBossDifficultyAction(action, difficulty);
}

void sendContainerAction(uint8_t action, uint8_t containerId)
{
    tipGameHub().sendContainerAction(action, containerId);
}

void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount)
{
    tipGameHub().sendCyclopediaMapDonate(areaId, amount);
}

void sendCyclopediaMapResetViewpoints(uint16_t subAreaId)
{
    tipGameHub().sendCyclopediaMapResetViewpoints(subAreaId);
}

void sendExivaOptions(bool allowAll, bool allowOwnGuild, bool allowOwnParty, bool allowVipList, bool allowPlayerWhitelist, bool allowGuildWhitelist, const std::vector<std::string>& characterWhiteList, const std::vector<std::string>& removeCharacter, const std::vector<std::string>& guildWhiteList, const std::vector<std::string>& removeGuild)
{
    tipGameHub().sendExivaOptions(allowAll, allowOwnGuild, allowOwnParty, allowVipList, allowPlayerWhitelist, allowGuildWhitelist, characterWhiteList, removeCharacter, guildWhiteList, removeGuild);
}

void sendForgeBrowseHistoryRequest(uint16_t page)
{
    tipGameHub().sendForgeBrowseHistoryRequest(page);
}

void sendOpenRewardWall()
{
    tipGameHub().sendOpenRewardWall();
}

void sendPartyAnalyzerPriceType()
{
    tipGameHub().sendPartyAnalyzerPriceType();
}

void sendPartyAnalyzerReset()
{
    tipGameHub().sendPartyAnalyzerReset();
}

void sendQuickLootAt(uint8_t variant, const Position& position, uint16_t itemId, int stackPos)
{
    tipGameHub().sendQuickLootAt(variant, position, itemId, stackPos);
}

void sendRequestStoreHome()
{
    tipGameHub().sendRequestStoreHome();
}

void sendRequestStoreOfferById(const uint32_t offerId, const uint8_t sortOrder, const uint8_t serviceType)
{
    tipGameHub().sendRequestStoreOfferById(offerId, sortOrder, serviceType);
}

void sendRequestStorePremiumBoost()
{
    tipGameHub().sendRequestStorePremiumBoost();
}

void sendRequestStoreSearch(const std::string_view searchText, const uint8_t sortOrder, const uint8_t serviceType)
{
    tipGameHub().sendRequestStoreSearch(searchText, sortOrder, serviceType);
}

void sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, bool autoTrackNewQuests, bool autoUntrackCompletedQuests, uint8_t extra)
{
    tipGameHub().sendRequestTrackerQuestLog(missionIds, autoTrackNewQuests, autoUntrackCompletedQuests, extra);
}

void sendRequestUsefulThings(const uint8_t serviceType)
{
    tipGameHub().sendRequestUsefulThings(serviceType);
}

void sendSelectSpellAim(const std::vector<uint16_t>& spellIds, bool enabled)
{
    tipGameHub().sendSelectSpellAim(spellIds, enabled);
}

void sendSetHirelingName(std::string_view name)
{
    tipGameHub().sendSetHirelingName(name);
}

void sendStartOfflineTraining(const uint8_t skillType)
{
    tipGameHub().sendStartOfflineTraining(skillType);
}

void sendTeleport(const Position& pos)
{
    tipGameHub().sendTeleport(pos);
}

void sendTyping(const bool typing)
{
    tipGameHub().sendTyping(typing);
}

void sendTutorialChangeVocation(uint8_t vocationClientId)
{
    tipGameHub().sendTutorialChangeVocation(vocationClientId);
}

void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId)
{
    tipGameHub().sendWeaponProficiencyAction(actionType, itemId);
}

void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions)
{
    tipGameHub().sendWeaponProficiencyApply(itemId, levels, perkPositions);
}

void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex)
{
    tipGameHub().sendWeaponProficiencySlotAction(actionType, itemId, level, perkPosition, offerIndex);
}

void setClientOption(uint8_t option, bool value)
{
    tipGameHub().setClientOption(option, value);
}

void setCreaturePodiumConfiguration(uint32_t monsterRaceId, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible, uint8_t monsterVisible)
{
    tipGameHub().setCreaturePodiumConfiguration(monsterRaceId, pos, itemId, stackpos, direction, podiumVisible, monsterVisible);
}

void setHirelingOutfit(const Outfit& outfit, uint32_t hirelingCreatureId)
{
    tipGameHub().setHirelingOutfit(outfit, hirelingCreatureId);
}

void setShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible)
{
    tipGameHub().setShowOffSocketConfiguration(outfit, pos, itemId, stackpos, direction, podiumVisible);
}

void setTactics(tibia::client::ChaseModes chaseMode, bool safeFight, tibia::client::PVPModes pvpMode)
{
    tipGameHub().setTactics(chaseMode, safeFight, pvpMode);
}

tibia::client::ChaseModes getChaseMode()
{
    return tipGameHub().getChaseMode();
}

bool isSafeFight()
{
    return tipGameHub().isSafeFight();
}

tibia::client::PVPModes getPVPMode()
{
    return tipGameHub().getPVPMode();
}

void soulsealFightAction(uint16_t raceId)
{
    tipGameHub().soulsealFightAction(raceId);
}

void stashStowItem(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action)
{
    tipGameHub().stashStowItem(position, itemId, count, stackpos, action);
}

void stashWithdraw(uint16_t itemId, uint32_t count, uint8_t stackpos)
{
    tipGameHub().stashWithdraw(itemId, count, stackpos);
}

void stop()
{
    tipGameHub().stop();
}

void talkChannel(tibia::client::MessageMode mode, uint16_t channelId, std::string_view message)
{
    tipGameHub().talkChannel(mode, channelId, message);
}

void talkPrivate(tibia::client::MessageMode mode, std::string_view receiver, std::string_view message)
{
    tipGameHub().talkPrivate(mode, receiver, message);
}

void taskHuntingShopPurchase(uint8_t offerIndex)
{
    tipGameHub().taskHuntingShopPurchase(offerIndex);
}

void taskHuntingShopRequest()
{
    tipGameHub().taskHuntingShopRequest();
}

void transferCoins(std::string_view recipient, uint32_t amount)
{
    tipGameHub().transferCoins(recipient, amount);
}

void useInventoryItem(uint16_t itemId)
{
    tipGameHub().useInventoryItem(itemId);
}

void useMapObjectOnCreature(const Position& from, uint16_t fromId, int fromStack, uint32_t creatureId)
{
    tipGameHub().useMapObjectOnCreature(from, fromId, fromStack, creatureId);
}

void useWithMapObject(const Position& from, uint16_t fromId, int fromStack, const Position& to, uint16_t toId, int toStack)
{
    tipGameHub().useWithMapObject(from, fromId, fromStack, to, toId, toStack);
}

void weeklyTaskAction(uint8_t actionType, uint16_t value)
{
    tipGameHub().weeklyTaskAction(actionType, value);
}

void wrapMapObject(const Position& position, uint16_t itemId, int stackPos)
{
    tipGameHub().wrapMapObject(position, itemId, stackPos);
}

int getProtocolVersion()
{
    return tipGameHub().getProtocolVersion();
}

bool isUsingProtobuf()
{
    return tipGameHub().isUsingProtobuf();
}

int getPing()
{
    return tipGameHub().getPing();
}

stdext::map<int, Vip> getVips()
{
    return tipGameHub().getVips();
}

uint32_t getFollowingCreatureId()
{
    return tipGameHub().getFollowingCreatureId();
}

void setCanReportBugs(const bool enable)
{
    tipGameHub().setCanReportBugs(enable);
}

bool canExivaOptions()
{
    return tipGameHub().canExivaOptions();
}

void setCanExivaOptions(const bool enable)
{
    tipGameHub().setCanExivaOptions(enable);
}

std::string getWorldName()
{
    return tipGameHub().getWorldName();
}

uint8_t getWalkMaxSteps()
{
    return tipGameHub().getWalkMaxSteps();
}

void setWalkMaxSteps(const uint8_t v)
{
    tipGameHub().setWalkMaxSteps(v);
}

int getBugReportState()
{
    return tipGameHub().getBugReportState();
}

void setBugReportState(const int state)
{
    tipGameHub().setBugReportState(state);
}

UnjustifiedPoints getUnjustifiedPoints()
{
    return tipGameHub().getUnjustifiedPoints();
}

void setUnjustifiedPoints(const UnjustifiedPoints unjustifiedPoints)
{
    tipGameHub().setUnjustifiedPoints(unjustifiedPoints);
}

uint32_t getOpenPvpSituations()
{
    return tipGameHub().getOpenPvpSituations();
}

void setOpenPvpSituations(const uint8_t openPvpSituations)
{
    tipGameHub().setOpenPvpSituations(openPvpSituations);
}

ProtocolGamePtr getProtocolGame()
{
    return tipGameHub().getProtocolGame();
}

uint16_t getServerBeat()
{
    return tipGameHub().getServerBeat();
}

void setServerBeat(const uint16_t beat)
{
    tipGameHub().setServerBeat(beat);
}

void setExpertPvpMode(const bool enable)
{
    tipGameHub().setExpertPvpMode(enable);
}

bool getExpertPvpMode()
{
    return tipGameHub().getExpertPvpMode();
}

void setNextTalkAim(const Position& pos)
{
    tipGameHub().setNextTalkAim(pos);
}

void setNextTalkAimMode(const uint8_t mode)
{
    tipGameHub().setNextTalkAimMode(mode);
}

Position takeNextTalkAim()
{
    return tipGameHub().takeNextTalkAim();
}

uint8_t takeNextTalkAimMode()
{
    return tipGameHub().takeNextTalkAimMode();
}

void resetMapUpdatedAt()
{
    tipGameHub().resetMapUpdatedAt();
}

void updateMapLatency()
{
    tipGameHub().updateMapLatency();
}

void requestStop()
{
    tipGameHub().requestStop();
}

void wireAutoWalk(const std::vector<tibia::client::Direction>& dirs, const Position& startPos)
{
    tipGameHub().autoWalk(dirs, startPos);
}

} // namespace game
} // namespace tibia
