/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "gamehub_impl.h"
#include "tibia/runtime/host.h"
#include <tibia/game/sessionhooks.h>
#include "tibia/gamewindow/remainingdialogs.h"

#include <framework/core/gamehooks.h>

#include <tibia/creatures/attachedeffect.h>
#include <tibia/config/runtimegameconfig.h>
#include "client/item.h"
#include <tibia/game/localplayersessionthing.h>
#include "game/localplayergopath.h"
#include <tibia/game/mapsession.h>
#include "client/protocolgame.h"
#include "protocol/protocolcodes.h"
#include "tibia/protocol/extendedopcode.h"
#include "tibia/assets/thingtype.h"
#include "tibia/assets/things.h"
#include "worldmap/worldmapfield.h"
#include "container/containerciptypes.h"
#include "creatures/creaturestorage.h"
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gameservergamesession.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/mapsession.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/game/tgamenotify.h>

#include <QString>
#include <tibia/game/thingsession_forward.h>

Game s_qtclientGameHub;

Game::~Game() = default;

void Game::init()
{
    resetGameStates();
}

namespace {

void bindExtendedOpcodeSender(const ProtocolGamePtr& protocol)
{
    if (!protocol) {
        tibia::protocol::ExtendedOpcodeReader::instance().setSender(nullptr);
        return;
    }
    tibia::protocol::ExtendedOpcodeReader::instance().setSender([protocol](const uint8_t opcode, const std::string& buffer) {
        if (!protocol->isConnected())
            return false;
        protocol->sendExtendedOpcode(opcode, buffer);
        return true;
    });
}

}

void Game::terminate()
{
    resetGameStates();
    bindExtendedOpcodeSender(nullptr);
    m_protocolGame = nullptr;
}

void Game::rebindLocalPlayerGoPath()
{
    m_localPlayerGoPath.reset();
    if (m_localPlayer)
        m_localPlayerGoPath = std::make_unique<LocalPlayerGoPath>(*m_localPlayer);
}

void Game::resetGameStates()
{
    m_online = false;
    m_dead = false;
    m_sceneFrozen = false;
    m_serverBeat = 50;
    m_seq = 0;
    m_ping = -1;
    m_mapUpdatedAt = 0;
    m_mapUpdateTimer = { true, Timer{} };
    setCanReportBugs(false);
    setBugReportState(-1);
    setCanExivaOptions(false);
    m_fightMode = tibia::client::FightBalanced;
    m_chaseMode = tibia::client::DontChase;
    m_pvpMode = tibia::client::WhiteDove;
    m_safeFight = true;
    m_followingCreatureId = 0;
    m_attackingCreatureId = 0;
    m_localPlayerGoPath.reset();
    m_localPlayer = nullptr;
    m_pingSent = 0;
    m_pingReceived = 0;
    m_unjustifiedPoints = UnjustifiedPoints();

    if (m_pingEvent) {
        m_pingEvent->cancel();
        m_pingEvent = nullptr;
    }

    if (m_checkConnectionEvent) {
        m_checkConnectionEvent->cancel();
        m_checkConnectionEvent = nullptr;
    }

    m_vips.clear();
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->clear();
    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance())
        inventory->clear();
    tibia::game::resetAwareRange();
}

void Game::processConnectionError(const std::error_code& ec)
{
    // connection errors only have meaning if we still have a protocol
    if (m_protocolGame) {
        // eof = end of file, a clean disconnect
        if (ec != gameConnectionEof())
            tibia::game::notify("onConnectionError", ec.message(), ec.value());

        processDisconnect();
    }
}

void Game::processDisconnect()
{
    if (isOnline() || m_switchingCharacter) {
        m_switchingCharacter = false;
        processGameEnd();
    }

    if (m_protocolGame) {
        m_protocolGame->disconnect();
        bindExtendedOpcodeSender(nullptr);
        m_protocolGame = nullptr;
    }
}

void Game::processUpdateNeeded(const std::string_view signature)
{
    tibia::game::notify("onUpdateNeeded", signature);
}

void Game::processLoginError(const std::string_view error)
{
    tibia::game::TGameserverGameSession::wireProcessLoginError(error);
}

void Game::processLoginAdvice(const std::string_view message)
{
    tibia::game::TGameserverGameSession::wireProcessLoginAdvice(message);
}

void Game::processLoginWait(const std::string_view message, const uint8_t time)
{
    tibia::game::TGameserverGameSession::wireProcessLoginWait(message, time);
}

void Game::processSessionEnd(const uint8_t reason)
{
    tibia::game::TGameserverGameSession::wireProcessSessionEnd(reason);
}

void Game::processSessionDumpStart(const std::string_view sessionKey)
{
    tibia::game::notify("onSessionDumpStart", std::string(sessionKey));
}

void Game::processSessionDumpMarker(const std::string_view marker)
{
    tibia::game::notify("onSessionDumpMarker", std::string(marker));
}

void Game::processSoundTrigger(const uint8_t type, const uint16_t soundId)
{
    tibia::game::notify("onSoundTrigger", type, soundId);
}

void Game::processLogin()
{
    tibia::game::TGameserverGameSession::wireProcessLogin();
}

void Game::processPendingGame()
{
    m_localPlayer->setPendingGame(true);
    tibia::game::notify("onPendingGame");
    m_protocolGame->sendEnterGame();
}

void Game::processEnterGame()
{
    tibia::schedule::post([localPlayer = m_localPlayer] {
        localPlayer->setPendingGame(false);
    });
    tibia::game::notify("onEnterGame");
}

void Game::processWorldEntered()
{
    tibia::game::TGameserverGameSession::wireProcessWorldEntered();
}

void Game::processGameStart()
{
    m_online = true;
    tibia::host::app().resetTargetFps();

    // synchronize fight modes with the server
    m_protocolGame->sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);

    // An in-flight ping (kept across alt switch) reschedules the loop from processPingBack.
    if ((getFeature(tibia::client::GameClientPing) || getFeature(tibia::client::GameExtendedClientPing)) && m_pingSent == m_pingReceived) {
        m_pingEvent = tibia::schedule::after([this] { ping(); }, m_pingDelay);
    }

    m_checkConnectionEvent = tibia::schedule::every([this] {
        if (!isConnectionOk() && !m_connectionFailWarned) {
            tibia::game::notify("onConnectionFailing", true);
            tibia::game::TGameNotify::instance()->emitConnectionFailing(true);
            m_connectionFailWarned = true;
        } else if (isConnectionOk() && m_connectionFailWarned) {
            tibia::game::notify("onConnectionFailing", false);
            tibia::game::TGameNotify::instance()->emitConnectionFailing(false);
            m_connectionFailWarned = false;
        }
    }, 1000);

    tibia::game::notify("onGameStart");
    tibia::game::TGameNotify::instance()->emitGameStarted();

    if (m_switchingCharacter) {
        m_switchingCharacter = false;
        tibia::game::notify("onCharacterSwitched", m_characterName);
        tibia::game::TGameNotify::instance()->emitCharacterSwitched(QString::fromStdString(m_characterName));
    }
}

void Game::processCharacterSwitch(const std::string_view characterName)
{
    // CipSoft session end: modules reset on onGameEnd, but the socket stays open and
    // logout-only UI (character list, background) checks isSwitchingCharacter.
    m_switchingCharacter = true;
    endSession();

    // Same socket: a ping sent before the switch still gets its reply, so keep the counters paired.
    const auto pingSent = m_pingSent;
    const auto pingReceived = m_pingReceived;
    resetGameStates();
    m_pingSent = pingSent;
    m_pingReceived = pingReceived;
    tibia::game::cleanMapDynamicThings();

    m_localPlayer = std::make_shared<LocalPlayerSessionThing>();
    rebindLocalPlayerGoPath();
    m_localPlayer->onCreate();
    m_localPlayer->setName(characterName);
    m_characterName = characterName;
}

void Game::endSession()
{
    m_online = false;
    tibia::game::notify("onGameEnd");
    tibia::game::TGameNotify::instance()->emitGameEnded();

    if (m_connectionFailWarned) {
        tibia::game::notify("onConnectionFailing", false);
        tibia::game::TGameNotify::instance()->emitConnectionFailing(false);
        m_connectionFailWarned = false;
    }
}

void Game::processGameEnd()
{
    tibia::world::notifySessionEnd();
    // FPS fixed at 60 for when UI is rendering alone.
    tibia::host::app().setTargetFps(60u);

    endSession();

    // reset game state
    resetGameStates();

    m_worldName = "";
    m_characterName = "";

    // clean map creatures
    tibia::game::cleanMapDynamicThings();
}

void Game::processDeath(const uint8_t deathType, const uint8_t penality)
{
    tibia::game::TGameserverGameSession::wireProcessDeath(deathType, penality);
}

void Game::setSceneFrozen(const bool frozen)
{
    if (m_sceneFrozen == frozen)
        return;

    m_sceneFrozen = frozen;
}

void Game::processReadyForSecondaryConnection(const std::string_view token)
{
    m_secondaryConnectionToken = std::string(token);
    tibia::game::notify("onReadyForSecondaryConnection", m_secondaryConnectionToken);
}

void Game::processOutfitMemorial(const OutfitMemorialData& data)
{
    if (auto* dialog = tibia::gamewindow::TOutfitMemorialDialogController::instance())
        dialog->showMemorial(data);
    else
        tibia::game::notify("onOutfitMemorial", data);
}

void Game::processPlayerHelpers(const uint16_t helpers)
{
    tibia::game::notify("onPlayerHelpersUpdate", helpers);
}

void Game::processTeamFinderLeader(const TeamFinderLeaderData& data)
{
    tibia::game::notify("onTeamFinderLeader", data);
}

void Game::processTeamFinderList(const bool rateLimited, const std::vector<TeamFinderListEntry>& entries)
{
    tibia::game::notify("onTeamFinderList", rateLimited, entries);
}

