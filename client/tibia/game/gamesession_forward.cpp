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
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "network/networkqualityprotocolmessagehandler.h"

namespace tibia {
namespace game {

namespace gameclient = ::tibia::protocol::gameclient;

namespace {

/// The current session's cyclopedia protocol handler; null between sessions.
cyclopedia::TCyclopediaProtocolMessageHandler* cyclopediaHandler()
{
    TGameSessionBase* session = currentGameSession();
    return session ? session->cyclopediaProtocolMessageHandler().get() : nullptr;
}

} // namespace

void acceptTrade()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendAcceptTrade();
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
bool canPerformGameAction()
{
    return gameSessionState().canPerformGameAction();
}

void clearImbuement(uint8_t slot)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendClearImbuement(slot);
}
void closeImbuingWindow()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCloseImbuingWindow();
}

void confirmCharacterTradeConfiguration(uint32_t price, uint32_t endTimestamp, const std::vector<std::tuple<uint16_t, uint8_t>>& items, const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems, const std::vector<uint16_t>& highlights, bool createAuction)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCharacterTradeConfigurationAction(createAuction ? 3 : 2, price, endTimestamp, items,
                                                          storeItems, highlights);
}

void editGuildMessage(std::string_view text)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendEditGuildMessage(text);
}

void editText(uint32_t id, std::string_view text)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendEditText(id, text);
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

void inspectTrade(bool counterOffer, uint8_t index)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendInspectTrade(counterOffer, index);
}

void inspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount)
{
    if (!canPerformGameAction())
        return;
    if (inspectionType == tibia::client::INSPECT_CYCLOPEDIA) {
        if (auto* handler = cyclopediaHandler())
            handler->sendInspectCyclopediaObject(itemId);
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

void requestBestiary()
{
    if (!canPerformGameAction())
        return;
    if (auto* handler = cyclopediaHandler())
        handler->requestMonsterCyclopedia();
}

void requestBestiaryOverview(std::string_view catName, bool search, std::vector<uint16_t> raceIds)
{
    if (!canPerformGameAction())
        return;
    auto* handler = cyclopediaHandler();
    if (!handler)
        return;
    if (search)
        handler->requestMonsterCyclopediaMonsters(raceIds);
    else
        handler->requestMonsterCyclopediaMonsters(QString::fromUtf8(catName.data(), static_cast<qsizetype>(catName.size())));
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

void requestCharacterTradeConfiguration(uint8_t action)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendCharacterTradeConfigurationAction(action);
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

void requestSendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId)
{
    if (!canPerformGameAction())
        return;
    auto* handler = cyclopediaHandler();
    if (!handler)
        return;
    switch (action) {
        case 0: handler->sendUnlockCharm(runeId); break;
        case 1: handler->sendAssignCharm(runeId, raceId); break;
        case 2: handler->sendRemoveCharm(runeId); break;
        case 3: handler->sendResetAllCharms(); break;
        default: break;
    }
}

void requestSendCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage, uint16_t page)
{
    if (!canPerformGameAction())
        return;
    auto* handler = cyclopediaHandler();
    if (!handler)
        return;
    if (characterInfoType == tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS ||
        characterInfoType == tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS)
        handler->sendOpenCharacterInfo(playerId, characterInfoType, entriesPerPage, page);
    else
        handler->sendOpenCharacterInfo(playerId, characterInfoType);
}

void requestSetCharacterTitle(uint8_t titleId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendFriendSystemAction(0x0E, titleId);
}

void requestTransactionDetails(uint32_t transactionId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendGetTransactionDetails(transactionId);
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
void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount)
{
    if (!canPerformGameAction())
        return;
    if (auto* handler = cyclopediaHandler())
        handler->sendMapDonation(areaId, amount);
}

void sendCyclopediaMapResetViewpoints(uint16_t subAreaId)
{
    if (!canPerformGameAction())
        return;
    if (auto* handler = cyclopediaHandler())
        handler->sendResetMapAreaViewpoints(subAreaId);
}

void sendCyclopediaMapRequestArea(uint16_t subAreaId)
{
    if (!canPerformGameAction())
        return;
    if (auto* handler = cyclopediaHandler())
        handler->sendRequestMapArea(subAreaId);
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
    if (auto* handler = cyclopediaHandler())
        handler->sendPartyHuntLootType(lootType);
}

void sendPartyAnalyzerReset()
{
    if (!canPerformGameAction())
        return;
    if (auto* handler = cyclopediaHandler())
        handler->sendResetPartyHuntAnalyser();
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

void soulsealFightAction(const uint16_t raceId)
{
    if (!canPerformGameAction() || std::cmp_equal(raceId, 0))
        return;

    gameclient::sendSoulSealsAction(raceId);
}

void talkChannel(const tibia::client::MessageMode mode, const uint16_t channelId, const std::string_view message)
{
    if (!canPerformGameAction() || message.empty())
        return;

    gameclient::sendTalk(mode, channelId, "", message);
}
int getPing()
{
    // The live game server session's round trip time; -1 without one.
    const auto* session = dynamic_cast<const TGameserverGameSession*>(currentGameSession());
    return session ? int(session->latency().milliseconds) : -1;
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

int getBugReportState()
{
    return gameSessionState().getBugReportState();
}

void setBugReportState(const int state)
{
    gameSessionState().setBugReportState(state);
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

void updateMapLatency()
{
    gameSessionState().updateMapLatency();
}

} // namespace game
} // namespace tibia
