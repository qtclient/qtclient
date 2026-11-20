#include "gamesession_forward.h"

#include "game/gamesessionstate.h"
#include "tibia/runtime/host.h"
#include <tibia/game/sessionhooks.h>
#include "tibia/gamewindow/remainingdialogs.h"
#include <tibia/game/gamenotify.h>
#include <tibia/creatures/attachedeffect.h>
#include <tibia/config/runtimegameconfig.h>
#include "input/playermovementintenthandler.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "app/clientoptions.h"
#include "config/clientoptiondefaults.h"
#include "minimap/waypointcosts.h"
#include <tibia/game/mapsession.h>
#include "protocol/protocolcodes.h"
#include "tibia/protocol/extendedopcode.h"
#include <tibia/appearances/appearancetype.h>
#include "tibia/assets/things.h"
#include "worldmap/worldmapfield.h"
#include "container/containerciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "creatures/creaturestorage.h"
#include "clockservice.h"
#include <QStringList>
#include <tibia/schedule/schedule.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gameservergamesession.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/mapsession.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/game/tgamenotify.h>
#include <QString>
#include <tibia/game/thingsession_forward.h>
#include "protocol/clientmessages.h"

namespace tibia {
namespace game {

namespace gameclient = ::tibia::protocol::gameclient;

void acceptMarketOffer(uint32_t timestamp, uint16_t counter, uint16_t amount)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendMarketAcceptOffer(timestamp, counter, amount);
}

void acceptTrade()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendAcceptTrade();
}

void answerModalDialog(uint32_t dialog, uint8_t button, uint8_t choice)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendAnswerModalDialog(dialog, button, choice);
}

void applyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendApplyImbuement(slot, imbuementId, protectionCharm);
}

void attack(uint32_t creatureId)
{
    gameSessionState().attack(creatureId);
}
void browseField(const Position& position)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendBrowseField(position);
}
void browseMarket(const uint8_t browseId, const uint16_t browseType, const uint8_t tier)
{
    if (!canPerformGameAction()) {
        return;
    }

    gameclient::sendMarketBrowse(browseId, browseType, tier);
}
void buyItem(const uint16_t typeId, const uint8_t countOrSubType, const uint16_t amount, const bool ignoreCapacity, const bool buyWithBackpack)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    gameclient::sendBuyItem(typeId, countOrSubType, amount, ignoreCapacity, buyWithBackpack);
}

void buyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendBuyStoreOffer(offerId, action, name, type, location);
}

bool canPerformGameAction()
{
    return gameSessionState().canPerformGameAction();
}

void cancelAttack()
{
    gameSessionState().cancelAttack();
}

void cancelFollow()
{
    gameSessionState().cancelFollow();
}

void cancelMarketOffer(uint32_t timestamp, uint16_t counter)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendMarketCancelOffer(timestamp, counter);
}

void changeOutfitAndMount(const Outfit& outfit, bool mounted, bool randomMount)
{
    gameSessionState().changeOutfitAndMount(outfit, mounted, randomMount);
}

void clearImbuement(uint8_t slot)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendClearImbuement(slot);
}
void closeById(const int containerId)
{
    if (!canPerformGameAction() || containerId < 0)
        return;

    gameclient::sendCloseContainer(static_cast<uint8_t>(containerId));
}

void closeDepotSearchRequest()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCloseDepotSearch();
}

void closeImbuingWindow()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCloseImbuingWindow();
}

void closeNpcChannel()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCloseNpcChannel();
}

void closeNpcTrade()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCloseNpcTrade();
}

void configureShowOffSocket(const Position& pos, uint16_t itemId, uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendConfigureShowOffSocket(pos, itemId, stackpos);
}

void confirmCharacterTradeConfiguration(uint32_t price, uint32_t endTimestamp, const std::vector<std::tuple<uint16_t, uint8_t>>& items, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems, const std::vector<uint16_t>& highlights, bool createAuction)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCharacterTradeConfigurationAction(createAuction ? 3 : 2, price, endTimestamp, items,
                                                          storeItems, highlights);
}

void createMarketOffer(uint8_t type, uint16_t itemId, uint8_t itemTier, uint16_t amount, uint64_t price, uint8_t anonymous)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendMarketCreateOffer(type, itemId, itemTier, amount, price, anonymous);
}

void editGuildMessage(std::string_view text)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendEditGuildMessage(text);
}