void Game::processDepotSearchResults(const std::vector<DepotSearchResultItem>& items)
{
    tibia::game::notify("onDepotSearchResults", items);
}

void Game::processDepotSearchDetail(const DepotSearchDetail& detail)
{
    tibia::game::notify("onDepotSearchDetail", detail);
}

void Game::processCloseDepotSearch()
{
    tibia::game::notify("onCloseDepotSearch");
}

void Game::processPlayerModes(const tibia::client::FightModes fightMode, const tibia::client::ChaseModes chaseMode, const bool safeMode, const tibia::client::PVPModes pvpMode)
{
    m_fightMode = fightMode;
    m_chaseMode = chaseMode;
    m_safeFight = safeMode;
    m_pvpMode = pvpMode;

    tibia::game::notify("onFightModeChange", fightMode);
    tibia::game::notify("onChaseModeChange", chaseMode);
    tibia::game::notify("onSafeFightChange", safeMode);
    tibia::game::notify("onPVPModeChange", pvpMode);
}

void Game::processPing()
{
    tibia::game::TGameserverGameSession::wireProcessPing();
}

void Game::processPingBack()
{
    tibia::game::TGameserverGameSession::wireProcessPingBack();
}

void Game::processTextMessage(const tibia::client::MessageMode mode, const std::string_view text)
{
    tibia::game::notify("onTextMessage", mode, text);
    tibia::game::TGameNotify::instance()->emitTextMessage(static_cast<int>(mode),
                                                          QString::fromUtf8(text.data(), static_cast<int>(text.size())));
}

void Game::processNumericalEffect(const tibia::client::MessageMode mode, const Position& pos, const uint64_t value, const uint8_t color)
{
    tibia::game::notify("onNumericalEffect", mode, pos, value, color);
}

void Game::processTalk(const std::string_view name, const uint16_t level, const tibia::client::MessageMode mode, const std::string_view text, const uint16_t channelId, const Position& pos, const uint32_t statement)
{
    tibia::game::notify("onTalk", name, level, mode, text, channelId, pos, statement);
}

void Game::processOpenContainer(const uint8_t containerId, const tibia::container::TInventorySlot& containerItem,
                                const std::string_view name, const uint8_t capacity, const bool hasParent,
                                const std::vector<tibia::container::TInventorySlot>& items, const bool isUnlocked,
                                const bool hasPages, const bool showSearch, const uint16_t containerSize,
                                const uint16_t firstIndex)
{
    tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance();
    if (!storage)
        return;

    // Replace-open: applyOpen emits containerUpdated(id, true); UI resets in place
    // (no separate remove). Matches former onOpen-then-previous-onClose ordering.
    storage->applyOpen(containerId, capacity, QString::fromUtf8(name.data(), static_cast<int>(name.size())),
                       containerItem, hasParent, isUnlocked, hasPages, showSearch, containerSize, firstIndex, items);
}

void Game::processCloseContainer(const uint8_t containerId)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->closeContainer(containerId);
}

void Game::processContainerAddItem(const uint8_t containerId, const tibia::container::TInventorySlot& item,
                                   const uint16_t slot)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->applyAddItem(containerId, item, slot);
}

void Game::processContainerUpdateItem(const uint8_t containerId, const uint16_t slot,
                                      const tibia::container::TInventorySlot& item)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->applyUpdateItem(containerId, slot, item);
}

void Game::processContainerRemoveItem(const uint8_t containerId, const uint16_t slot,
                                      const tibia::container::TInventorySlot& lastItem)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->applyRemoveItem(containerId, slot, lastItem);
}

void Game::processInventoryChange(const uint8_t slot, const tibia::container::TInventorySlot& item)
{
    tibia::container::TInventorySlot row = item;
    if (!row.empty() && getFeature(tibia::client::GameThingClock) && row.durationSeconds > 0) {
        ThingType* type = row.appearance ? row.appearance->thingType()
                                         : tibia::game::rawThingType(row.typeId, ThingCategoryItem);
        // expirestop-only items are paused server-side when equipped in their cloth slot.
        if (type && type->getClothSlot() == static_cast<int>(slot)
            && (type->hasExpire() || type->hasClockExpire())) {
            row.decaying = true;
            row.durationEndMs = g_clock.millis() + static_cast<int64_t>(row.durationSeconds) * 1000;
        }
    }

    // Phase 5.0-D: CIP inventory storage is authority (no LocalPlayerSessionThing ItemPtr array).
    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance()) {
        if (!row.empty())
            inventory->setSlot(slot, row);
        else
            inventory->clearSlot(slot);
    }

    if (m_localPlayer)
        m_localPlayer->setInventoryItem(static_cast<tibia::client::InventorySlot>(slot));
}

void Game::processChannelList(const std::vector<std::tuple<uint16_t, std::string>>& channelList)
{
    tibia::game::notify("onChannelList", channelList);
}

void Game::processOpenChannel(const uint16_t channelId, const std::string_view name)
{
    tibia::game::notify("onOpenChannel", channelId, name);
}

void Game::processOpenPrivateChannel(const std::string_view name)
{
    tibia::game::notify("onOpenPrivateChannel", name);
}

void Game::processOpenOwnPrivateChannel(const uint16_t channelId, const std::string_view name)
{
    tibia::game::notify("onOpenOwnPrivateChannel", channelId, name);
}

void Game::processCloseChannel(const uint16_t channelId)
{
    tibia::game::notify("onCloseChannel", channelId);
}

void Game::processEditGuildMessage(const std::string_view text)
{
    tibia::game::notify("onEditGuildMessage", text);
}

void Game::processVipAdd(const uint32_t id, const std::string_view name, const uint32_t status, const std::string_view description, const uint32_t iconId, const bool notifyLogin, const std::vector<uint8_t>& groupID)
{
    m_vips[id] = Vip(name, status, description, iconId, notifyLogin, groupID);
    tibia::game::notify("onAddVip", id, name, status, description, iconId, notifyLogin, groupID);
}

void Game::processVipStateChange(const uint32_t id, const uint32_t status)
{
    std::get<1>(m_vips[id]) = status;
    const std::vector<uint8_t>& groupID = std::get<5>(m_vips[id]);
    tibia::game::notify("onVipStateChange", id, status, groupID);
}

void Game::processVipGroupChange(const std::vector<std::tuple<uint8_t, std::string, bool>>& vipGroups, const uint8_t groupsAmountLeft)
{
    tibia::game::notify("onVipGroupChange", vipGroups, groupsAmountLeft);
}

void Game::processTutorialHint(const uint8_t id)
{
    tibia::game::notify("onTutorialHint", id);
}

void Game::processAddAutomapFlag(const Position& pos, const uint8_t icon, const std::string_view message)
{
    tibia::game::notify("onAddAutomapFlag", pos, icon, message);
}

void Game::processRemoveAutomapFlag(const Position& pos, const uint8_t icon, const std::string_view message)
{
    tibia::game::notify("onRemoveAutomapFlag", pos, icon, message);
}

void Game::processOpenOutfitWindow(const Outfit& currentOutfit, const std::vector<std::tuple<uint16_t, std::string, uint8_t, uint8_t>>& outfitList,
                                   const std::vector<std::tuple<uint16_t, std::string, uint8_t>>& mountList,
                                   const std::vector<std::tuple<uint16_t, std::string>>& familiarList,
                                   const std::vector<std::tuple<uint16_t, std::string>>& wingsList,
                                   const std::vector<std::tuple<uint16_t, std::string>>& aurasList,
                                   const std::vector<std::tuple<uint16_t, std::string>>& effectList,
                                   const std::vector<std::tuple<uint16_t, std::string>>& shaderList)
{
    tibia::game::notify("onOpenOutfitWindow", currentOutfit, outfitList, mountList, familiarList, wingsList, aurasList, effectList, shaderList);
}

void Game::processOpenNpcTrade(
    const std::vector<std::tuple<tibia::container::TInventorySlot, std::string, uint32_t, uint32_t, uint32_t>>& items)
{
    tibia::game::notify("onOpenNpcTrade", items);
}

void Game::processPlayerGoods(const uint64_t money, const std::vector<std::pair<uint16_t, uint16_t>>& goods)
{
    tibia::game::notify("onPlayerGoods", money, goods);
}

void Game::processCloseNpcTrade()
{
    tibia::game::notify("onCloseNpcTrade");
}

void Game::processOwnTrade(const std::string_view name, const std::vector<tibia::container::TInventorySlot>& items)
{
    tibia::game::notify("onOwnTrade", name, items);
}

void Game::processCounterTrade(const std::string_view name, const std::vector<tibia::container::TInventorySlot>& items)
{
    tibia::game::notify("onCounterTrade", name, items);
}

void Game::processCloseTrade()
{
    tibia::game::notify("onCloseTrade");
}

void Game::processEditText(const uint32_t id, const uint32_t itemId, const uint16_t maxLength, const std::string_view text, const std::string_view writer, const std::string_view date, const bool recentlyTraded)
{
    tibia::game::notify("onEditText", id, itemId, maxLength, text, writer, date, recentlyTraded);
}

void Game::processEditList(const uint32_t id, const uint8_t doorId, const std::string_view text)
{
    tibia::game::notify("onEditList", id, doorId, text);
}

void Game::processQuestLog(const std::vector<std::tuple<uint16_t, std::string, bool>>& questList)
{
    tibia::game::notify("onQuestLog", questList);
}

void Game::processQuestLine(const uint16_t questId, const std::vector<std::tuple<std::string, std::string, uint16_t>>& questMissions)
{
    tibia::game::notify("onQuestLine", questId, questMissions);
}

