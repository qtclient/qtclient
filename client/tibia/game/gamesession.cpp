#include "gamesession.h"

#include "gamehub.h"
#include "gamehub_impl.h"
#include "game/playerdata.h"
#include "game/playerskillstats.h"
#include <tibia/creatures/creaturestorage.h>
#include <tibia/game/localplayersessionthing.h>
#include <tibia/game/mapsession.h>
#include <tibia/config/runtimegameconfig.h>
#include <framework/core/clock.h>
#include <tibia/game/sessionhooks.h>
#include <tibia/game/tgamenotify.h>
#include <vector>

namespace tibia::game {

namespace {

using Args = TGameNotify::Args;

} // namespace

struct IGameSession::LiveFans {
    std::vector<SoftHook> soft;
    std::vector<QMetaObject::Connection> typed;
};

IGameSession::IGameSession(QObject* parent)
    : cip::tibia::game::IGameSession(parent)
    , m_liveFans(new LiveFans)
{
    bindLiveNotifyFans();
}

IGameSession::~IGameSession()
{
    if (m_liveFans) {
        for (const QMetaObject::Connection& c : m_liveFans->typed)
            QObject::disconnect(c);
        delete m_liveFans;
        m_liveFans = nullptr;
    }
}

void IGameSession::bindLiveNotifyFans()
{
    // Mirror TGameserverGameSession notify → signal wiring on the tip twin.
    // Does not call loginWorld / forceLogout / cancelLogin.
    auto* fans = m_liveFans;
    if (!fans)
        return;

    // Same SoftHook fields as TGameserverGameSession (minus ProtocolGame
    // onConnect — tip emits `connected` with `onLogin` as the closest g_game
    // bus event without opening the socket path).
    fans->soft.emplace_back("onLogin", [this](const Args&) {
        Q_EMIT connected();
        Q_EMIT gameLoginSuccessful();
        return std::any{};
    });
    fans->soft.emplace_back("onLoginError", [this](const Args& a) {
        Q_EMIT gameLoginErrorMessage(argString(a, 0), 0);
        return std::any{};
    });
    fans->soft.emplace_back("onLoginAdvice", [this](const Args& a) {
        Q_EMIT gameLoginAdviceMessage(argString(a, 0));
        return std::any{};
    });
    fans->soft.emplace_back("onLoginWait", [this](const Args& a) {
        Q_EMIT gameLoginWaitMessage(argString(a, 0), static_cast<int>(argInt(a, 1)));
        return std::any{};
    });
    fans->soft.emplace_back("onConnectionError", [this](const Args& a) {
        Q_EMIT connectionError(argString(a, 0));
        return std::any{};
    });
    fans->soft.emplace_back("onSessionEnd", [this](const Args& a) {
        Q_EMIT sessionEndInformation(static_cast<int>(argInt(a, 0)));
        return std::any{};
    });

    TGameNotify* notify = TGameNotify::instance();
    fans->typed.push_back(connect(notify, &TGameNotify::gameStarted, this, [this] {
        Q_EMIT worldEntered();
    }));
    fans->typed.push_back(connect(notify, &TGameNotify::gameEnded, this, [this] {
        if (isSwitchingCharacter())
            return;
        Q_EMIT disconnected();
    }));
    fans->typed.push_back(connect(notify, &TGameNotify::connectionFailing, this, [this](bool failing) {
        if (failing)
            Q_EMIT pingTimeout();
        else
            Q_EMIT pingTimeoutRecovered();
    }));
    fans->typed.push_back(connect(notify, &TGameNotify::death, this, [this](int, int) {
        Q_EMIT deadMessage();
    }));
}

void IGameSession::onPrepareNextFrameToRender(const qint64 nextFramePredictedTimestampMs)
{
    // Live path: TGameClient::tick → TNextFrameUpdater. Do not advance frames here.
    (void)nextFramePredictedTimestampMs;
}

void IGameSession::onPlayerCreatureAddedToCreatureStorage()
{
    Q_EMIT playerCreatureAddedToCreatureStorage();
}

void IGameSession::onSettingHintsFromServerChanged(const bool canChangePvPFrameSetting)
{
    // CIP gates PvP-frame option UI from this hint. Live options path owns the
    // flag — tip does not write g_game (avoids fighting GameClient config).
    (void)canChangePvPFrameSetting;
}

void IGameSession::onMessageAtEndOfLoginRampUpReceived()
{
    Q_EMIT loginRampUpFinished();
}

void IGameSession::onWorldTypeReceived(const int worldType)
{
    (void)worldType;
    Q_EMIT worldTypeSet();
}

void IGameSession::onCharacterConfigurationLoaded()
{
    // Live character config load stays on TCharacterConfigurationManager /
    // GameClient. Tip slot is CIP-named keep for moc callers.
}

IGameSession* tipIGameSession()
{
    static IGameSession keep;
    return &keep;
}

namespace {
const bool kTipIGameSessionLinked = (tipIGameSession() != nullptr);
}

uint64_t millis()
{
    return static_cast<uint64_t>(g_clock.millis());
}

uint8_t mapSeaFloor()
{
    return tibia::config::mapSeaFloor();
}

uint8_t worldLightIntensity()
{
    return worldLight().intensity;
}

uint8_t worldLightColor()
{
    return worldLight().color;
}

uint32_t mapItemRevision()
{
    return itemRevision();
}

uint32_t mapDynamicRevision()
{
    return dynamicRevision();
}

int subscribeWorldTime(std::function<void(int, int)> listener)
{
    return tibia::world::subscribeWorldTime(std::move(listener));
}

void unsubscribeWorldTime(const int id)
{
    tibia::world::unsubscribeWorldTime(id);
}

int subscribeSessionEnd(std::function<void()> listener)
{
    return tibia::world::subscribeSessionEnd(std::move(listener));
}

void unsubscribeSessionEnd(const int id)
{
    tibia::world::unsubscribeSessionEnd(id);
}

LocalPlayerSessionThingPtr localPlayer()
{
    return tipGameHub().getLocalPlayer();
}


bool isOnline()
{
    return tipGameHub().isOnline();
}

bool isDead()
{
    return tipGameHub().isDead();
}

bool isSceneFrozen()
{
    return tipGameHub().isSceneFrozen();
}

void setSceneFrozen(const bool frozen)
{
    tipGameHub().setSceneFrozen(frozen);
}

bool isSwitchingCharacter()
{
    return tipGameHub().isSwitchingCharacter();
}

std::string characterName()
{
    return tipGameHub().getCharacterName();
}

int clientRevision()
{
    return tipGameHub().getClientVersion();
}

tibia::client::OperatingSystem_t clientOs()
{
    return tipGameHub().getOs();
}

bool cipFeature(const tibia::client::GameFeature feature)
{
    return tipGameHub().getFeature(feature);
}

void enableFeature(const tibia::client::GameFeature feature)
{
    tipGameHub().enableFeature(feature);
}

void disableFeature(const tibia::client::GameFeature feature)
{
    tipGameHub().disableFeature(feature);
}

void setProtocolVersion(const uint16_t version)
{
    tipGameHub().setProtocolVersion(version);
}

void setClientRevision(const uint16_t version)
{
    tipGameHub().setClientVersion(version);
}

void initGameHub()
{
    tipGameHub().init();
}

void terminateGameHub()
{
    tipGameHub().terminate();
}

void processConnectionError(const std::error_code& ec)
{
    tipGameHub().processConnectionError(ec);
}

void startDecodedReplay()
{
    tipGameHub().startDecodedReplay();
}

void deliverReplayPacket(const uint8_t* data, size_t size)
{
    tipGameHub().deliverReplayPacket(data, size);
}

void finishDecodedReplay()
{
    tipGameHub().finishDecodedReplay();
}

std::string formatCreatureName(const std::string_view name)
{
    return tipGameHub().formatCreatureName(name);
}

void setCustomOs(const tibia::client::OperatingSystem_t os)
{
    tipGameHub().setCustomOs(os);
}

uint32_t attackingCreatureId()
{
    return tipGameHub().getAttackingCreatureId();
}

uint32_t followingCreatureId()
{
    return tipGameHub().getFollowingCreatureId();
}

void setAttackingCreatureId(const uint32_t creatureId)
{
    tipGameHub().setAttackingCreatureId(creatureId);
}

void talk(const std::string_view message)
{
    tipGameHub().talk(message);
}

void turn(const tibia::client::Direction direction)
{
    tipGameHub().turn(direction);
}

bool walk(const tibia::client::Direction direction)
{
    return tipGameHub().walk(direction);
}

void interruptAutoWalk()
{
    tipGameHub().interruptAutoWalk();
}

void safeLogout()
{
    tipGameHub().safeLogout();
}

void processLogin()
{
    tipGameHub().processLogin();
}

void processGameStart()
{
    tipGameHub().processGameStart();
}

void processPendingGame()
{
    tipGameHub().processPendingGame();
}

void processWorldEntered()
{
    tipGameHub().processWorldEntered();
}

void processPing()
{
    tipGameHub().processPing();
}

void processPingBack()
{
    tipGameHub().processPingBack();
}

void processDeath(uint8_t deathType, uint8_t penality)
{
    tipGameHub().processDeath(deathType, penality);
}

void processLoginError(std::string_view error)
{
    tipGameHub().processLoginError(error);
}

void processLoginAdvice(std::string_view message)
{
    tipGameHub().processLoginAdvice(message);
}

void processLoginWait(std::string_view message, uint8_t time)
{
    tipGameHub().processLoginWait(message, time);
}

void processSessionEnd(uint8_t reason)
{
    tipGameHub().processSessionEnd(reason);
}

void processCharacterSwitch(std::string_view characterName)
{
    tipGameHub().processCharacterSwitch(characterName);
}

void processPlayerHelpers(uint16_t helpers)
{
    tipGameHub().processPlayerHelpers(helpers);
}

void processAttackCancel(uint32_t seq)
{
    tipGameHub().processAttackCancel(seq);
}

void processPlayerModes(tibia::client::FightModes fightMode, tibia::client::ChaseModes chaseMode, bool safeMode,
                        tibia::client::PVPModes pvpMode)
{
    tipGameHub().processPlayerModes(fightMode, chaseMode, safeMode, pvpMode);
}

void processNumericalEffect(tibia::client::MessageMode mode, const Position& pos, uint64_t value, uint8_t color)
{
    tipGameHub().processNumericalEffect(mode, pos, value, color);
}

void processTextMessage(tibia::client::MessageMode mode, std::string_view text)
{
    tipGameHub().processTextMessage(mode, text);
}

void processTalk(std::string_view name, uint16_t level, tibia::client::MessageMode mode, std::string_view text,
                 uint16_t channelId, const Position& pos, uint32_t statement)
{
    tipGameHub().processTalk(name, level, mode, text, channelId, pos, statement);
}

void processVipStateChange(uint32_t id, uint32_t status)
{
    tipGameHub().processVipStateChange(id, status);
}

void processTeamFinderLeader(const TeamFinderLeaderData& data)
{
    tipGameHub().processTeamFinderLeader(data);
}

void processOpenContainer(uint8_t containerId, const ::tibia::container::TInventorySlot& containerItem,
                          std::string_view name, uint8_t capacity, bool hasParent,
                          const std::vector<::tibia::container::TInventorySlot>& items, bool isUnlocked, bool hasPages,
                          bool showSearch, uint16_t containerSize, uint16_t firstIndex)
{
    tipGameHub().processOpenContainer(containerId, containerItem, name, capacity, hasParent, items, isUnlocked, hasPages, showSearch, containerSize, firstIndex);
}

void processCloseContainer(uint8_t containerId)
{
    tipGameHub().processCloseContainer(containerId);
}

void processContainerAddItem(uint8_t containerId, const ::tibia::container::TInventorySlot& item, uint16_t slot)
{
    tipGameHub().processContainerAddItem(containerId, item, slot);
}

void processContainerUpdateItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& item)
{
    tipGameHub().processContainerUpdateItem(containerId, slot, item);
}

void processContainerRemoveItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& lastItem)
{
    tipGameHub().processContainerRemoveItem(containerId, slot, lastItem);
}

void processUpdateNeeded(std::string_view signature)
{
    tipGameHub().processUpdateNeeded(signature);
}

void processSessionDumpStart(std::string_view sessionKey)
{
    tipGameHub().processSessionDumpStart(sessionKey);
}

void processSessionDumpMarker(std::string_view marker)
{
    tipGameHub().processSessionDumpMarker(marker);
}

void processSoundTrigger(uint8_t type, uint16_t soundId)
{
    tipGameHub().processSoundTrigger(type, soundId);
}

void processReadyForSecondaryConnection(std::string_view token)
{
    tipGameHub().processReadyForSecondaryConnection(token);
}

void processOutfitMemorial(const OutfitMemorialData& data)
{
    tipGameHub().processOutfitMemorial(data);
}

void processWalkCancel(tibia::client::Direction direction)
{
    tipGameHub().processWalkCancel(direction);
}

void processTeamFinderList(bool rateLimited, const std::vector<TeamFinderListEntry>& entries)
{
    tipGameHub().processTeamFinderList(rateLimited, entries);
}

void processDepotSearchResults(const std::vector<DepotSearchResultItem>& items)
{
    tipGameHub().processDepotSearchResults(items);
}