void editList(uint32_t id, uint8_t doorId, std::string_view text)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendEditList(id, doorId, text);
}

void editText(uint32_t id, std::string_view text)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendEditText(id, text);
}

void equipItemId(const uint16_t itemId, const uint8_t tier)
{
    if (!canPerformGameAction())
        return;

    const auto& thing = tibia::game::thingType(itemId, ThingCategoryItem);
    if (thing && thing->getClassification() > 0) {
        gameclient::sendEquipItemWithTier(itemId, tier);
        return;
    }
    gameclient::sendEquipItemWithCountOrSubType(itemId, tier);
}
void excludeFromOwnChannel(const std::string_view name)
{
    if (!canPerformGameAction() || name.empty())
        return;

    gameclient::sendExcludeFromOwnChannel(name);
}

void follow(uint32_t creatureId)
{
    gameSessionState().follow(creatureId);
}

void forgeRequest(tibia::client::ForgeAction_t actionType, bool convergence, uint16_t firstItemid, uint8_t firstItemTier, uint16_t secondItemId, bool improveChance, bool tierLoss)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendForgeRequest(actionType, convergence, firstItemid, firstItemTier, secondItemId, improveChance, tierLoss);
}

void gatherAll(const Position& pos, uint16_t itemId, uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendGatherAll(pos, itemId, stackpos);
}

void gemAction(uint8_t actionType, uint16_t param, uint8_t pos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendWheelGemAction(actionType, param, pos);
}

void greetNpc(uint32_t npcId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendGreet(npcId);
}
void inspectCharacter(const uint32_t creatureId, const uint8_t tab)
{
    if (!canPerformGameAction())
        return;

    // CIP InspectPlayer: tab 0 is allow-all / withdraw (no creature id). Tabs 1+
    // name a player (invite/ask/allow/revoke/open inspection).
    if (creatureId == 0 && tab != 0)
        return;

    gameclient::sendInspectCharacter(creatureId, tab);
}
void inspectNpcTrade(const uint16_t typeId, const uint8_t count)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    gameclient::sendInspectNpcTrade(typeId, count);
}

void inspectTrade(bool counterOffer, uint8_t index)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendInspectTrade(counterOffer, index);
}

void inspectionNormalObject(const Position& position)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendInspectionNormalObject(position);
}

void inspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount)
{
    if (!canPerformGameAction())
        return;
    if (inspectionType == tibia::client::INSPECT_CYCLOPEDIA) {
        cyclopedia::TCyclopediaProtocolMessageHandler::sendInspectCyclopediaObject(itemId);
        return;
    }
    gameclient::sendInspectionObject(inspectionType, itemId, itemCount);
}
void inviteToOwnChannel(const std::string_view name)
{
    if (!canPerformGameAction() || name.empty())
        return;

    gameclient::sendInviteToOwnChannel(name);
}

bool isFollowing()
{
    return gameSessionState().isFollowing();
}

void joinAggression(uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendJoinAggression(creatureId);
}

void joinChannel(uint16_t channelId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendJoinChannel(channelId);
}

void leaveChannel(uint16_t channelId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendLeaveChannel(channelId);
}
void leaveMarket()
{
    if (!canPerformGameAction())
        return;

    gameclient::sendMarketLeave();
}

void lookCreature(uint32_t creatureId, bool isBattleList)
{
    gameSessionState().lookCreature(creatureId, isBattleList);
}
void lookMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    gameclient::sendLook(position, itemId, static_cast<uint8_t>(stackPos));
}

void mount(bool mount)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendMountStatus(mount);
}
void moveMapObject(const Position& from, const uint16_t itemId, const int stackPos, const Position& toPos, int count)
{
    if (count <= 0)
        count = 1;
    if (!canPerformGameAction() || !from.isValid() || itemId == 0 || stackPos < 0 || from == toPos)
        return;
    gameclient::sendMove(from, itemId, static_cast<uint8_t>(stackPos), toPos, count);
}

void openDepotSearch()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendOpenDepotSearch();
}

void openOwnChannel()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendOpenOwnChannel();
}
void openParentById(const int containerId)
{
    if (!canPerformGameAction() || containerId < 0)
        return;

    gameclient::sendUpContainer(static_cast<uint8_t>(containerId));
}

void openParentContainerFromDepotSearch(const Position& pos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendOpenParentContainer(pos);
}