void Game::processModalDialog(const uint32_t id, const std::string_view title, const std::string_view message, const std::vector<std::tuple<uint8_t, std::string>>
                                & buttonList, const uint8_t enterButton, const uint8_t escapeButton, const std::vector<std::tuple<uint8_t, std::string>>
                                & choiceList, const bool priority)
{
    tibia::game::notify("onModalDialog", id, title, message, buttonList, enterButton, escapeButton, choiceList, priority);
}

void Game::processItemDetail(const ItemInspectionData& data)
{
    tibia::game::notify("onParseItemDetail", data);
}

void Game::processCyclopediaCharacterGeneralStats(const CyclopediaCharacterGeneralStats& stats, const std::vector<std::vector<uint16_t>>& skills,
                                                const std::vector<std::tuple<uint8_t, uint16_t>>& combats)
{
    tibia::game::notify("onParseCyclopediaCharacterGeneralStats", stats, skills, combats);
}

void Game::processCyclopediaCharacterCombatStats(const CyclopediaCharacterCombatStats& data, const double mitigation, const std::vector<std::vector<uint16_t>>& additionalSkillsArray,
                                                const std::vector<std::vector<uint16_t>>& forgeSkillsArray, const std::vector<uint16_t>& perfectShotDamageRangesArray,
                                                const std::vector<std::tuple<uint8_t, uint16_t>>& combatsArray, const std::vector<std::tuple<uint16_t, uint16_t>>& concoctionsArray)
{
    tibia::game::notify("onParseCyclopediaCharacterCombatStats", data, mitigation, additionalSkillsArray, forgeSkillsArray, perfectShotDamageRangesArray, combatsArray, concoctionsArray);
}

void Game::processCyclopediaCharacterGeneralStatsBadge(const uint8_t showAccountInformation, const uint8_t playerOnline, const uint8_t playerPremium,
                                                const std::string_view loyaltyTitle, const std::vector<std::tuple<uint32_t, std::string>>& badgesVector)
{
    tibia::game::notify("onParseCyclopediaCharacterBadges", showAccountInformation, playerOnline, playerPremium, loyaltyTitle, badgesVector);
}

void Game::processCyclopediaCharacterInspection(const CyclopediaCharacterInspection& data)
{
    tibia::game::notify("onParseCyclopediaCharacterInspection", data);
}

void Game::processInspectionState(const uint32_t creatureId, const uint8_t state)
{
    tibia::game::notify("onInspectionState", creatureId, state);
}

void Game::processCyclopediaCharacterItemSummary(const CyclopediaCharacterItemSummary& data)
{
    tibia::game::notify("onUpdateCyclopediaCharacterItemSummary", data);
}

void Game::processCyclopediaCharacterAppearances(const OutfitColorStruct& currentOutfit, const std::vector<CharacterInfoOutfits>& outfits,
                                                const std::vector<CharacterInfoMounts>& mounts, const std::vector<CharacterInfoFamiliar>& familiars)
{
    tibia::game::notify("onParseCyclopediaCharacterAppearances", currentOutfit, outfits, mounts, familiars);
}

void Game::processCyclopediaCharacterRecentDeaths(const CyclopediaCharacterRecentDeaths& data)
{
    tibia::game::notify("onCyclopediaCharacterRecentDeaths", data);
}

void Game::processCyclopediaCharacterRecentPvpKills(const CyclopediaCharacterRecentPvPKills& data)
{
    tibia::game::notify("onCyclopediaCharacterRecentKills", data);
}

void Game::processBosstiaryInfo(const std::vector<BosstiaryData>& boss)
{
    tibia::game::notify("onParseSendBosstiary", boss);
}

void Game::processBosstiarySlots(const BosstiarySlotsData& data)
{
    tibia::game::notify("onParseBosstiarySlots", data);
}

void Game::processParseBestiaryRaces(const std::vector<CyclopediaBestiaryRace>& bestiaryData)
{
    tibia::game::notify("onParseBestiaryRaces", bestiaryData);
}

void Game::processParseBestiaryOverview(const std::string_view raceName, const std::vector<BestiaryOverviewMonsters>& data, const uint16_t animusMasteryPoints)
{
    tibia::game::notify("onParseBestiaryOverview", raceName, data, animusMasteryPoints);
}

void Game::processUpdateBestiaryMonsterData(const BestiaryMonsterData& data)
{
    tibia::game::notify("onUpdateBestiaryMonsterData", data);
}

void Game::processUpdateBestiaryCharmsData(const BestiaryCharmsData& charmData)
{
    tibia::game::notify("onUpdateBestiaryCharmsData", charmData);
}

void Game::processAttackCancel(const uint32_t seq)
{
    if (isAttacking() && (seq == 0 || m_seq == seq)) {
        cancelAttack();
        tibia::game::notify("onAttackCancel");
    }
}

void Game::processWalkCancel(const tibia::client::Direction direction)
{
    m_localPlayer->cancelWalk(direction);
}

void Game::loginWorld(const std::string_view account, const std::string_view password, const std::string_view worldName, const std::string_view worldHost, const int worldPort, const std::string_view characterName, const std::string_view authenticatorToken, const std::string_view sessionKey, const std::string_view& recordTo)
{
    if (m_protocolGame || isOnline())
        throw Exception("Unable to login into a world while already online or logging.");

    if (m_protocolVersion == 0)
        throw Exception("Must set a valid game protocol version before logging.");

    // reset the new game state
    resetGameStates();

    m_localPlayer = std::make_shared<LocalPlayerSessionThing>();
    rebindLocalPlayerGoPath();
    m_localPlayer->onCreate();
    m_localPlayer->setName(characterName);

    m_protocolGame = std::make_shared<ProtocolGame>();
    bindExtendedOpcodeSender(m_protocolGame);
    if (!recordTo.empty())
        m_protocolGame->setRecordFile(recordTo);
    m_protocolGame->login(account, password, worldHost, static_cast<uint16_t>(worldPort), characterName, authenticatorToken, sessionKey);
    m_characterName = characterName;
    m_worldName = worldName;
}

void Game::startDecodedReplay()
{
    if (m_protocolGame || isOnline())
        throw Exception("Unable to login into a world while already online or logging.");

    if (m_protocolVersion == 0)
        throw Exception("Must set a valid game protocol version before logging.");

    resetGameStates();

    m_localPlayer = std::make_shared<LocalPlayerSessionThing>();
    rebindLocalPlayerGoPath();
    m_localPlayer->setName("Player");

    m_protocolGame = std::make_shared<ProtocolGame>();
    bindExtendedOpcodeSender(m_protocolGame);
    m_protocolGame->beginReplay();
    m_characterName = "Player";
    m_worldName = "Record";
}

void Game::deliverReplayPacket(const uint8_t* data, const size_t size)
{
    if (!m_protocolGame || !data || size == 0 || size > 0xffff)
        return;
    m_protocolGame->deliverParserBytes(data, static_cast<uint16_t>(size));
}

void Game::finishDecodedReplay()
{
    if (m_protocolGame)
        m_protocolGame->fail({}, true);
}

void Game::cancelLogin()
{
    // send logout even if the game has not started yet, to make sure that the player doesn't stay logged there
    if (m_protocolGame)
        m_protocolGame->sendLogout();

    processDisconnect();
}

void Game::forceLogout()
{
    if (!isOnline())
        return;

    m_protocolGame->sendLogout();
    processDisconnect();
}

void Game::safeLogout()
{
    if (!isOnline())
        return;

    m_protocolGame->sendLogout();
}

bool Game::walk(const tibia::client::Direction direction)
{
    if (!canPerformGameAction() || direction == tibia::client::InvalidDirection)
        return false;

    // CipSoft TPlayerMovementGameActionHandler : direction intent
    // publishes stop-auto-walk (0x94) + APPLY_AUTOCHASE before the step.
    interruptAutoWalk();

    tibia::game::notify("onWalk", direction);

    // CipSoft client prediction: start TCreatureMovementTween before the wire
    // MoveCreature confirms (gamesession → storage; no Map Creature clock).
    tibia::game::predictLocalPlayerStep(direction);

    if (m_mapUpdateTimer.first || m_localPlayer->getPreWalkingSize() == 1) {
        m_mapUpdateTimer.second.restart();
        m_mapUpdateTimer.first = false;
    }

    switch (direction) {
        case tibia::client::North:
            m_protocolGame->sendWalkNorth();
            break;
        case tibia::client::East:
            m_protocolGame->sendWalkEast();
            break;
        case tibia::client::South:
            m_protocolGame->sendWalkSouth();
            break;
        case tibia::client::West:
            m_protocolGame->sendWalkWest();
            break;
        case tibia::client::NorthEast:
            m_protocolGame->sendWalkNorthEast();
            break;
        case tibia::client::SouthEast:
            m_protocolGame->sendWalkSouthEast();
            break;
        case tibia::client::SouthWest:
            m_protocolGame->sendWalkSouthWest();
            break;
        case tibia::client::NorthWest:
            m_protocolGame->sendWalkNorthWest();
            break;
        default:
            break;
    }

    return true;
}

void Game::requestStop()
{
    if (!canPerformGameAction() || !m_protocolGame)
        return;
    m_protocolGame->sendStop();
}

void Game::interruptAutoWalk()
{
    if (!m_localPlayer)
        return;
    const bool hadPath =
        tibia::game::localPlayerIsAutoWalking() || tibia::game::localPlayerHasAutoWalkDirs();
    // Client clear + Target; sendStop only when a GoPath was live.
    tibia::game::localPlayerStopAutoWalk();
    if (isFollowing())
        cancelFollow();
    if (hadPath)
        requestStop();
}