void processDepotSearchDetail(const DepotSearchDetail& detail)
{
    tipGameHub().processDepotSearchDetail(detail);
}

void processCloseDepotSearch()
{
    tipGameHub().processCloseDepotSearch();
}

void processChannelList(const std::vector<std::tuple<uint16_t, std::string>>& channelList)
{
    tipGameHub().processChannelList(channelList);
}

void processOpenChannel(uint16_t channelId, std::string_view name)
{
    tipGameHub().processOpenChannel(channelId, name);
}

void processOpenPrivateChannel(std::string_view name)
{
    tipGameHub().processOpenPrivateChannel(name);
}

void processOpenOwnPrivateChannel(uint16_t channelId, std::string_view name)
{
    tipGameHub().processOpenOwnPrivateChannel(channelId, name);
}

void processCloseChannel(uint16_t channelId)
{
    tipGameHub().processCloseChannel(channelId);
}

void processVipAdd(uint32_t id, std::string_view name, uint32_t status, std::string_view description, uint32_t iconId,
                  bool notifyLogin, const std::vector<uint8_t>& groupID)
{
    tipGameHub().processVipAdd(id, name, status, description, iconId, notifyLogin, groupID);
}

void processVipGroupChange(const std::vector<std::tuple<uint8_t, std::string, bool>>& vipGroups, uint8_t groupsAmountLeft)
{
    tipGameHub().processVipGroupChange(vipGroups, groupsAmountLeft);
}