void openStore(uint8_t serviceType, std::string_view category)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendOpenStore(serviceType, category);
}

void openTransactionHistory(uint8_t entriesPerPage)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendOpenTransactionHistory(entriesPerPage);
}

void partyDisband()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendDisbandParty();
}

void partyInvite(uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendInviteToParty(creatureId);
}

void partyLeave()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendLeaveParty();
}

void partyPassLeadership(uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendPassLeadership(creatureId);
}

void partyRevokeInvitation(uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRevokeInvitation(creatureId);
}

void partyShareExperience(bool active)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendShareExperience(active);
}

void preyAction(uint8_t slot, uint8_t actionType, uint16_t index)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendPreyAction(slot, actionType, index);
}

void processInventoryChange(uint8_t slot, const ::tibia::container::TInventorySlot& item)
{
    gameSessionState().processInventoryChange(slot, item);
}

void rejectTrade()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRejectTrade();
}

void reportBugCip(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendBugReport(category, comment, typoSpeaker, typoText, position);
}

void reportRuleViolationCip(uint8_t type, uint8_t reason, std::string_view name, std::string_view comment, std::string_view translation, uint32_t id)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRuleViolationReport(type, reason, name, comment, translation, id);
}

void requestBestiary()
{
    if (!canPerformGameAction())
        return;
    cyclopedia::TCyclopediaProtocolMessageHandler::sendOpenMonsterCyclopedia();
}

void requestBestiaryOverview(std::string_view catName, bool search, std::vector<uint16_t> raceIds)
{
    if (!canPerformGameAction())
        return;
    if (search)
        cyclopedia::TCyclopediaProtocolMessageHandler::sendOpenMonsterCyclopediaMonsters(raceIds);
    else
        cyclopedia::TCyclopediaProtocolMessageHandler::sendOpenMonsterCyclopediaMonsters(
            QString::fromUtf8(catName.data(), static_cast<qsizetype>(catName.size())));
}

void requestBossSlootInfo()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestBossSlootInfo();
}

void requestBossSlotAction(uint8_t action, uint32_t raceId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestBossSlotAction(action, raceId);
}

void requestBosstiaryInfo()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestBosstiaryInfo();
}

void requestChannels()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestChannels();
}

void requestCharacterTradeConfiguration(uint8_t action)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCharacterTradeConfigurationAction(action);
}

void requestDepotSearchItem(uint16_t itemId, uint8_t tier)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendDepotSearchType(itemId, tier);
}

void requestGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendGetRewardDaily(bonusShrine, items);
}

void requestGuildMessage()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendGetGuildMessage();
}

void requestItemInfo(const uint16_t typeId, const uint8_t subType, const uint8_t index)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    gameclient::sendRequestItemInfo(typeId, subType, index);
}

void requestOpenRewardHistory()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendOpenRewardHistory();
}

void requestOutfit()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestOutfit();
}

void requestQuestLine(uint16_t questId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestQuestLine(questId);
}

void requestQuestLog()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestQuestLog();
}

void requestResourceBalance(uint8_t resourceType)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestResourceBalance(resourceType);
}

void requestSendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId)
{
    if (!canPerformGameAction())
        return;
    using H = cyclopedia::TCyclopediaProtocolMessageHandler;
    switch (action) {
        case 0: H::sendUnlockCharm(runeId); break;
        case 1: H::sendAssignCharm(runeId, raceId); break;
        case 2: H::sendRemoveCharm(runeId); break;
        case 3: H::sendResetAllCharms(); break;
        default: break;
    }
}

void requestSendCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage, uint16_t page)
{
    if (!canPerformGameAction())
        return;
    using H = cyclopedia::TCyclopediaProtocolMessageHandler;
    if (characterInfoType == tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS ||
        characterInfoType == tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS)
        H::sendOpenCharacterInfo(playerId, characterInfoType, entriesPerPage, page);
    else
        H::sendOpenCharacterInfo(playerId, characterInfoType);
}