void Game::autoWalk(const std::vector<tibia::client::Direction>& dirs, const Position& startPos)
{
    if (!canPerformGameAction())
        return;

    if (dirs.size() == 0)
        return;

    // protocol limits walk path
    if (dirs.size() > 127) {
        g_logger.error("Auto walk path too great");
        return;
    }

    // must cancel follow before any new walk
    if (isFollowing()) {
        cancelFollow();
    }

    // CIP GoPath: arm remaining dirs and predict the first step while idle so
    // matching MoveCreature confirms the in-flight TCreatureMovementTween.
    // Further steps are chained at tween end / NFU (gamesession helpers).
    if (startPos == m_localPlayer->getServerPosition()) {
        tibia::game::armLocalPlayerGoPathDirs(dirs);
        tibia::game::localPlayerTryBeginNextGoPathStep();
    }

    tibia::game::notify("onAutoWalk", m_localPlayer, dirs);
    m_protocolGame->sendAutoWalk(dirs);
}

void Game::turn(const tibia::client::Direction direction)
{
    if (!canPerformGameAction())
        return;

    switch (direction) {
        case tibia::client::North:
            m_protocolGame->sendTurnNorth();
            break;
        case tibia::client::East:
            m_protocolGame->sendTurnEast();
            break;
        case tibia::client::South:
            m_protocolGame->sendTurnSouth();
            break;
        case tibia::client::West:
            m_protocolGame->sendTurnWest();
            break;
        default:
            break;
    }
}

void Game::stop()
{
    if (!canPerformGameAction())
        return;

    // qtclient STOP_ATTACKING_FOLLOWING_WALKING: clear attack, follow, and auto-walk.
    if (isAttacking())
        cancelAttack();
    if (isFollowing())
        cancelFollow();
    tibia::game::localPlayerStopAutoWalk();

    m_protocolGame->sendStop();
}

void Game::lookCreature(const uint32_t creatureId, const bool isBattleList)
{
    if (!canPerformGameAction() || !creatureId)
        return;

    if (isBattleList && m_protocolVersion >= 961) {
        m_protocolGame->sendLookCreature(creatureId);
        return;
    }

    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> row =
        storage ? storage->creatureAt(creatureId) : nullptr;
    if (!row)
        return;

    int stackPos = 0;
    if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(row->position())) {
        for (const auto& entry : tile->stackCreatureEntries()) {
            if (entry.creatureId == creatureId) {
                stackPos = entry.stackIndex;
                break;
            }
        }
    }
    m_protocolGame->sendLook(row->position(), static_cast<int>(Proto::Creature), static_cast<uint8_t>(stackPos));
}

void Game::lookMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    m_protocolGame->sendLook(position, itemId, static_cast<uint8_t>(stackPos));
}

void Game::moveMapObject(const Position& from, const uint16_t itemId, const int stackPos, const Position& toPos,
                         int count)
{
    if (count <= 0)
        count = 1;
    if (!canPerformGameAction() || !from.isValid() || itemId == 0 || stackPos < 0 || from == toPos)
        return;
    m_protocolGame->sendMove(from, itemId, static_cast<uint8_t>(stackPos), toPos, count);
}

void Game::rotateMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    m_protocolGame->sendRotateItem(position, itemId, static_cast<uint8_t>(stackPos));
}

void Game::wrapMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    m_protocolGame->sendOnWrapItem(position, itemId, static_cast<uint8_t>(stackPos));
}

void Game::useMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    m_protocolGame->sendUseItem(position, itemId, static_cast<uint8_t>(stackPos), findEmptyContainerId());
    tibia::game::notify("onUse", position, itemId, stackPos, 0);
}

void Game::useInventoryItem(const uint16_t itemId)
{
    if (!canPerformGameAction() || !tibia::game::isValidDatId(itemId, ThingCategoryItem))
        return;

    const auto& pos = Position(0xFFFF, 0, 0); // means that is a item in inventory
    m_protocolGame->sendUseItem(pos, itemId, 0, findEmptyContainerId());

    tibia::game::notify("onUse", pos, itemId, 0, 0);
}

void Game::useWithMapObject(const Position& from, const uint16_t fromId, const int fromStack, const Position& to,
                            const uint16_t toId, const int toStack)
{
    if (!canPerformGameAction() || !from.isValid() || fromId == 0 || fromStack < 0 || !to.isValid() || toId == 0
        || toStack < 0)
        return;
    m_protocolGame->sendUseItemWith(from, fromId, static_cast<uint8_t>(fromStack), to, toId,
                                    static_cast<uint8_t>(toStack));
    tibia::game::notify("onUseWith", from, fromId, to, fromStack);
}

void Game::useInventoryItemWithMapObject(const uint16_t itemId, const Position& to, const uint16_t toId,
                                         const int toStack)
{
    if (!canPerformGameAction() || !to.isValid() || toId == 0 || toStack < 0)
        return;

    // pre-780: resolve real inventory slot when possible (classic wire stackpos).
    if (getClientVersion() < 780) {
        if (const auto& item = findPlayerItem(itemId, -1)) {
            Position pos = item->getPosition();
            if (!pos.isValid())
                pos = Position(0xFFFF, 0, 0);
            m_protocolGame->sendUseItemWith(pos, item->getId(), item->getStackPos(), to, toId,
                                            static_cast<uint8_t>(toStack));
            tibia::game::notify("onUseWith", pos, itemId, to, item->getStackPos());
            return;
        }
    }

    const auto& pos = Position(0xFFFF, 0, 0);
    m_protocolGame->sendUseItemWith(pos, itemId, 0, to, toId, static_cast<uint8_t>(toStack));
    tibia::game::notify("onUseWith", pos, itemId, to, 0);
}

void Game::useMapObjectOnCreature(const Position& from, const uint16_t fromId, const int fromStack,
                                  const uint32_t creatureId)
{
    if (!canPerformGameAction() || !from.isValid() || fromId == 0 || fromStack < 0 || !creatureId)
        return;
    m_protocolGame->sendUseOnCreature(from, fromId, static_cast<uint8_t>(fromStack), creatureId);
}

void Game::useOnCreature(const ItemPtr& item, const uint32_t creatureId)
{
    if (!canPerformGameAction() || !item || !creatureId)
        return;

    Position pos = item->getPosition();
    if (!pos.isValid())
        pos = Position(0xFFFF, 0, 0);

    m_protocolGame->sendUseOnCreature(pos, item->getId(), item->getStackPos(), creatureId);
    tibia::game::notify("onUseWith", pos, item->getId(), creatureId, item->getStackPos());
}

void Game::useInventoryItemOnCreature(const uint16_t itemId, const uint32_t creatureId)
{
    if (!canPerformGameAction() || !creatureId)
        return;

    if (getClientVersion() < 780) {
        if (const auto& item = findPlayerItem(itemId, -1)) {
            useOnCreature(item, creatureId);
            return;
        }
    }

    const auto& pos = Position(0xFFFF, 0, 0);
    m_protocolGame->sendUseOnCreature(pos, itemId, 0, creatureId);
    tibia::game::notify("onUseWith", pos, itemId, creatureId, 0);
}

ItemPtr Game::findPlayerItem(const uint32_t itemId, const int subType)
{
    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance()) {
        for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot) {
            const tibia::container::TInventorySlot& row = inventory->slotAt(slot);
            if (row.empty() || row.typeId != itemId)
                continue;
            const uint8_t fluidOrSub = row.fluidOrSplash ? row.fluid : 0;
            if (subType != -1 && fluidOrSub != static_cast<uint8_t>(subType))
                continue;
            return ephemeralItemForWire(row.typeId, Position(UINT16_MAX, static_cast<uint16_t>(slot), 0), 0,
                                        row.stackable ? row.count : 0, row.tier, fluidOrSub);
        }
    }

    return findItemInContainers(itemId, subType, 0);
}

ItemPtr Game::findItemInContainers(const uint32_t itemId, const int subType, const uint8_t tier)
{
    tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance();
    if (!storage)
        return nullptr;

    for (const tibia::container::TContainerID containerId : storage->openIds()) {
        const tibia::container::TStoredContainer* row = storage->containerAt(containerId);
        if (!row)
            continue;
        for (size_t i = 0; i < row->items.size(); ++i) {
            const tibia::container::TInventorySlot& slot = row->items[i];
            if (slot.empty() || slot.typeId != itemId || slot.tier != tier)
                continue;
            const uint8_t fluidOrSub = slot.fluidOrSplash ? slot.fluid : 0;
            if (subType != -1 && fluidOrSub != static_cast<uint8_t>(subType))
                continue;
            const int pageSlot = static_cast<int>(i);
            return ephemeralItemForWire(slot.typeId,
                                        Position(0xffff, static_cast<uint16_t>(containerId | 0x40),
                                                 static_cast<uint8_t>(pageSlot)),
                                        pageSlot, slot.stackable ? slot.count : 0, slot.tier, fluidOrSub);
        }
    }

    return nullptr;
}

int Game::openAt(const Position& position, const uint16_t itemId, const int stackPos, const int previousContainerId)
{
    if (!canPerformGameAction() || itemId == 0 || stackPos < 0)
        return -1;

    const int id = previousContainerId >= 0 ? previousContainerId : findEmptyContainerId();
    m_protocolGame->sendUseItem(position, itemId, static_cast<uint8_t>(stackPos), id);

    tibia::game::notify("onUse", position, itemId, stackPos, 0);

    return id;
}

void Game::openParentById(const int containerId)
{
    if (!canPerformGameAction() || containerId < 0)
        return;

    m_protocolGame->sendUpContainer(static_cast<uint8_t>(containerId));
}

void Game::closeById(const int containerId)
{
    if (!canPerformGameAction() || containerId < 0)
        return;

    m_protocolGame->sendCloseContainer(static_cast<uint8_t>(containerId));
}

ItemPtr Game::ephemeralItemForWire(const uint16_t typeId, const Position& position, const int stackPos,
                                   const uint16_t count, const uint8_t tier, const uint8_t fluidOrSubtype)
{
    if (typeId == 0)
        return nullptr;
    const ItemPtr item = Item::create(typeId);
    item->setPosition(position, static_cast<uint8_t>(std::max(0, stackPos)));
    if (count != 0)
        item->setCount(count);
    else if (fluidOrSubtype != 0)
        item->setCountOrSubType(fluidOrSubtype);
    if (tier != 0)
        item->setTier(tier);
    return item;
}