void processTutorialHint(uint8_t id)
{
    tipGameHub().processTutorialHint(id);
}

void processAddAutomapFlag(const Position& pos, uint8_t icon, std::string_view message)
{
    tipGameHub().processAddAutomapFlag(pos, icon, message);
}

void processRemoveAutomapFlag(const Position& pos, uint8_t icon, std::string_view message)
{
    tipGameHub().processRemoveAutomapFlag(pos, icon, message);
}

void processOpenOutfitWindow(const Outfit& currentOutfit,
                             const std::vector<std::tuple<uint16_t, std::string, uint8_t, uint8_t>>& outfitList,
                             const std::vector<std::tuple<uint16_t, std::string, uint8_t>>& mountList,
                             const std::vector<std::tuple<uint16_t, std::string>>& familiarList,
                             const std::vector<std::tuple<uint16_t, std::string>>& wingsList,
                             const std::vector<std::tuple<uint16_t, std::string>>& aurasList,
                             const std::vector<std::tuple<uint16_t, std::string>>& effectsList,
                             const std::vector<std::tuple<uint16_t, std::string>>& shaderList)
{
    tipGameHub().processOpenOutfitWindow(currentOutfit, outfitList, mountList, familiarList, wingsList, aurasList, effectsList, shaderList);
}