void requestSendCyclopediaHouseAuction(tibia::client::CyclopediaHouseAuctionType_t type, uint32_t houseId, uint32_t timestamp, uint64_t bidValue, std::string_view name)
{
    if (!canPerformGameAction())
        return;
    using H = cyclopedia::TCyclopediaProtocolMessageHandler;
    const QString text = QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
    switch (type) {
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_NONE: H::sendRequestTownHouses(text); break;
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_BID: H::sendHouseBid(houseId, bidValue); break;
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_MOVEOUT: H::sendHouseMoveOut(houseId, timestamp); break;
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_TRANSFER: H::sendHouseTransfer(houseId, timestamp, text, bidValue); break;
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_CANCEL_MOVEOUT: H::sendHouseCancelMoveOut(houseId); break;
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_CANCEL_TRANSFER: H::sendHouseCancelTransfer(houseId); break;
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_ACCEPT_TRANSFER: H::sendHouseAcceptTransfer(houseId); break;
        case tibia::client::CYCLOPEDIA_HOUSE_TYPE_REFECT_TRANSFER: H::sendHouseRejectTransfer(houseId); break;
    }
}

void requestSetCharacterTitle(uint8_t titleId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendFriendSystemAction(0x0E, titleId);
}

void requestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder, const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestStoreOffers(categoryName, subCategory, sortOrder, serviceType);
}

void requestTextForReport(const Position& position, uint16_t typeId, uint8_t stackPosition)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendGetTextForReport(position, typeId, stackPosition);
}
void requestTradeAt(const Position& position, const uint16_t itemId, const int stackPos, const uint32_t creatureId)
{
    if (!canPerformGameAction() || itemId == 0 || stackPos < 0 || !creatureId)
        return;

    gameclient::sendRequestTrade(position, itemId, static_cast<uint8_t>(stackPos), creatureId);
}

void requestTransactionDetails(uint32_t transactionId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendGetTransactionDetails(transactionId);
}

void requestTransactionHistory(uint32_t page, uint32_t entriesPerPage)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestTransactionHistory(page, entriesPerPage);
}

void retrieveDepotSearch(uint16_t itemId, uint8_t tier, uint8_t type)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRetrieveDepotSearch(itemId, tier, type);
}
void rotateMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    gameclient::sendRotateItem(position, itemId, static_cast<uint8_t>(stackPos));
}

void seekInContainer(uint8_t containerId, uint16_t index)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendSeekInContainer(containerId, index);
}

void selectImbuementItem(uint16_t itemId, const Position& pos, uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendImbuementWindowAction(tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM, itemId, pos, stackpos);
}

void selectImbuementScroll()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendImbuementWindowAction(tibia::client::IMBUEMENT_WINDOW_SCROLL);
}
void sellItem(const uint16_t typeId, const uint8_t subType, const uint16_t amount, const bool ignoreEquipped)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    gameclient::sendSellItem(typeId, subType, amount, ignoreEquipped);
}

void sendContainerAction(uint8_t action, uint8_t containerId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendContainerAction(action, containerId);
}

void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount)
{
    if (!canPerformGameAction())
        return;
    cyclopedia::TCyclopediaProtocolMessageHandler::sendMapDonation(areaId, amount);
}

void sendCyclopediaMapResetViewpoints(uint16_t subAreaId)
{
    if (!canPerformGameAction())
        return;
    cyclopedia::TCyclopediaProtocolMessageHandler::sendResetMapAreaViewpoints(subAreaId);
}

void sendCyclopediaMapRequestArea(uint16_t subAreaId)
{
    if (!canPerformGameAction())
        return;
    cyclopedia::TCyclopediaProtocolMessageHandler::sendRequestMapArea(subAreaId);
}
void sendExivaOptions(const bool allowAll, const bool allowOwnGuild, const bool allowOwnParty, const bool allowVipList, const bool allowPlayerWhitelist, const bool allowGuildWhitelist, const std::vector<std::string>& characterWhiteList, const std::vector<std::string>& removeCharacter, const std::vector<std::string>& guildWhiteList, const std::vector<std::string>& removeGuild)
{
    if (!canPerformGameAction() || !canExivaOptions())
        return;

    gameclient::sendExivaRestrictions(
        allowAll, allowOwnGuild, allowOwnParty, allowVipList,
        allowPlayerWhitelist, allowGuildWhitelist,
        characterWhiteList, removeCharacter, guildWhiteList, removeGuild);
}

void sendForgeBrowseHistoryRequest(uint16_t page)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendForgeBrowseHistoryRequest(page);
}

void sendOpenRewardWall()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendOpenRewardWall();
}
void sendPartyAnalyzerPriceType(const int lootType)
{
    if (!canPerformGameAction())
        return;
    cyclopedia::TCyclopediaProtocolMessageHandler::sendPartyHuntLootType(lootType);
}