void Game::refreshContainer(const int containerId)
{
    if (!canPerformGameAction() || containerId < 0)
        return;

    m_protocolGame->sendRefreshContainer(static_cast<uint8_t>(containerId));
}

void Game::attack(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    if (m_localPlayer && creatureId && creatureId == m_localPlayer->getId())
        return;

    // cancel when attacking again
    uint32_t id = creatureId;
    if (id && id == m_attackingCreatureId)
        id = 0;

    if (id && isFollowing())
        cancelFollow();

    setAttackingCreatureId(id);
    tibia::game::localPlayerStopAutoWalk();

    if (m_protocolVersion >= 963) {
        if (id)
            m_seq = id;
    } else
        ++m_seq;

    m_protocolGame->sendAttack(id, m_seq);

    // Attack requests only; setAttackingCreatureId also mirrors server-driven targets.
    tibia::game::notify("onAttack", id);
}

void Game::follow(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    if (m_localPlayer && creatureId && creatureId == m_localPlayer->getId())
        return;

    // cancel when following again
    uint32_t id = creatureId;
    if (id && id == m_followingCreatureId)
        id = 0;

    if (id && isAttacking())
        cancelAttack();

    setFollowingCreatureId(id);
    tibia::game::localPlayerStopAutoWalk();

    if (m_protocolVersion >= 963) {
        if (id)
            m_seq = id;
    } else
        ++m_seq;

    m_protocolGame->sendFollow(id, m_seq);
}

void Game::cancelAttackAndFollow()
{
    if (!canPerformGameAction())
        return;

    if (isFollowing())
        setFollowingCreatureId(0);

    if (isAttacking())
        setAttackingCreatureId(0);

    tibia::game::localPlayerStopAutoWalk();

    m_protocolGame->sendCancelAttackAndFollow();

    tibia::game::notify("onCancelAttackAndFollow");
}

void Game::talk(const std::string_view message)
{
    if (!canPerformGameAction() || message.empty())
        return;

    talkChannel(tibia::client::MessageSay, 0, message);
}

void Game::talkChannel(const tibia::client::MessageMode mode, const uint16_t channelId, const std::string_view message)
{
    if (!canPerformGameAction() || message.empty())
        return;

    m_protocolGame->sendTalk(mode, channelId, "", message);
}

void Game::talkPrivate(const tibia::client::MessageMode mode, const std::string_view receiver, const std::string_view message)
{
    if (!canPerformGameAction() || receiver.empty() || message.empty())
        return;

    m_protocolGame->sendTalk(mode, 0, receiver, message);
}

void Game::openPrivateChannel(const std::string_view receiver)
{
    if (!canPerformGameAction() || receiver.empty())
        return;

    m_protocolGame->sendOpenPrivateChannel(receiver);
}

void Game::requestChannels()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestChannels();
}

void Game::joinChannel(const uint16_t channelId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendJoinChannel(channelId);
}

void Game::leaveChannel(const uint16_t channelId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendLeaveChannel(channelId);
}

void Game::closeNpcChannel()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendCloseNpcChannel();
}

void Game::openOwnChannel()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendOpenOwnChannel();
}

void Game::inviteToOwnChannel(const std::string_view name)
{
    if (!canPerformGameAction() || name.empty())
        return;

    m_protocolGame->sendInviteToOwnChannel(name);
}

void Game::excludeFromOwnChannel(const std::string_view name)
{
    if (!canPerformGameAction() || name.empty())
        return;

    m_protocolGame->sendExcludeFromOwnChannel(name);
}

void Game::partyInvite(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendInviteToParty(creatureId);
}

void Game::partyJoin(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendJoinParty(creatureId);
}

void Game::partyRevokeInvitation(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRevokeInvitation(creatureId);
}

void Game::partyPassLeadership(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendPassLeadership(creatureId);
}

void Game::partyLeave()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendLeaveParty();
}

void Game::partyDisband()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendDisbandParty();
}

void Game::partyShareExperience(const bool active)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendShareExperience(active);
}

void Game::sendPartyAnalyzerReset()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendPartyAnalyzerAction(tibia::client::PartyAnalyzerAction_t::PARTYANALYZERACTION_RESET);
}

void Game::sendPartyAnalyzerPriceType()
{
    if (!canPerformGameAction())
        return;

    // server just toggles between Market/Leader
    m_protocolGame->sendPartyAnalyzerAction(tibia::client::PartyAnalyzerAction_t::PARTYANALYZERACTION_PRICETYPE);
}

void Game::sendPartyAnalyzerPriceValue()
{
    if (!canPerformGameAction())
        return;

    // For now, send empty items vector - the server will handle getting the prices
    // This matches what the server code expects for PARTYANALYZERACTION_PRICEVALUE
    m_protocolGame->sendPartyAnalyzerAction(tibia::client::PartyAnalyzerAction_t::PARTYANALYZERACTION_PRICEVALUE);
}

void Game::sendPartyAnalyzerAction(const uint8_t action, const std::vector<std::tuple<uint16_t, uint64_t>>& items)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendPartyAnalyzerAction(action, items);
}

void Game::requestTeamFinderLeader()
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendTeamFinderLeaderAction(0);
}

void Game::resetTeamFinderLeader()
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendTeamFinderLeaderAction(1);
}

void Game::updateTeamFinderMember(const uint32_t memberId, const uint8_t status)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendTeamFinderLeaderAction(2, memberId, status);
}

void Game::createTeamFinder(const TeamFinderAssemble& assemble)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendTeamFinderLeaderAction(3, 0, 0, assemble);
}

void Game::requestTeamFinderList()
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendTeamFinderMemberAction(0);
}

void Game::joinTeamFinder(const uint32_t leaderId)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendTeamFinderMemberAction(1, leaderId);
}

void Game::leaveTeamFinder(const uint32_t leaderId)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendTeamFinderMemberAction(2, leaderId);
}

void Game::openDepotSearch()
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendOpenDepotSearch();
}

void Game::closeDepotSearchRequest()
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendCloseDepotSearch();
}

void Game::requestDepotSearchItem(const uint16_t itemId, const uint8_t tier)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendDepotSearchType(itemId, tier);
}

void Game::retrieveDepotSearch(const uint16_t itemId, const uint8_t tier, const uint8_t type)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendRetrieveDepotSearch(itemId, tier, type);
}

void Game::openParentContainerFromDepotSearch(const Position& pos)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendOpenParentContainer(pos);
}

void Game::gatherAll(const Position& pos, const uint16_t itemId, const uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendGatherAll(pos, itemId, stackpos);
}

void Game::requestOutfit()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestOutfit();
}

void Game::changeOutfit(const Outfit& outfit)
{
    changeOutfitAndMount(outfit, outfit.hasMount(), false);
}

void Game::changeOutfitAndMount(const Outfit& outfit, const bool mounted, const bool randomMount)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendChangeOutfit(outfit, mounted, randomMount);
}

void Game::sendTyping(const bool typing)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendTyping(typing);
}

void Game::addVip(const std::string_view name)
{
    if (!canPerformGameAction() || name.empty())
        return;

    m_protocolGame->sendAddVip(name);
}

void Game::removeVip(const uint32_t playerId)
{
    if (!canPerformGameAction())
        return;

    const auto it = m_vips.find(playerId);
    if (it == m_vips.end())
        return;

    m_vips.erase(it);
    m_protocolGame->sendRemoveVip(playerId);
}

void Game::editVip(const uint32_t playerId, const std::string_view description, const uint32_t iconId, const bool notifyLogin, const std::vector<uint8_t>& groupID)
{
    if (!canPerformGameAction())
        return;

    const auto it = m_vips.find(playerId);
    if (it == m_vips.end())
        return;

    std::get<2>(m_vips[playerId]) = description;
    std::get<3>(m_vips[playerId]) = iconId;
    std::get<4>(m_vips[playerId]) = notifyLogin;
    std::get<5>(m_vips[playerId]) = groupID;

    if (getFeature(tibia::client::GameAdditionalVipInfo)) {
        if (getFeature(tibia::client::GameVipGroups)) {
            m_protocolGame->sendEditVip(playerId, description, iconId, notifyLogin, groupID);
        } else {
            m_protocolGame->sendEditVip(playerId, description, iconId, notifyLogin);
        }
    }
}

void Game::editVipGroups(const tibia::client::GroupsEditInfoType_t action, const uint8_t groupId, const std::string_view groupName)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendEditVipGroups(action, groupId, groupName);
}

void Game::setChaseMode(const tibia::client::ChaseModes chaseMode)
{
    if (!canPerformGameAction())
        return;

    if (m_chaseMode == chaseMode)
        return;

    m_chaseMode = chaseMode;
    m_protocolGame->sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onChaseModeChange", chaseMode);
}

void Game::setFightMode(const tibia::client::FightModes fightMode)
{
    if (!canPerformGameAction())
        return;

    if (m_fightMode == fightMode)
        return;

    m_fightMode = fightMode;
    m_protocolGame->sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onFightModeChange", fightMode);
}

void Game::setSafeFight(const bool on)
{
    if (!canPerformGameAction())
        return;

    if (m_safeFight == on)
        return;

    m_safeFight = on;
    m_protocolGame->sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onSafeFightChange", on);
}