void processOpenNpcTrade(const std::vector<std::tuple<::tibia::container::TInventorySlot, std::string, uint32_t, uint32_t, uint32_t>>& items)
{
    tipGameHub().processOpenNpcTrade(items);
}

void processPlayerGoods(uint64_t money, const std::vector<std::pair<uint16_t, uint16_t>>& goods)
{
    tipGameHub().processPlayerGoods(money, goods);
}

void processCloseNpcTrade()
{
    tipGameHub().processCloseNpcTrade();
}

void processOwnTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items)
{
    tipGameHub().processOwnTrade(name, items);
}

void processCounterTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items)
{
    tipGameHub().processCounterTrade(name, items);
}

void processCloseTrade()
{
    tipGameHub().processCloseTrade();
}

void processEditText(uint32_t id, uint32_t itemId, uint16_t maxLength, std::string_view text, std::string_view writer,
                     std::string_view date, bool recentlyTraded)
{
    tipGameHub().processEditText(id, itemId, maxLength, text, writer, date, recentlyTraded);
}

void processEditList(uint32_t id, uint8_t doorId, std::string_view text)
{
    tipGameHub().processEditList(id, doorId, text);
}

void processQuestLog(const std::vector<std::tuple<uint16_t, std::string, bool>>& questList)
{
    tipGameHub().processQuestLog(questList);
}

void processQuestLine(uint16_t questId, const std::vector<std::tuple<std::string, std::string, uint16_t>>& questMissions)
{
    tipGameHub().processQuestLine(questId, questMissions);
}

void processModalDialog(uint32_t id, std::string_view title, std::string_view message,
                        const std::vector<std::tuple<uint8_t, std::string>>& buttonList, uint8_t enterButton,
                        uint8_t escapeButton, const std::vector<std::tuple<uint8_t, std::string>>& choiceList, bool priority)
{
    tipGameHub().processModalDialog(id, title, message, buttonList, enterButton, escapeButton, choiceList, priority);
}

void processItemDetail(const ItemInspectionData& data)
{
    tipGameHub().processItemDetail(data);
}

void processInspectionState(uint32_t creatureId, uint8_t state)
{
    tipGameHub().processInspectionState(creatureId, state);
}

void processCyclopediaCharacterGeneralStats(const CyclopediaCharacterGeneralStats& stats,
                                            const std::vector<std::vector<uint16_t>>& skills,
                                            const std::vector<std::tuple<uint8_t, uint16_t>>& combats)
{
    tipGameHub().processCyclopediaCharacterGeneralStats(stats, skills, combats);
}

void processCyclopediaCharacterCombatStats(const CyclopediaCharacterCombatStats& data, double mitigation,
                                           const std::vector<std::vector<uint16_t>>& additionalSkillsArray,
                                           const std::vector<std::vector<uint16_t>>& forgeSkillsArray,
                                           const std::vector<uint16_t>& perfectShotDamageRangesArray,
                                           const std::vector<std::tuple<uint8_t, uint16_t>>& combatsArray,
                                           const std::vector<std::tuple<uint16_t, uint16_t>>& concoctionsArray)
{
    tipGameHub().processCyclopediaCharacterCombatStats(data, mitigation, additionalSkillsArray, forgeSkillsArray, perfectShotDamageRangesArray, combatsArray, concoctionsArray);
}