void sendPartyAnalyzerReset()
{
    if (!canPerformGameAction())
        return;
    cyclopedia::TCyclopediaProtocolMessageHandler::sendResetPartyHuntAnalyser();
}

void sendQuickLootAt(uint8_t variant, const Position& position, uint16_t itemId, int stackPos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendQuickLoot(variant, position, itemId, static_cast<uint8_t>(std::max(0, stackPos)));
}

void sendRequestStoreHome()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestStoreHome();
}

void sendRequestStoreOfferById(const uint32_t offerId, const uint8_t sortOrder, const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestStoreOfferById(offerId, sortOrder, serviceType);
}

void sendRequestStorePremiumBoost()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestStorePremiumBoost();
}

void sendRequestStoreSearch(const std::string_view searchText, const uint8_t sortOrder, const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestStoreSearch(searchText, sortOrder, serviceType);
}
void sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, const bool autoTrackNewQuests, const bool autoUntrackCompletedQuests, const uint8_t extra)
{
    if (!canPerformGameAction()) {
        return;
    }

    gameclient::sendRequestTrackerQuestLog(
        missionIds,
        autoTrackNewQuests,
        autoUntrackCompletedQuests,
        extra
    );
}

void sendRequestUsefulThings(const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestUsefulThings(serviceType);
}

void sendSetHirelingName(std::string_view name)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendSetHirelingName(name);
}

void sendStartOfflineTraining(const uint8_t skillType)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendStartOfflineTraining(skillType);
}

void sendTeleport(const Position& pos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeleport(pos);
}

void sendTutorialChangeVocation(uint8_t vocationClientId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTutorialChangeVocation(vocationClientId);
}

void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendWeaponProficiencyAction(actionType, itemId);
}

void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendWeaponProficiencyApply(itemId, levels, perkPositions);
}

void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendWeaponProficiencySlotAction(actionType, itemId, level, perkPosition, offerIndex);
}

void setClientOption(uint8_t option, bool value)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendSetClientOptions(option, value);
}

void setCreaturePodiumConfiguration(uint32_t monsterRaceId, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible, uint8_t monsterVisible)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendSetCreaturePodiumConfiguration(monsterRaceId, pos, itemId, stackpos, direction,
                                                       podiumVisible, monsterVisible);
}

void setHirelingOutfit(const Outfit& outfit, uint32_t hirelingCreatureId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendSetHirelingOutfit(outfit, hirelingCreatureId);
}

void setShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, uint16_t itemId, uint8_t stackpos, uint8_t direction, uint8_t podiumVisible)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendSetShowOffSocketConfiguration(outfit, pos, itemId, stackpos, direction, podiumVisible);
}

void setTactics(tibia::client::ChaseModes chaseMode, bool safeFight, tibia::client::PVPModes pvpMode)
{
    gameSessionState().setTactics(chaseMode, safeFight, pvpMode);
}

tibia::client::ChaseModes getChaseMode()
{
    return gameSessionState().getChaseMode();
}

bool isSafeFight()
{
    return gameSessionState().isSafeFight();
}

tibia::client::PVPModes getPVPMode()
{
    return gameSessionState().getPVPMode();
}
void soulsealFightAction(const uint16_t raceId)
{
    if (!canPerformGameAction() || std::cmp_equal(raceId, 0))
        return;

    gameclient::sendSoulSealsAction(raceId);
}

void stashStowItem(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendStashStow(position, itemId, count, stackpos, action);
}

void stashWithdraw(uint16_t itemId, uint32_t count, uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendStashWithdraw(itemId, count, stackpos);
}
void stop()
{
    if (!canPerformGameAction())
        return;

    // STOP_ATTACKING_FOLLOWING_WALKING: clear attack, follow and the path, then Cancel.
    gameSessionState().cancelAttackAndFollow();
}
void talkChannel(const tibia::client::MessageMode mode, const uint16_t channelId, const std::string_view message)
{
    if (!canPerformGameAction() || message.empty())
        return;

    gameclient::sendTalk(mode, channelId, "", message);
}
void talkPrivate(const tibia::client::MessageMode mode, const std::string_view receiver, const std::string_view message)
{
    if (!canPerformGameAction() || receiver.empty() || message.empty())
        return;

    gameclient::sendTalk(mode, 0, receiver, message);
}