void Game::setTactics(const tibia::client::ChaseModes chaseMode, const bool safeFight, tibia::client::PVPModes pvpMode)
{
    if (!canPerformGameAction())
        return;

    if (!getFeature(tibia::client::GamePVPMode))
        pvpMode = m_pvpMode;
    if (m_chaseMode == chaseMode && m_safeFight == safeFight && m_pvpMode == pvpMode)
        return;

    const bool chaseChanged = m_chaseMode != chaseMode;
    const bool safeChanged = m_safeFight != safeFight;
    const bool pvpChanged = m_pvpMode != pvpMode;
    m_chaseMode = chaseMode;
    m_safeFight = safeFight;
    m_pvpMode = pvpMode;
    m_protocolGame->sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    if (chaseChanged)
        tibia::game::notify("onChaseModeChange", chaseMode);
    if (safeChanged)
        tibia::game::notify("onSafeFightChange", safeFight);
    if (pvpChanged)
        tibia::game::notify("onPVPModeChange", pvpMode);
}

void Game::setPVPMode(const tibia::client::PVPModes pvpMode)
{
    if (!canPerformGameAction())
        return;

    if (!getFeature(tibia::client::GamePVPMode))
        return;

    if (m_pvpMode == pvpMode)
        return;

    m_pvpMode = pvpMode;
    m_protocolGame->sendChangeFightModes(m_fightMode, m_chaseMode, m_safeFight, m_pvpMode);
    tibia::game::notify("onPVPModeChange", pvpMode);
}

void Game::setUnjustifiedPoints(const UnjustifiedPoints unjustifiedPoints)
{
    if (!canPerformGameAction())
        return;

    if (!getFeature(tibia::client::GameUnjustifiedPoints))
        return;

    if (m_unjustifiedPoints == unjustifiedPoints)
        return;

    m_unjustifiedPoints = unjustifiedPoints;
    tibia::game::notify("onUnjustifiedPointsChange", unjustifiedPoints);
}

void Game::setOpenPvpSituations(const uint8_t openPvpSituations)
{
    if (!canPerformGameAction())
        return;

    if (m_openPvpSituations == openPvpSituations)
        return;

    m_openPvpSituations = openPvpSituations;
    tibia::game::notify("onOpenPvpSituationsChange", openPvpSituations);
}

void Game::inspectNpcTrade(const uint16_t typeId, const uint8_t count)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    m_protocolGame->sendInspectNpcTrade(typeId, count);
}

void Game::buyItem(const uint16_t typeId, const uint8_t countOrSubType, const uint16_t amount, const bool ignoreCapacity,
                   const bool buyWithBackpack)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    m_protocolGame->sendBuyItem(typeId, countOrSubType, amount, ignoreCapacity, buyWithBackpack);
}

void Game::sellItem(const uint16_t typeId, const uint8_t subType, const uint16_t amount, const bool ignoreEquipped)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    m_protocolGame->sendSellItem(typeId, subType, amount, ignoreEquipped);
}

void Game::closeNpcTrade()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendCloseNpcTrade();
}

void Game::requestTradeAt(const Position& position, const uint16_t itemId, const int stackPos, const uint32_t creatureId)
{
    if (!canPerformGameAction() || itemId == 0 || stackPos < 0 || !creatureId)
        return;

    m_protocolGame->sendRequestTrade(position, itemId, static_cast<uint8_t>(stackPos), creatureId);
}

void Game::inspectTrade(const bool counterOffer, const uint8_t index)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendInspectTrade(counterOffer, index);
}

void Game::acceptTrade()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendAcceptTrade();
}

void Game::rejectTrade()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRejectTrade();
}

void Game::editText(const uint32_t id, const std::string_view text)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendEditText(id, text);
}

void Game::editList(const uint32_t id, const uint8_t doorId, const std::string_view text)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendEditList(id, doorId, text);
}

void Game::editGuildMessage(const std::string_view text)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendEditGuildMessage(text);
}

void Game::reportRuleViolationCip(const uint8_t type, const uint8_t reason, const std::string_view name, const std::string_view comment,
                                  const std::string_view translation, const uint32_t id)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRuleViolationReport(type, reason, name, comment, translation, id);
}

void Game::reportBugCip(const uint8_t category, const std::string_view comment, const std::string_view typoSpeaker, const std::string_view typoText,
                        const Position& position)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendBugReport(category, comment, typoSpeaker, typoText, position);
}

void Game::requestGuildMessage()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendGetGuildMessage();
}

void Game::requestTextForReport(const Position& position, const uint16_t typeId, const uint8_t stackPosition)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendGetTextForReport(position, typeId, stackPosition);
}

void Game::requestQuestLog()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestQuestLog();
}

void Game::requestQuestLine(const uint16_t questId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestQuestLine(questId);
}

void Game::equipItemId(const uint16_t itemId, const uint8_t tier)
{
    if (!canPerformGameAction())
        return;

    if (getFeature(tibia::client::GameThingUpgradeClassification)) {
        const auto& thing = tibia::game::thingType(itemId, ThingCategoryItem);
        if (thing && thing->getClassification() > 0) {
            m_protocolGame->sendEquipItemWithTier(itemId, tier);
            return;
        }
    }
    m_protocolGame->sendEquipItemWithCountOrSubType(itemId, tier);
}

void Game::mount(const bool mount)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendMountStatus(mount);
}

void Game::requestItemInfo(const uint16_t typeId, const uint8_t subType, const uint8_t index)
{
    if (!canPerformGameAction() || typeId == 0)
        return;

    m_protocolGame->sendRequestItemInfo(typeId, subType, index);
}

void Game::answerModalDialog(const uint32_t dialog, const uint8_t button, const uint8_t choice)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendAnswerModalDialog(dialog, button, choice);
}

void Game::browseField(const Position& position)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendBrowseField(position);
}

void Game::seekInContainer(const uint8_t containerId, const uint16_t index)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendSeekInContainer(containerId, index);
}

void Game::buyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendBuyStoreOffer(offerId, action, name, type, location);
}

void Game::requestTransactionHistory(const uint32_t page, const uint32_t entriesPerPage)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestTransactionHistory(page, entriesPerPage);
}

void Game::requestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder, const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestStoreOffers(categoryName, subCategory, sortOrder, serviceType);
}

void Game::sendRequestStoreHome()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestStoreHome();
}

void Game::sendRequestStorePremiumBoost()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestStorePremiumBoost();
}

void Game::sendRequestUsefulThings(const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestUsefulThings(serviceType);
}

void Game::sendRequestStoreOfferById(const uint32_t offerId, const uint8_t sortOrder, const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestStoreOfferById(offerId, sortOrder, serviceType);
}

void Game::sendRequestStoreSearch(const std::string_view searchText, const uint8_t sortOrder, const uint8_t serviceType)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestStoreSearch(searchText, sortOrder, serviceType);
}

void Game::openStore(const uint8_t serviceType, const std::string_view category)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendOpenStore(serviceType, category);
}

void Game::transferCoins(const std::string_view recipient, const uint32_t amount)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendTransferCoins(recipient, amount);
}

void Game::openTransactionHistory(const uint8_t entriesPerPage)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendOpenTransactionHistory(entriesPerPage);
}

void Game::ping()
{
    if (!m_protocolGame || !m_protocolGame->isConnected())
        return;

    if (m_pingReceived != m_pingSent)
        return;

    m_protocolGame->sendPing();
    ++m_pingSent;
    m_pingTimer.restart();
}

void Game::changeMapAwareRange(const uint8_t xrange, const uint8_t yrange)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendChangeMapAwareRange(xrange, yrange);
}

bool Game::isAttacking()
{
    if (!m_attackingCreatureId)
        return false;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    return storage && !!storage->creatureAt(m_attackingCreatureId);
}
bool Game::isFollowing()
{
    if (!m_followingCreatureId)
        return false;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    return storage && !!storage->creatureAt(m_followingCreatureId);
}
bool Game::isConnectionOk() { return m_protocolGame && m_protocolGame->getElapsedTicksSinceLastRead() < 5000; }

bool Game::canPerformGameAction() const
{
    // we can only perform game actions if we meet these conditions:
    // - the game is online
    // - the local player exists
    // - the local player is not dead
    // - we have a game protocol
    // - the game protocol is connected
    return m_online && m_localPlayer && !m_dead && m_protocolGame && m_protocolGame->isConnected();
}

void Game::setProtocolVersion(const uint16_t version)
{
    if (m_protocolVersion == version)
        return;

    if (isOnline())
        throw Exception("Unable to change protocol version while online");

    if (version != 0 && (version < 740 || version > tibia::config::lastSupportedVersion()))
        throw Exception("Protocol version {} not supported", version);

    m_protocolVersion = version;

    Proto::buildMessageModesMap(version);

    tibia::game::notify("onProtocolVersionChange", version);
}

void Game::setClientVersion(const uint16_t version)
{
    if (m_clientVersion == version)
        return;

    if (isOnline())
        throw Exception("Unable to change client version while online");

    if (version != 0 && (version < 740 || version > tibia::config::lastSupportedVersion()))
        throw Exception("Client version {} not supported", version);

    m_features.reset();

    m_clientVersion = version;

    tibia::game::notify("onClientVersionChange", version);
}

void Game::setAttackingCreatureId(const uint32_t creatureId)
{
    if (creatureId == m_attackingCreatureId)
        return;

    const uint32_t oldId = m_attackingCreatureId;
    m_attackingCreatureId = creatureId;

    tibia::game::notify("onAttackingCreatureChange", creatureId, oldId);
}

void Game::setFollowingCreatureId(const uint32_t creatureId)
{
    if (creatureId == m_followingCreatureId)
        return;

    const uint32_t oldId = m_followingCreatureId;
    m_followingCreatureId = creatureId;

    tibia::game::notify("onFollowingCreatureChange", creatureId, oldId);
}

std::string Game::formatCreatureName(const std::string_view name)
{
    std::string formatedName{ name };
    if (getFeature(tibia::client::GameFormatCreatureName) && name.length() > 0) {
        bool upnext = true;
        for (char& i : formatedName) {
            const char ch = i;
            if (upnext) {
                i = std::toupper(ch);
                upnext = false;
            }
            if (ch == ' ')
                upnext = true;
        }
    }

    return formatedName;
}

int Game::findEmptyContainerId()
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        return storage->findEmptyId();
    return 0;
}