void processCyclopediaCharacterGeneralStatsBadge(uint8_t showAccountInformation, uint8_t playerOnline,
                                                 uint8_t playerPremium, std::string_view loyaltyTitle,
                                                 const std::vector<std::tuple<uint32_t, std::string>>& badgesVector)
{
    tipGameHub().processCyclopediaCharacterGeneralStatsBadge(showAccountInformation, playerOnline, playerPremium, loyaltyTitle, badgesVector);
}

void processCyclopediaCharacterInspection(const CyclopediaCharacterInspection& data)
{
    tipGameHub().processCyclopediaCharacterInspection(data);
}

void processCyclopediaCharacterItemSummary(const CyclopediaCharacterItemSummary& data)
{
    tipGameHub().processCyclopediaCharacterItemSummary(data);
}

void processCyclopediaCharacterAppearances(const OutfitColorStruct& currentOutfit,
                                           const std::vector<CharacterInfoOutfits>& outfits,
                                           const std::vector<CharacterInfoMounts>& mounts,
                                           const std::vector<CharacterInfoFamiliar>& familiars)
{
    tipGameHub().processCyclopediaCharacterAppearances(currentOutfit, outfits, mounts, familiars);
}

void processCyclopediaCharacterRecentDeaths(const CyclopediaCharacterRecentDeaths& data)
{
    tipGameHub().processCyclopediaCharacterRecentDeaths(data);
}

void processCyclopediaCharacterRecentPvpKills(const CyclopediaCharacterRecentPvPKills& data)
{
    tipGameHub().processCyclopediaCharacterRecentPvpKills(data);
}

void processParseBestiaryRaces(const std::vector<CyclopediaBestiaryRace>& bestiaryData)
{
    tipGameHub().processParseBestiaryRaces(bestiaryData);
}

void processParseBestiaryOverview(std::string_view raceName, const std::vector<BestiaryOverviewMonsters>& data,
                                  uint16_t animusMasteryPoints)
{
    tipGameHub().processParseBestiaryOverview(raceName, data, animusMasteryPoints);
}

void processUpdateBestiaryMonsterData(const BestiaryMonsterData& data)
{
    tipGameHub().processUpdateBestiaryMonsterData(data);
}

void processUpdateBestiaryCharmsData(const BestiaryCharmsData& charmData)
{
    tipGameHub().processUpdateBestiaryCharmsData(charmData);
}

void processBosstiaryInfo(const std::vector<BosstiaryData>& boss)
{
    tipGameHub().processBosstiaryInfo(boss);
}

void processBosstiarySlots(const BosstiarySlotsData& data)
{
    tipGameHub().processBosstiarySlots(data);
}

void processHighscore(std::string_view serverName, std::string_view world, uint8_t worldType, uint8_t battlEye,
                      const std::vector<std::tuple<uint32_t, std::string>>& vocations,
                      const std::vector<std::tuple<uint8_t, std::string>>& categories, uint16_t page, uint16_t totalPages,
                      const std::vector<std::tuple<uint32_t, std::string, std::string, uint8_t, std::string, uint16_t, uint8_t, uint64_t>>& highscores,
                      uint32_t entriesTs, const std::vector<std::string>& worlds, std::string_view allVocationsName,
                      uint32_t selectedVocation, uint8_t selectedCategory, uint8_t selectedIndex,
                      bool showLoyaltyTitle, uint8_t valueType)
{
    tipGameHub().processHighscore(serverName, world, worldType, battlEye, vocations, categories, page, totalPages, highscores, entriesTs, worlds, allVocationsName, selectedVocation, selectedCategory, selectedIndex, showLoyaltyTitle, valueType);
}

void processCyclopediaCharacterOffenceStats(const CyclopediaCharacterOffenceStats& data)
{
    tipGameHub().processCyclopediaCharacterOffenceStats(data);
}

void processCyclopediaCharacterDefenceStats(const CyclopediaCharacterDefenceStats& data)
{
    tipGameHub().processCyclopediaCharacterDefenceStats(data);
}