void transferCoins(std::string_view recipient, uint32_t amount)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTransferCoins(recipient, amount);
}
void useInventoryItem(const uint16_t itemId)
{
    if (!canPerformGameAction() || !tibia::game::isValidDatId(itemId, ThingCategoryItem))
        return;

    const auto& pos = Position(0xFFFF, 0, 0); // means that is a item in inventory
    gameclient::sendUseItem(pos, itemId, 0, gameSessionState().findEmptyContainerId());

    tibia::game::notify("onUse", pos, itemId, 0, 0);
}
void useMapObjectOnCreature(const Position& from, const uint16_t fromId, const int fromStack, const uint32_t creatureId)
{
    if (!canPerformGameAction() || !from.isValid() || fromId == 0 || fromStack < 0 || !creatureId)
        return;
    gameclient::sendUseOnCreature(from, fromId, static_cast<uint8_t>(fromStack), creatureId);
}
void useWithMapObject(const Position& from, const uint16_t fromId, const int fromStack, const Position& to, const uint16_t toId, const int toStack)
{
    if (!canPerformGameAction() || !from.isValid() || fromId == 0 || fromStack < 0 || !to.isValid() || toId == 0
        || toStack < 0)
        return;
    gameclient::sendUseItemWith(from, fromId, static_cast<uint8_t>(fromStack), to, toId,
                                    static_cast<uint8_t>(toStack));
    tibia::game::notify("onUseWith", from, fromId, to, fromStack);
}
void wrapMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    gameclient::sendOnWrapItem(position, itemId, static_cast<uint8_t>(stackPos));
}

int getProtocolVersion()
{
    return gameSessionState().getProtocolVersion();
}

bool isUsingProtobuf()
{
    return gameSessionState().isUsingProtobuf();
}

int getPing()
{
    return gameSessionState().getPing();
}

uint32_t getFollowingCreatureId()
{
    return gameSessionState().getFollowingCreatureId();
}

void setCanReportBugs(const bool enable)
{
    gameSessionState().setCanReportBugs(enable);
}

bool canExivaOptions()
{
    return gameSessionState().canExivaOptions();
}

void setCanExivaOptions(const bool enable)
{
    gameSessionState().setCanExivaOptions(enable);
}

std::string getWorldName()
{
    return gameSessionState().getWorldName();
}

uint8_t getWalkMaxSteps()
{
    return gameSessionState().getWalkMaxSteps();
}

void setWalkMaxSteps(const uint8_t v)
{
    gameSessionState().setWalkMaxSteps(v);
}

int getBugReportState()
{
    return gameSessionState().getBugReportState();
}

void setBugReportState(const int state)
{
    gameSessionState().setBugReportState(state);
}

UnjustifiedPoints getUnjustifiedPoints()
{
    return gameSessionState().getUnjustifiedPoints();
}

void setUnjustifiedPoints(const UnjustifiedPoints unjustifiedPoints)
{
    gameSessionState().setUnjustifiedPoints(unjustifiedPoints);
}

uint32_t getOpenPvpSituations()
{
    return gameSessionState().getOpenPvpSituations();
}

void setOpenPvpSituations(const uint8_t openPvpSituations)
{
    gameSessionState().setOpenPvpSituations(openPvpSituations);
}

uint16_t getServerBeat()
{
    return gameSessionState().getServerBeat();
}

void setServerBeat(const uint16_t beat)
{
    gameSessionState().setServerBeat(beat);
}

void setExpertPvpMode(const bool enable)
{
    gameSessionState().setExpertPvpMode(enable);
}

bool getExpertPvpMode()
{
    return gameSessionState().getExpertPvpMode();
}

void setNextTalkAim(const Position& pos)
{
    gameSessionState().setNextTalkAim(pos);
}

void setNextTalkAimMode(const uint8_t mode)
{
    gameSessionState().setNextTalkAimMode(mode);
}

Position takeNextTalkAim()
{
    return gameSessionState().takeNextTalkAim();
}

uint8_t takeNextTalkAimMode()
{
    return gameSessionState().takeNextTalkAimMode();
}

void resetMapUpdatedAt()
{
    gameSessionState().resetMapUpdatedAt();
}

void updateMapLatency()
{
    gameSessionState().updateMapLatency();
}

void requestStop()
{
    gameSessionState().requestStop();
}


} // namespace game
} // namespace tibia