tibia::client::OperatingSystem_t Game::getOs()
{
    if (m_clientCustomOs > tibia::client::CLIENTOS_NONE)
        return m_clientCustomOs;

    // qtclient tip speaks CipSoft NEW_* OS ids so Crystal keeps classic CIP wire
    // (isOTCR stays false). Product packs may setCustomOs(CLIENTOS_OTCR_*).
    if (tibia::host::app().getOs() == "windows")
        return tibia::client::CLIENTOS_TIBIA_NEW_WINDOWS;

    if (tibia::host::app().getOs() == "mac")
        return tibia::client::CLIENTOS_TIBIA_NEW_MAC;

    return tibia::client::CLIENTOS_TIBIA_NEW_LINUX;
}

void Game::leaveMarket()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendMarketLeave();

    tibia::game::notify("onMarketLeave");
}

void Game::browseMarket(const uint8_t browseId, const uint16_t browseType, const uint8_t tier)
{
    if (!canPerformGameAction()) {
        return;
    }

    m_protocolGame->sendMarketBrowse(browseId, browseType, tier);
}

void Game::createMarketOffer(const uint8_t type, const uint16_t itemId, const uint8_t itemTier, const uint16_t amount, const uint64_t price, const uint8_t anonymous)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendMarketCreateOffer(type, itemId, itemTier, amount, price, anonymous);
}

void Game::cancelMarketOffer(const uint32_t timestamp, const uint16_t counter)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendMarketCancelOffer(timestamp, counter);
}

void Game::acceptMarketOffer(const uint32_t timestamp, const uint16_t counter, const uint16_t amount)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendMarketAcceptOffer(timestamp, counter, amount);
}

void Game::preyAction(const uint8_t slot, const uint8_t actionType, const uint16_t index)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendPreyAction(slot, actionType, index);
}

void Game::preyRequest()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendPreyRequest();
}

namespace
{
    uint8_t toServerDifficulty(const uint16_t uiDifficulty)
    {
        if (std::cmp_equal(uiDifficulty, 0)) {
            g_logger.warning("ToServerDifficulty received invalid UI difficulty 0");
            return 1;
        }
        if (std::cmp_greater_equal(uiDifficulty, 4)) {
            return 3;
        }
        return static_cast<uint8_t>(uiDifficulty - 1);
    }
}

void Game::bountyTaskAction(const uint8_t actionType, const uint16_t value)
{
    if (!canPerformGameAction())
        return;

    switch (actionType) {
        case tibia::client::BOUNTY_ACTION_REQUEST:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_OPEN_BOUNTY);
            break;
        case tibia::client::BOUNTY_ACTION_REROLL:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_BOUNTY_REROLL);
            break;
        case tibia::client::BOUNTY_ACTION_SELECT:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_BOUNTY_SELECT_TASK, value);
            break;
        case tibia::client::BOUNTY_ACTION_CLAIM_REWARD:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_BOUNTY_CLAIM_REWARD);
            break;
        case tibia::client::BOUNTY_ACTION_CHANGE_DIFFICULTY:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_BOUNTY_CHANGE_DIFFICULTY, toServerDifficulty(value));
            break;
        case tibia::client::BOUNTY_ACTION_CLAIM_DAILY:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_BOUNTY_CLAIM_DAILY);
            break;
        default:
            g_logger.warning("Unknown bounty task action type {}", static_cast<int>(actionType));
            break;
    }
}

void Game::weeklyTaskAction(const uint8_t actionType, const uint16_t value)
{
    if (!canPerformGameAction())
        return;

    switch (actionType) {
        case tibia::client::WEEKLY_ACTION_REFRESH_DATA:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_OPEN_WEEKLY);
            break;
        case tibia::client::WEEKLY_ACTION_SELECT_DIFFICULTY:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_WEEKLY_SELECT_DIFFICULTY, toServerDifficulty(value));
            break;
        case tibia::client::WEEKLY_ACTION_DELIVER_ITEM:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_WEEKLY_DELIVER, value);
            break;
        default:
            g_logger.warning("Unknown weekly task action type {}", static_cast<int>(actionType));
            break;
    }
}

void Game::taskHuntingShopRequest()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_OPEN_HUNTING_SHOP);
}

void Game::taskHuntingShopPurchase(const uint8_t offerIndex)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_HUNTING_SHOP_BUY_OFFER, offerIndex, 0);
}

void Game::bountyTalismanUpgrade(const uint8_t pathIndex)
{
    if (!canPerformGameAction())
        return;

    if (std::cmp_greater(pathIndex, 3)) {
        g_logger.warning("BountyTalismanUpgrade: invalid pathIndex {}", static_cast<int>(pathIndex));
        return;
    }

    m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_BOUNTY_TALISMAN_UPGRADE, pathIndex);
}

void Game::bountyPreferredAction(const uint8_t actionType, const uint16_t slot, const uint16_t raceId)
{
    if (!canPerformGameAction())
        return;

    switch (actionType) {
        case tibia::client::PREFERRED_ACTION_REQUEST:
            m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_OPEN_BOUNTY);
            break;
        case tibia::client::PREFERRED_ACTION_BUY_SLOT:
            if (std::cmp_not_equal(slot, 0)) {
                m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_PREFERRED_UNLOCK, slot);
            }
            break;
        case tibia::client::PREFERRED_ACTION_SET_PREFERRED:
            if (std::cmp_not_equal(slot, 0) && std::cmp_not_equal(raceId, 0)) {
                m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_PREFERRED_ASSIGN, slot, raceId);
            }
            break;
        case tibia::client::PREFERRED_ACTION_SET_UNWANTED:
            if (std::cmp_not_equal(slot, 0) && std::cmp_not_equal(raceId, 0)) {
                m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_UNWANTED_ASSIGN, slot, raceId);
            }
            break;
        case tibia::client::PREFERRED_ACTION_REMOVE_PREFERRED:
            if (std::cmp_not_equal(slot, 0)) {
                m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_PREFERRED_CLEAR, slot);
            }
            break;
        case tibia::client::PREFERRED_ACTION_REMOVE_UNWANTED:
            if (std::cmp_not_equal(slot, 0)) {
                m_protocolGame->sendTaskBoardAction(tibia::client::TASK_BOARD_OPTION_UNWANTED_CLEAR, slot);
            }
            break;
        default:
            g_logger.warning("Unknown bounty preferred action type {}", static_cast<int>(actionType));
            break;
    }
}

void Game::soulsealFightAction(const uint16_t raceId)
{
    if (!canPerformGameAction() || std::cmp_equal(raceId, 0))
        return;

    m_protocolGame->sendSoulSealsAction(raceId);
}

void Game::sendStartOfflineTraining(const uint8_t skillType)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendStartOfflineTraining(skillType);
}

void Game::sendTutorialChangeVocation(uint8_t vocationClientId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendTutorialChangeVocation(vocationClientId);
}

void Game::openPortableForgeRequest()
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendOpenPortableForge();
}

void Game::requestResourceBalance(const uint8_t resourceType)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendRequestResourceBalance(resourceType);
}

void Game::setClientOption(const uint8_t option, const bool value)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendSetClientOptions(option, value);
}

void Game::forgeRequest(tibia::client::ForgeAction_t actionType, bool convergence, uint16_t firstItemid, uint8_t firstItemTier, uint16_t secondItemId, bool improveChance, bool tierLoss)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendForgeRequest(actionType, convergence, firstItemid, firstItemTier, secondItemId, improveChance, tierLoss);
}

void Game::sendForgeBrowseHistoryRequest(uint16_t page)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendForgeBrowseHistoryRequest(page);
}

void Game::sendExivaOptions(
    const bool allowAll, const bool allowOwnGuild, const bool allowOwnParty,
    const bool allowVipList, const bool allowPlayerWhitelist, const bool allowGuildWhitelist,
    const std::vector<std::string>& characterWhiteList,
    const std::vector<std::string>& removeCharacter,
    const std::vector<std::string>& guildWhiteList,
    const std::vector<std::string>& removeGuild)
{
    if (!canPerformGameAction() || !canExivaOptions())
        return;

    m_protocolGame->sendExivaRestrictions(
        allowAll, allowOwnGuild, allowOwnParty, allowVipList,
        allowPlayerWhitelist, allowGuildWhitelist,
        characterWhiteList, removeCharacter, guildWhiteList, removeGuild);
}

void Game::applyImbuement(const uint8_t slot, const uint32_t imbuementId, const bool protectionCharm)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendApplyImbuement(slot, imbuementId, protectionCharm);
}

void Game::clearImbuement(const uint8_t slot)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendClearImbuement(slot);
}

void Game::closeImbuingWindow()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendCloseImbuingWindow();
}

void Game::selectImbuementItem(const uint16_t itemId, const Position& pos, const uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendImbuementWindowAction(tibia::client::IMBUEMENT_WINDOW_SELECT_ITEM, itemId, pos, stackpos);
}

void Game::selectImbuementScroll()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendImbuementWindowAction(tibia::client::IMBUEMENT_WINDOW_SCROLL);
}

void Game::imbuementDurations(const bool isOpen)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendImbuementDurations(isOpen);
}

void Game::stashWithdraw(const uint16_t itemId, const uint32_t count, const uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendStashWithdraw(itemId, count, stackpos);
}

void Game::greetNpc(const uint32_t npcId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendGreet(npcId);
}

void Game::stashStowItem(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendStashStow(position, itemId, count, stackpos, action);
}

void Game::requestHighscore(const uint8_t action, const uint8_t category, const uint32_t vocation, const std::string_view world, const uint8_t worldType, const uint8_t battlEye, const uint16_t page, const uint8_t totalPages)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendHighscoreInfo(action, category, vocation, world, worldType, battlEye, page, totalPages);
}