void processCyclopediaCharacterMiscStats(const CyclopediaCharacterMiscStats& data)
{
    tipGameHub().processCyclopediaCharacterMiscStats(data);
}

void requestBless()
{
    tipGameHub().requestBless();
}

int openAt(const Position& position, const uint16_t itemId, const int stackPos, const int previousContainerId)
{
    return tipGameHub().openAt(position, itemId, stackPos, previousContainerId);
}

int openAt(const MapPos pos, const uint16_t itemId, const int stackPos, const int previousContainerId)
{
    return openAt(Position(pos.x, pos.y, static_cast<uint8_t>(pos.z)), itemId, stackPos, previousContainerId);
}

void partyJoin(const uint32_t creatureId)
{
    tipGameHub().partyJoin(creatureId);
}

void openContainerQuickLoot(const uint8_t action, const uint8_t category, const Position& pos, const uint16_t itemId, const uint8_t stackpos, const bool useMainAsFallback)
{
    tipGameHub().openContainerQuickLoot(action, category, pos, itemId, stackpos, useMainAsFallback);
}

void useMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    tipGameHub().useMapObject(position, itemId, stackPos);
}

void useMapObject(const MapPos pos, const uint16_t itemId, const int stackPos)
{
    tipGameHub().useMapObject(Position(pos.x, pos.y, static_cast<uint8_t>(pos.z)), itemId, stackPos);
}

void useInventoryItemWithMapObject(const uint16_t itemId, const Position& to, const uint16_t toId, const int toStack)
{
    tipGameHub().useInventoryItemWithMapObject(itemId, to, toId, toStack);
}

void useInventoryItemWithMapObject(const uint16_t itemId, const MapPos to, const uint16_t toId, const int toStack)
{
    tipGameHub().useInventoryItemWithMapObject(itemId, Position(to.x, to.y, static_cast<uint8_t>(to.z)), toId, toStack);
}

void useInventoryItemOnCreature(const uint16_t itemId, const uint32_t creatureId)
{
    tipGameHub().useInventoryItemOnCreature(itemId, creatureId);
}

ItemPtr findPlayerItem(const uint32_t itemId, const int subType)
{
    return tipGameHub().findPlayerItem(itemId, subType);
}

void requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t size, const std::vector<uint16_t>& listedItems)
{
    tipGameHub().requestQuickLootBlackWhiteList(filter, size, listedItems);
}

void sendStatusTrackerBestiary(const uint16_t raceId, const bool status)
{
    tipGameHub().sendStatusTrackerBestiary(raceId, status);
}

void requestBestiarySearch(const uint16_t raceId)
{
    tipGameHub().requestBestiarySearch(raceId);
}

void sendFriendSystemPayload(const std::string& json)
{
    tipGameHub().sendFriendSystemPayload(json);
}

void openPrivateChannel(const std::string_view receiver)
{
    tipGameHub().openPrivateChannel(receiver);
}

void requestTeamFinderList()
{
    tipGameHub().requestTeamFinderList();
}

void requestTeamFinderLeader()
{
    tipGameHub().requestTeamFinderLeader();
}

void joinTeamFinder(const uint32_t leaderId)
{
    tipGameHub().joinTeamFinder(leaderId);
}

void leaveTeamFinder(const uint32_t leaderId)
{
    tipGameHub().leaveTeamFinder(leaderId);
}

void createTeamFinder(const TeamFinderAssemble& assemble)
{
    tipGameHub().createTeamFinder(assemble);
}

void resetTeamFinderLeader()
{
    tipGameHub().resetTeamFinderLeader();
}

void updateTeamFinderMember(const uint32_t memberId, const uint8_t status)
{
    tipGameHub().updateTeamFinderMember(memberId, status);
}

Position localPlayerPosition()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->getPosition();
    return {};
}

MapPos localPlayerMapPos()
{
    const Position pos = localPlayerPosition();
    return { pos.x, pos.y, pos.z };
}

std::string localPlayerName()
{
    if (TPlayerData* data = TPlayerData::liveInstance(); data && !data->characterName().empty())
        return data->characterName();
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->getName();
    return {};
}

uint32_t localPlayerId()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->getId();
    return 0;
}

int localPlayerLevel()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return static_cast<int>(data->level());
    return 0;
}

uint16_t localPlayerLevelPercent()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->levelPercent();
    return 0;
}

uint32_t localPlayerHealth()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->health();
    return 0;
}

uint32_t localPlayerMaxHealth()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->maxHealth();
    return 0;
}

uint32_t localPlayerMana()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->mana();
    return 0;
}

uint32_t localPlayerMaxMana()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->maxMana();
    return 0;
}

uint16_t localPlayerStamina()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->stamina();
    return 0;
}

uint8_t localPlayerVocation()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->vocation();
    return 0;
}

uint8_t localPlayerShield()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->getShield();
    return 0;
}

bool localPlayerHasReachedMain()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->hasReachedMain();
    return false;
}

uint64_t localPlayerTotalMoney()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->totalMoney();
    return 0;
}

uint32_t localPlayerFreeCapacity()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->freeCapacity();
    return 0;
}

uint16_t localPlayerInventoryTypeId(const tibia::client::InventorySlot slot)
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->inventorySlot(slot).typeId;
    return 0;
}

uint32_t localPlayerInventoryCount(const uint16_t itemId, const uint8_t tier)
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->getInventoryCount(itemId, tier);
    return 0;
}

InventorySlotInfo localPlayerInventorySlot(const tibia::client::InventorySlot slot)
{
    InventorySlotInfo info;
    if (const LocalPlayerSessionThingPtr player = localPlayer()) {
        const tibia::container::TInventorySlot& row = player->inventorySlot(slot);
        info.empty = row.empty();
        info.typeId = row.typeId;
        info.stackable = row.stackable;
        info.count = row.count;
        info.charges = row.charges;
    }
    return info;
}

MapPos inventoryWireMapPos(const tibia::client::InventorySlot slot)
{
    return { UINT16_MAX, static_cast<int>(static_cast<uint16_t>(slot)), 0 };
}

void openLocalPlayerInventory(const tibia::client::InventorySlot slot)
{
    const LocalPlayerSessionThingPtr player = localPlayer();
    if (!player)
        return;
    const tibia::container::TInventorySlot& item = player->inventorySlot(slot);
    if (item.empty())
        return;
    openAt(inventoryWireMapPos(slot), item.typeId, 0);
}

void localPlayerStopAutoWalk()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        player->stopAutoWalk();
}

bool localPlayerIsAutoWalking()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->isAutoWalking();
    return false;
}

void localPlayerCancelPredictedStepCollision()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        player->cancelPredictedStepCollision();
}

bool localPlayerAutoWalk(const Position& goal, const bool retry,
                         const tibia::client::AutoWalkPathMode pathMode)
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->autoWalk(goal, retry, pathMode);
    return false;
}

bool localPlayerAutoWalk(const MapPos goal, const bool retry,
                         const tibia::client::AutoWalkPathMode pathMode)
{
    return localPlayerAutoWalk(Position(goal.x, goal.y, static_cast<uint8_t>(goal.z)), retry, pathMode);
}

uint8_t localPlayerBlessVisualState()
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        return data->blessVisualState();
    return 0;
}

void localPlayerSetInformationChannel(const uint32_t durationMs)
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        player->setInformationChannel(durationMs);
}

void localPlayerClearInformationChannel()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        player->clearInformationChannel();
}

void localPlayerSetSoftDead(const bool softDead)
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        player->setSoftDead(softDead);
}

Outfit localPlayerOutfit()
{
    if (const LocalPlayerSessionThingPtr player = localPlayer())
        return player->getOutfit();
    return {};
}

} // namespace tibia::game