void Game::processHighscore(const std::string_view serverName, const std::string_view world, const uint8_t worldType, const uint8_t battlEye,
                            const std::vector<std::tuple<uint32_t, std::string>>& vocations,
                            const std::vector<std::tuple<uint8_t, std::string>>& categories,
                            const uint16_t page, const uint16_t totalPages,
                            const std::vector<std::tuple<uint32_t, std::string, std::string, uint8_t, std::string, uint16_t, uint8_t, uint64_t>>& highscores, const uint32_t entriesTs,
                            const std::vector<std::string>& worlds, const std::string_view allVocationsName, const uint32_t selectedVocation,
                            const uint8_t selectedCategory, const uint8_t selectedIndex, const bool showLoyaltyTitle, const uint8_t valueType)
{
    tibia::game::notify("onProcessHighscores", serverName, world, worldType, battlEye, vocations, categories, page, totalPages, highscores, entriesTs,
                                 worlds, allVocationsName, selectedVocation, selectedCategory, selectedIndex, showLoyaltyTitle, valueType);
}

void Game::requestBless()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestBless();
}

void Game::sendQuickLootAt(const uint8_t variant, const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendQuickLoot(variant, position, itemId, static_cast<uint8_t>(std::max(0, stackPos)));
}

void Game::requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t /*size*, const std::vector<uint16_t>& listedItems)
{
    if (!canPerformGameAction())
        return;

    // Always trust the vector length — Lua `#` can disagree with table contents and
    // a mismatched size desyncs the game protocol (server reads the wrong byte count).
    const uint16_t actualSize = static_cast<uint16_t>(listedItems.size());
    m_protocolGame->requestQuickLootBlackWhiteList(filter, actualSize, listedItems);
}

void Game::openContainerQuickLoot(const uint8_t action, const uint8_t category, const Position& pos, const uint16_t itemId, const uint8_t stackpos, const bool useMainAsFallback)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->openContainerQuickLoot(action, category, pos, itemId, stackpos, useMainAsFallback);
}

void Game::sendTeleport(const Position& pos)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendTeleport(pos);
}

void Game::inspectionNormalObject(const Position& position)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendInspectionNormalObject(position);
}

void Game::inspectionObject(const tibia::client::InspectObjectTypes inspectionType, const uint16_t itemId, const uint8_t itemCount)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendInspectionObject(inspectionType, itemId, itemCount);
}

void Game::inspectCharacter(const uint32_t creatureId, const uint8_t tab)
{
    if (!canPerformGameAction())
        return;

    // CIP InspectPlayer: tab 0 is allow-all / withdraw (no creature id). Tabs 1+
    // name a player (invite/ask/allow/revoke/open inspection).
    if (creatureId == 0 && tab != 0)
        return;

    m_protocolGame->sendInspectCharacter(creatureId, tab);
}

void Game::requestBestiary()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestBestiary();
}

void Game::requestBestiaryOverview(const std::string_view catName, bool search, std::vector<uint16_t> raceIds)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestBestiaryOverview(catName, search, raceIds);
}

void Game::requestBestiarySearch(const uint16_t raceId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestBestiarySearch(raceId);
}

void Game::requestSendBuyCharmRune(const uint8_t runeId, const uint8_t action, const uint16_t raceId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendBuyCharmRune(runeId, action, raceId);
}

void Game::requestSendCharacterInfo(const uint32_t playerId, const tibia::client::CyclopediaCharacterInfoType_t characterInfoType, const uint16_t entriesPerPage, const uint16_t page)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendCyclopediaRequestCharacterInfo(playerId, characterInfoType, entriesPerPage, page);
}

void Game::requestSetCharacterTitle(const uint8_t titleId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendFriendSystemAction(0x0E, titleId);
}

void Game::sendFriendSystemPayload(const std::string& json)
{
    if (!canPerformGameAction() || json.empty())
        return;
    m_protocolGame->sendFriendSystemAction(0, 0, json);
}

void Game::configureShowOffSocket(const Position& pos, const uint16_t itemId, const uint8_t stackpos)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendConfigureShowOffSocket(pos, itemId, stackpos);
}

void Game::setShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, const uint16_t itemId,
                                         const uint8_t stackpos, const uint8_t direction, const uint8_t podiumVisible)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendSetShowOffSocketConfiguration(outfit, pos, itemId, stackpos, direction, podiumVisible);
}

void Game::setHirelingOutfit(const Outfit& outfit, const uint32_t hirelingCreatureId)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendSetHirelingOutfit(outfit, hirelingCreatureId);
}

void Game::joinAggression(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendJoinAggression(creatureId);
}

void Game::setCreaturePodiumConfiguration(const uint32_t monsterRaceId, const Position& pos, const uint16_t itemId,
                                          const uint8_t stackpos, const uint8_t direction, const uint8_t podiumVisible,
                                          const uint8_t monsterVisible)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendSetCreaturePodiumConfiguration(monsterRaceId, pos, itemId, stackpos, direction,
                                                       podiumVisible, monsterVisible);
}

void Game::requestCharacterTradeConfiguration(const uint8_t action)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendCharacterTradeConfigurationAction(action);
}

void Game::confirmCharacterTradeConfiguration(const uint32_t price, const uint32_t endTimestamp,
                                              const std::vector<std::tuple<uint16_t, uint8_t>>& items,
                                              const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems,
                                              const std::vector<uint16_t>& highlights,
                                              const bool createAuction)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendCharacterTradeConfigurationAction(createAuction ? 3 : 2, price, endTimestamp, items,
                                                          storeItems, highlights);
}

void Game::requestTransactionDetails(const uint32_t transactionId)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendGetTransactionDetails(transactionId);
}

void Game::sendStoreEvent(const uint8_t eventType)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendStoreEvent(eventType);
}

void Game::sendClientCheck(const std::vector<uint8_t>& data)
{
    if (!m_protocolGame)
        return;
    m_protocolGame->sendClientCheck(data);
}

void Game::sendContainerAction(const uint8_t action, const uint8_t containerId)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendContainerAction(action, containerId);
}

void Game::sendClientDetails(const std::string_view details)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendClientDetails(details);
}

void Game::requestMarketStatistics(const uint16_t itemId, const uint8_t tier)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendMarketStatistics(itemId, tier);
}

void Game::sendSetHirelingName(const std::string_view name)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendSetHirelingName(name);
}

void Game::requestSendCyclopediaHouseAuction(const tibia::client::CyclopediaHouseAuctionType_t type, const uint32_t houseId, const uint32_t timestamp, const uint64_t bidValue, const std::string_view name)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendCyclopediaHouseAuction(type, houseId, timestamp, bidValue, name);
}

void Game::sendSelectSpellAim(const std::vector<uint16_t>& spellIds, const bool enabled)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendSelectSpellAim(spellIds, enabled);
}

void Game::sendCyclopediaMapDonate(const uint16_t areaId, const uint32_t amount)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendCyclopediaMapDonate(areaId, amount);
}

void Game::sendCyclopediaMapResetViewpoints(const uint16_t subAreaId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendCyclopediaMapResetViewpoints(subAreaId);
}

void Game::requestBosstiaryInfo()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestBosstiaryInfo();
}

void Game::requestBossSlootInfo()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestBossSlootInfo();
}

void Game::requestBossSlotAction(const uint8_t action, const uint32_t raceId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendRequestBossSlotAction(action, raceId);
}

void Game::sendStatusTrackerBestiary(const uint16_t raceId, const bool status)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendStatusTrackerBestiary(raceId, status);
}

void Game::sendOpenRewardWall()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendOpenRewardWall();
}

void Game::requestOpenRewardHistory()
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendOpenRewardHistory();
}

void Game::requestGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendGetRewardDaily(bonusShrine, items);
}

void Game::sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, const bool autoTrackNewQuests, const bool autoUntrackCompletedQuests, const uint8_t extra)
{
    if (!canPerformGameAction()) {
        return;
    }

    m_protocolGame->sendRequestTrackerQuestLog(
        missionIds,
        autoTrackNewQuests,
        autoUntrackCompletedQuests,
        extra
    );
}

void Game::processCyclopediaCharacterOffenceStats(const CyclopediaCharacterOffenceStats& data)
{
    tibia::game::notify("onCyclopediaCharacterOffenceStats", data);
}

void Game::processCyclopediaCharacterDefenceStats(const CyclopediaCharacterDefenceStats& data)
{
    tibia::game::notify("onCyclopediaCharacterDefenceStats", data);
}

void Game::processCyclopediaCharacterMiscStats(const CyclopediaCharacterMiscStats& data)
{
    tibia::game::notify("onCyclopediaCharacterMiscStats", data);
}

void Game::openWheel(uint32_t playerId)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendOpenWheel(playerId);
}

void Game::gemAction(const uint8_t actionType, const uint16_t param, const uint8_t pos)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendWheelGemAction(actionType, param, pos);
}

void Game::sendApplyWheelPoints(const std::vector<uint16_t>& slotPoints, const std::array<uint16_t, 8>& vesselGems)
{
    if (!canPerformGameAction())
        return;
    m_protocolGame->sendApplyWheelPoints(slotPoints, vesselGems);
}

void Game::sendWeaponProficiencyAction(const uint8_t actionType, const uint16_t itemId)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendWeaponProficiencyAction(actionType, itemId);
}

void Game::sendWeaponProficiencyApply(const uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendWeaponProficiencyApply(itemId, levels, perkPositions);
}

void Game::sendWeaponProficiencySlotAction(const uint8_t actionType, const uint16_t itemId, const uint8_t level, const uint8_t perkPosition, const int offerIndex)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendWeaponProficiencySlotAction(actionType, itemId, level, perkPosition, offerIndex);
}

void Game::sendBossDifficultyAction(const uint8_t action, const uint16_t difficulty)
{
    if (!canPerformGameAction())
        return;

    m_protocolGame->sendBossDifficultySelectionAction(action, difficulty);
}
