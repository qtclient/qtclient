#include "gamesession.h"

#include "game/gamesessionstate.h"
#include "game/playerdata.h"
#include "game/playerskillstats.h"
#include <tibia/creatures/creaturestorage.h>
#include <tibia/game/localplayersessionthing.h>
#include <tibia/game/mapsession.h>
#include "input/playermovementintenthandler.h"
#include "worldmap/worldmapfield.h"
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/config/runtimegameconfig.h>
#include "shared/clockservice.h"
#include <tibia/game/sessionhooks.h>
#include <tibia/game/tgamenotify.h>
#include <vector>

#include "tibia/runtime/host.h"
#include "tibia/gamewindow/remainingdialogs.h"
#include <tibia/game/gamenotify.h>
#include <tibia/creatures/attachedeffect.h>
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "app/clientoptions.h"
#include "config/clientoptiondefaults.h"
#include "minimap/waypointcosts.h"
#include "protocol/protocolcodes.h"
#include "tibia/protocol/extendedopcode.h"
#include "tibia/assets/thingtype.h"
#include "tibia/assets/things.h"
#include "container/containerciptypes.h"
#include "creatures/creaturestorage.h"
#include "creatures/walkdebug.h"
#include "shared/clockservice.h"
#include <QStringList>
#include <tibia/schedule/schedule.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gameservergamesession.h>
#include <tibia/game/gamesession.h>
#include <QString>
#include <tibia/game/thingsession_forward.h>
#include "protocol/clientmessages.h"

namespace tibia::game {

namespace gameclient = ::tibia::protocol::gameclient;

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
    // Mirror TGameserverGameSession notify → signal wiring on the qtclient twin.
    // Does not call loginWorld / forceLogout / cancelLogin.
    auto* fans = m_liveFans;
    if (!fans)
        return;

    // Same SoftHook fields as TGameserverGameSession (minus the connection's
    // onConnect — a dump has no socket, so `connected` follows `onLogin`).
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
    // flag — qtclient does not write g_game (avoids fighting GameClient config).
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
    return static_cast<uint64_t>(tibia::shared::TClockService::frameMillis());
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
    return gameSessionState().getLocalPlayer();
}


bool isOnline()
{
    return gameSessionState().isOnline();
}

bool isDead()
{
    return gameSessionState().isDead();
}

bool isSceneFrozen()
{
    return gameSessionState().isSceneFrozen();
}

void setSceneFrozen(const bool frozen)
{
    gameSessionState().setSceneFrozen(frozen);
}

bool isSwitchingCharacter()
{
    return gameSessionState().isSwitchingCharacter();
}

std::string characterName()
{
    return gameSessionState().getCharacterName();
}

int clientRevision()
{
    return gameSessionState().getClientVersion();
}
tibia::client::OperatingSystem_t clientOs()
{
    // The client speaks CipSoft NEW_* OS ids, so Crystal keeps the CIP wire.
    if (tibia::host::app().getOs() == "windows")
        return tibia::client::CLIENTOS_TIBIA_NEW_WINDOWS;

    if (tibia::host::app().getOs() == "mac")
        return tibia::client::CLIENTOS_TIBIA_NEW_MAC;

    return tibia::client::CLIENTOS_TIBIA_NEW_LINUX;
}

void setProtocolVersion(const uint16_t version)
{
    gameSessionState().setProtocolVersion(version);
}

void setClientRevision(const uint16_t version)
{
    gameSessionState().setClientVersion(version);
}
void initGameHub()
{
    gameSessionState().resetGameStates();
}

void terminateGameHub()
{
    gameSessionState().terminate();
}

void processConnectionError(const std::string_view message, const bool clean)
{
    gameSessionState().processConnectionError(message, clean);
}

void startDecodedReplay()
{
    gameSessionState().startDecodedReplay();
}

void deliverReplayPacket(const uint8_t* data, size_t size)
{
    gameSessionState().deliverReplayPacket(data, size);
}

void finishDecodedReplay()
{
    gameSessionState().finishDecodedReplay();
}
std::string formatCreatureName(const std::string_view name)
{
    std::string formatedName{ name };
    if (name.length() > 0) {
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

uint32_t attackingCreatureId()
{
    return gameSessionState().getAttackingCreatureId();
}

uint32_t followingCreatureId()
{
    return gameSessionState().getFollowingCreatureId();
}

void setAttackingCreatureId(const uint32_t creatureId)
{
    gameSessionState().setAttackingCreatureId(creatureId);
}
void talk(const std::string_view message)
{
    if (!canPerformGameAction() || message.empty())
        return;

    talkChannel(tibia::client::MessageSay, 0, message);
}
void turn(const tibia::client::Direction direction)
{
    if (!canPerformGameAction())
        return;

    switch (direction) {
        case tibia::client::North:
            gameclient::sendTurnNorth();
            break;
        case tibia::client::East:
            gameclient::sendTurnEast();
            break;
        case tibia::client::South:
            gameclient::sendTurnSouth();
            break;
        case tibia::client::West:
            gameclient::sendTurnWest();
            break;
        default:
            break;
    }
}

bool walk(const tibia::client::Direction direction)
{
    return gameSessionState().walk(direction);
}

void interruptAutoWalk()
{
    gameSessionState().interruptAutoWalk();
}
void safeLogout()
{
    if (!isOnline())
        return;

    gameclient::sendLogout();
}
void processLogin()
{
    tibia::game::TGameserverGameSession::wireProcessLogin();
}

void processGameStart()
{
    gameSessionState().processGameStart();
}

void processPendingGame()
{
    gameSessionState().processPendingGame();
}
void processWorldEntered()
{
    tibia::game::TGameserverGameSession::wireProcessWorldEntered();
}
void processPing()
{
    tibia::game::TGameserverGameSession::wireProcessPing();
}
void processPingBack()
{
    tibia::game::TGameserverGameSession::wireProcessPingBack();
}
void processDeath(const uint8_t deathType, const uint8_t penality)
{
    tibia::game::TGameserverGameSession::wireProcessDeath(deathType, penality);
}
void processLoginError(const std::string_view error)
{
    tibia::game::TGameserverGameSession::wireProcessLoginError(error);
}
void processLoginAdvice(const std::string_view message)
{
    tibia::game::TGameserverGameSession::wireProcessLoginAdvice(message);
}
void processLoginWait(const std::string_view message, const uint8_t time)
{
    tibia::game::TGameserverGameSession::wireProcessLoginWait(message, time);
}
void processSessionEnd(const uint8_t reason)
{
    tibia::game::TGameserverGameSession::wireProcessSessionEnd(reason);
}

void processCharacterSwitch(std::string_view characterName)
{
    gameSessionState().processCharacterSwitch(characterName);
}
void processPlayerHelpers(const uint16_t helpers)
{
    tibia::game::notify("onPlayerHelpersUpdate", helpers);
}

void processUpdateTarget(const uint32_t seq, const uint32_t targetId)
{
    gameSessionState().processUpdateTarget(seq, targetId);
}

void processPlayerTactics(const tibia::client::ChaseModes chaseMode, const bool safeMode, const tibia::client::PVPModes pvpMode)
{
    auto& hub = gameSessionState();
    hub.processPlayerModes(hub.getFightMode(), chaseMode, safeMode, pvpMode);
}

void processPlayerModes(tibia::client::FightModes fightMode, tibia::client::ChaseModes chaseMode, bool safeMode,
                        tibia::client::PVPModes pvpMode)
{
    gameSessionState().processPlayerModes(fightMode, chaseMode, safeMode, pvpMode);
}
void processNumericalEffect(const tibia::client::MessageMode mode, const Position& pos, const uint64_t value, const uint8_t color)
{
    tibia::game::notify("onNumericalEffect", mode, pos, value, color);
}
void processTextMessage(const tibia::client::MessageMode mode, const std::string_view text)
{
    tibia::game::notify("onTextMessage", mode, text);
    tibia::game::TGameNotify::instance()->emitTextMessage(static_cast<int>(mode),
                                                          QString::fromUtf8(text.data(), static_cast<int>(text.size())));
}
void processTalk(const std::string_view name, const uint16_t level, const tibia::client::MessageMode mode, const std::string_view text, const uint16_t channelId, const Position& pos, const uint32_t statement)
{
    tibia::game::notify("onTalk", name, level, mode, text, channelId, pos, statement);
}

void processTeamFinderLeader(const TeamFinderLeaderData& data)
{
    tibia::game::notify("onTeamFinderLeader", data);
}

void processOpenContainer(uint8_t containerId, const ::tibia::container::TInventorySlot& containerItem,
                          std::string_view name, uint8_t capacity, bool hasParent,
                          const std::vector<::tibia::container::TInventorySlot>& items, bool isUnlocked, bool hasPages,
                          bool showSearch, uint16_t containerSize, uint16_t firstIndex)
{
    gameSessionState().processOpenContainer(containerId, containerItem, name, capacity, hasParent, items, isUnlocked, hasPages, showSearch, containerSize, firstIndex);
}
void processCloseContainer(const uint8_t containerId)
{
    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance())
        storage->closeContainer(containerId);
}

void processContainerAddItem(uint8_t containerId, const ::tibia::container::TInventorySlot& item, uint16_t slot)
{
    gameSessionState().processContainerAddItem(containerId, item, slot);
}

void processContainerUpdateItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& item)
{
    gameSessionState().processContainerUpdateItem(containerId, slot, item);
}

void processContainerRemoveItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& lastItem)
{
    gameSessionState().processContainerRemoveItem(containerId, slot, lastItem);
}
void processUpdateNeeded(const std::string_view signature)
{
    tibia::game::notify("onUpdateNeeded", signature);
}
void processSessionDumpStart(const std::string_view sessionKey)
{
    tibia::game::notify("onSessionDumpStart", std::string(sessionKey));
}
void processSessionDumpMarker(const std::string_view marker)
{
    tibia::game::notify("onSessionDumpMarker", std::string(marker));
}
void processSoundTrigger(const uint8_t type, const uint16_t soundId)
{
    tibia::game::notify("onSoundTrigger", type, soundId);
}

void processReadyForSecondaryConnection(std::string_view token)
{
    gameSessionState().processReadyForSecondaryConnection(token);
}
void processOutfitMemorial(const OutfitMemorialData& data)
{
    if (auto* dialog = tibia::gamewindow::TOutfitMemorialDialogController::instance())
        dialog->showMemorial(data);
    else
        tibia::game::notify("onOutfitMemorial", data);
}
void processWalkCancel(const tibia::client::Direction direction)
{
    tibia::game::localPlayerCancelWalk(direction);
}
void processTeamFinderList(const bool rateLimited, const std::vector<TeamFinderListEntry>& entries)
{
    tibia::game::notify("onTeamFinderList", rateLimited, entries);
}
void processDepotSearchResults(const std::vector<DepotSearchResultItem>& items)
{
    tibia::game::notify("onDepotSearchResults", items);
}
void processDepotSearchDetail(const DepotSearchDetail& detail)
{
    tibia::game::notify("onDepotSearchDetail", detail);
}
void processCloseDepotSearch()
{
    tibia::game::notify("onCloseDepotSearch");
}
void processChannelList(const std::vector<std::tuple<uint16_t, std::string>>& channelList)
{
    tibia::game::notify("onChannelList", channelList);
}
void processOpenChannel(const uint16_t channelId, const std::string_view name)
{
    tibia::game::notify("onOpenChannel", channelId, name);
}
void processOpenPrivateChannel(const std::string_view name)
{
    tibia::game::notify("onOpenPrivateChannel", name);
}
void processOpenOwnPrivateChannel(const uint16_t channelId, const std::string_view name)
{
    tibia::game::notify("onOpenOwnPrivateChannel", channelId, name);
}
void processCloseChannel(const uint16_t channelId)
{
    tibia::game::notify("onCloseChannel", channelId);
}

void processTutorialHint(const uint8_t id)
{
    tibia::game::notify("onTutorialHint", id);
}
void processAddAutomapFlag(const Position& pos, const uint8_t icon, const std::string_view message)
{
    tibia::game::notify("onAddAutomapFlag", pos, icon, message);
}
void processRemoveAutomapFlag(const Position& pos, const uint8_t icon, const std::string_view message)
{
    tibia::game::notify("onRemoveAutomapFlag", pos, icon, message);
}
void processOpenOutfitWindow(const Outfit& currentOutfit, const std::vector<std::tuple<uint16_t, std::string, uint8_t, uint8_t>>& outfitList, const std::vector<std::tuple<uint16_t, std::string, uint8_t>>& mountList, const std::vector<std::tuple<uint16_t, std::string>>& familiarList)
{
    tibia::game::notify("onOpenOutfitWindow", currentOutfit, outfitList, mountList, familiarList);
}

void processOpenNpcTrade(const std::vector<std::tuple<::tibia::container::TInventorySlot, std::string, uint32_t, uint32_t, uint32_t>>& items)
{
    gameSessionState().processOpenNpcTrade(items);
}
void processPlayerGoods(const uint64_t money, const std::vector<std::pair<uint16_t, uint16_t>>& goods)
{
    tibia::game::notify("onPlayerGoods", money, goods);
}
void processCloseNpcTrade()
{
    tibia::game::notify("onCloseNpcTrade");
}

void processOwnTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items)
{
    gameSessionState().processOwnTrade(name, items);
}

void processCounterTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items)
{
    gameSessionState().processCounterTrade(name, items);
}
void processCloseTrade()
{
    tibia::game::notify("onCloseTrade");
}
void processEditText(const uint32_t id, const uint32_t itemId, const uint16_t maxLength, const std::string_view text, const std::string_view writer, const std::string_view date, const bool recentlyTraded)
{
    tibia::game::notify("onEditText", id, itemId, maxLength, text, writer, date, recentlyTraded);
}
void processEditList(const uint32_t id, const uint8_t doorId, const std::string_view text)
{
    tibia::game::notify("onEditList", id, doorId, text);
}
void processQuestLog(const std::vector<std::tuple<uint16_t, std::string, bool>>& questList)
{
    tibia::game::notify("onQuestLog", questList);
}
void processQuestLine(const uint16_t questId, const std::vector<std::tuple<std::string, std::string, uint16_t>>& questMissions)
{
    tibia::game::notify("onQuestLine", questId, questMissions);
}

void processModalDialog(uint32_t id, std::string_view title, std::string_view message,
                        const std::vector<std::tuple<uint8_t, std::string>>& buttonList, uint8_t enterButton,
                        uint8_t escapeButton, const std::vector<std::tuple<uint8_t, std::string>>& choiceList, bool priority)
{
    gameSessionState().processModalDialog(id, title, message, buttonList, enterButton, escapeButton, choiceList, priority);
}
void processItemDetail(const ItemInspectionData& data)
{
    tibia::game::notify("onParseItemDetail", data);
}
void processInspectionState(const uint32_t creatureId, const uint8_t state)
{
    tibia::game::notify("onInspectionState", creatureId, state);
}
void processCyclopediaCharacterGeneralStats(const CyclopediaCharacterGeneralStats& stats, const std::vector<std::vector<uint16_t>>& skills, const std::vector<std::tuple<uint8_t, uint16_t>>& combats)
{
    tibia::game::notify("onParseCyclopediaCharacterGeneralStats", stats, skills, combats);
}
void processCyclopediaCharacterCombatStats(const CyclopediaCharacterCombatStats& data, const double mitigation, const std::vector<std::vector<uint16_t>>& additionalSkillsArray, const std::vector<std::vector<uint16_t>>& forgeSkillsArray, const std::vector<uint16_t>& perfectShotDamageRangesArray, const std::vector<std::tuple<uint8_t, uint16_t>>& combatsArray, const std::vector<std::tuple<uint16_t, uint16_t>>& concoctionsArray)
{
    tibia::game::notify("onParseCyclopediaCharacterCombatStats", data, mitigation, additionalSkillsArray, forgeSkillsArray, perfectShotDamageRangesArray, combatsArray, concoctionsArray);
}
void processCyclopediaCharacterGeneralStatsBadge(const uint8_t showAccountInformation, const uint8_t playerOnline, const uint8_t playerPremium, const std::string_view loyaltyTitle, const std::vector<std::tuple<uint32_t, std::string>>& badgesVector)
{
    tibia::game::notify("onParseCyclopediaCharacterBadges", showAccountInformation, playerOnline, playerPremium, loyaltyTitle, badgesVector);
}
void processCyclopediaCharacterInspection(const CyclopediaCharacterInspection& data)
{
    tibia::game::notify("onParseCyclopediaCharacterInspection", data);
}
void processCyclopediaCharacterItemSummary(const CyclopediaCharacterItemSummary& data)
{
    tibia::game::notify("onUpdateCyclopediaCharacterItemSummary", data);
}
void processCyclopediaCharacterAppearances(const OutfitColorStruct& currentOutfit, const std::vector<CharacterInfoOutfits>& outfits, const std::vector<CharacterInfoMounts>& mounts, const std::vector<CharacterInfoFamiliar>& familiars)
{
    tibia::game::notify("onParseCyclopediaCharacterAppearances", currentOutfit, outfits, mounts, familiars);
}
void processCyclopediaCharacterRecentDeaths(const CyclopediaCharacterRecentDeaths& data)
{
    tibia::game::notify("onCyclopediaCharacterRecentDeaths", data);
}
void processCyclopediaCharacterRecentPvpKills(const CyclopediaCharacterRecentPvPKills& data)
{
    tibia::game::notify("onCyclopediaCharacterRecentKills", data);
}
void processParseBestiaryRaces(const std::vector<CyclopediaBestiaryRace>& bestiaryData)
{
    tibia::game::notify("onParseBestiaryRaces", bestiaryData);
}
void processParseBestiaryOverview(const std::string_view raceName, const std::vector<BestiaryOverviewMonsters>& data, const uint16_t animusMasteryPoints)
{
    tibia::game::notify("onParseBestiaryOverview", raceName, data, animusMasteryPoints);
}
void processUpdateBestiaryMonsterData(const BestiaryMonsterData& data)
{
    tibia::game::notify("onUpdateBestiaryMonsterData", data);
}
void processUpdateBestiaryCharmsData(const BestiaryCharmsData& charmData)
{
    tibia::game::notify("onUpdateBestiaryCharmsData", charmData);
}
void processBosstiaryInfo(const std::vector<BosstiaryData>& boss)
{
    tibia::game::notify("onParseSendBosstiary", boss);
}
void processBosstiarySlots(const BosstiarySlotsData& data)
{
    tibia::game::notify("onParseBosstiarySlots", data);
}
void processCyclopediaCharacterOffenceStats(const CyclopediaCharacterOffenceStats& data)
{
    tibia::game::notify("onCyclopediaCharacterOffenceStats", data);
}
void processCyclopediaCharacterDefenceStats(const CyclopediaCharacterDefenceStats& data)
{
    tibia::game::notify("onCyclopediaCharacterDefenceStats", data);
}
void processCyclopediaCharacterMiscStats(const CyclopediaCharacterMiscStats& data)
{
    tibia::game::notify("onCyclopediaCharacterMiscStats", data);
}

void requestBless()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestBless();
}
int openAt(const Position& position, const uint16_t itemId, const int stackPos, const int previousContainerId)
{
    if (!canPerformGameAction() || itemId == 0 || stackPos < 0)
        return -1;

    const int id = previousContainerId >= 0 ? previousContainerId : gameSessionState().findEmptyContainerId();
    gameclient::sendUseItem(position, itemId, static_cast<uint8_t>(stackPos), id);

    tibia::game::notify("onUse", position, itemId, stackPos, 0);

    return id;
}

int openAt(const MapPos pos, const uint16_t itemId, const int stackPos, const int previousContainerId)
{
    return openAt(Position(pos.x, pos.y, static_cast<uint8_t>(pos.z)), itemId, stackPos, previousContainerId);
}

void partyJoin(const uint32_t creatureId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendJoinParty(creatureId);
}

void openContainerQuickLoot(const uint8_t action, const uint8_t category, const Position& pos, const uint16_t itemId, const uint8_t stackpos, const bool useMainAsFallback)
{
    if (!canPerformGameAction())
        return;
    gameclient::openContainerQuickLoot(action, category, pos, itemId, stackpos, useMainAsFallback);
}
void useMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    if (!canPerformGameAction() || !position.isValid() || itemId == 0 || stackPos < 0)
        return;
    gameclient::sendUseItem(position, itemId, static_cast<uint8_t>(stackPos), gameSessionState().findEmptyContainerId());
    tibia::game::notify("onUse", position, itemId, stackPos, 0);
}

void useMapObject(const MapPos pos, const uint16_t itemId, const int stackPos)
{
    useMapObject(Position(pos.x, pos.y, static_cast<uint8_t>(pos.z)), itemId, stackPos);
}
void useInventoryItemWithMapObject(const uint16_t itemId, const Position& to, const uint16_t toId, const int toStack)
{
    if (!canPerformGameAction() || !to.isValid() || toId == 0 || toStack < 0)
        return;

    // pre-780: resolve real inventory slot when possible (classic wire stackpos).
    if (gameSessionState().getClientVersion() < 780) {
        if (const auto item = findPlayerItem(itemId, -1)) {
            Position pos = item->position;
            if (!pos.isValid())
                pos = Position(0xFFFF, 0, 0);
            gameclient::sendUseItemWith(pos, item->typeId, static_cast<uint8_t>(item->stackPos), to, toId,
                                            static_cast<uint8_t>(toStack));
            tibia::game::notify("onUseWith", pos, itemId, to, item->stackPos);
            return;
        }
    }

    const auto& pos = Position(0xFFFF, 0, 0);
    gameclient::sendUseItemWith(pos, itemId, 0, to, toId, static_cast<uint8_t>(toStack));
    tibia::game::notify("onUseWith", pos, itemId, to, 0);
}

void useInventoryItemWithMapObject(const uint16_t itemId, const MapPos to, const uint16_t toId, const int toStack)
{
    useInventoryItemWithMapObject(itemId, Position(to.x, to.y, static_cast<uint8_t>(to.z)), toId, toStack);
}
void useInventoryItemOnCreature(const uint16_t itemId, const uint32_t creatureId)
{
    if (!canPerformGameAction() || !creatureId)
        return;

    if (gameSessionState().getClientVersion() < 780) {
        if (const auto item = findPlayerItem(itemId, -1)) {
            gameSessionState().useOnCreature(*item, creatureId);
            return;
        }
    }

    const auto& pos = Position(0xFFFF, 0, 0);
    gameclient::sendUseOnCreature(pos, itemId, 0, creatureId);
    tibia::game::notify("onUseWith", pos, itemId, creatureId, 0);
}
std::optional<TObjectReference> findPlayerItem(const uint32_t itemId, const int subType)
{
    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance()) {
        for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot) {
            const tibia::container::TInventorySlot& row = inventory->slotAt(slot);
            if (row.empty() || row.typeId != itemId)
                continue;
            const uint8_t fluidOrSub = row.fluidOrSplash ? row.fluid : 0;
            if (subType != -1 && fluidOrSub != static_cast<uint8_t>(subType))
                continue;
            return TObjectReference{ row.typeId, Position(UINT16_MAX, static_cast<uint16_t>(slot), 0), 0,
                                     static_cast<uint16_t>(row.stackable ? row.count : 0), row.tier, fluidOrSub };
        }
    }

    return gameSessionState().findItemInContainers(itemId, subType, 0);
}

void requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t size, const std::vector<uint16_t>& listedItems)
{
    gameSessionState().requestQuickLootBlackWhiteList(filter, size, listedItems);
}

void sendStatusTrackerBestiary(const uint16_t raceId, const bool status)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendStatusTrackerBestiary(raceId, status);
}

void requestBestiarySearch(const uint16_t raceId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendRequestBestiarySearch(raceId);
}
void openPrivateChannel(const std::string_view receiver)
{
    if (!canPerformGameAction() || receiver.empty())
        return;

    gameclient::sendOpenPrivateChannel(receiver);
}

void requestTeamFinderList()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeamFinderMemberAction(0);
}

void requestTeamFinderLeader()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeamFinderLeaderAction(0);
}

void joinTeamFinder(const uint32_t leaderId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeamFinderMemberAction(1, leaderId);
}

void leaveTeamFinder(const uint32_t leaderId)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeamFinderMemberAction(2, leaderId);
}

void createTeamFinder(const TeamFinderAssemble& assemble)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeamFinderLeaderAction(3, 0, 0, assemble);
}

void resetTeamFinderLeader()
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeamFinderLeaderAction(1);
}

void updateTeamFinderMember(const uint32_t memberId, const uint8_t status)
{
    if (!canPerformGameAction())
        return;
    gameclient::sendTeamFinderLeaderAction(2, memberId, status);
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
        return static_cast<uint16_t>(data->levelPercent());
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
    // The classic vocation id: promoted vocations count from 11.
    if (TPlayerData* data = TPlayerData::liveInstance())
        return static_cast<uint8_t>(data->vocation() + (data->promoted() ? 10 : 0));
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

namespace {

tibia::input::TPlayerMovementIntentHandler* movementIntent()
{
    return gameSessionState().movementIntent();
}

}

void localPlayerStopAutoWalk()
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent())
        intent->stopWalking();
}

bool localPlayerIsAutoWalking()
{
    const tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    return intent && intent->hasGoal();
}

bool localPlayerHasAutoWalkDirs()
{
    return localPlayerIsAutoWalking();
}

bool localPlayerIsWalking()
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const auto row = storage ? storage->creatureAt(localPlayerId()) : nullptr;
    return row && row->isMoving();
}

bool localPlayerIsPreWalking()
{
    const tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    return intent && intent->isPredicting();
}

bool localPlayerAutoWalk(const Position& goal, const bool retry, const tibia::client::AutoWalkPathMode pathMode,
                         const bool enterUnwalkableGoal)
{
    (void)retry;
    (void)pathMode;
    tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    if (!intent || !goal.isValid() || !isOnline())
        return false;
    intent->walkTo(tibia::worldmap::TWorldMapCoordinate(goal), enterUnwalkableGoal);
    return intent->hasGoal();
}

bool localPlayerAutoWalk(const MapPos goal, const bool retry, const tibia::client::AutoWalkPathMode pathMode)
{
    return localPlayerAutoWalk(Position(goal.x, goal.y, static_cast<uint8_t>(goal.z)), retry, pathMode);
}

void localPlayerHoldDirection(const tibia::client::Direction direction)
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent(); intent && isOnline())
        intent->walkInDirection(tibia::input::toMovementDirection(direction));
}

void localPlayerRotate(const tibia::client::Direction direction)
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent(); intent && isOnline() && direction <= tibia::client::West)
        intent->rotate(static_cast<tibia::creatures::ECreatureLookDirection>(direction));
}

void localPlayerClearMovementGoal()
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent())
        intent->clearGoal();
}

void localPlayerReleaseDirection()
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent())
        intent->stopIntent();
}

void localPlayerMovementTick()
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent(); intent && isOnline())
        intent->tick();
}

void onLocalPlayerPositionChanged(const Position& position)
{
    if (tibia::input::TPlayerMovementIntentHandler* intent = movementIntent())
        intent->onPositionChanged(tibia::worldmap::TWorldMapCoordinate(position));
}

void localPlayerCancelWalk(const tibia::client::Direction direction)
{
    tibia::input::TPlayerMovementIntentHandler* intent = movementIntent();
    if (!intent)
        return;
    const auto look = direction <= tibia::client::West ? static_cast<tibia::creatures::ECreatureLookDirection>(direction)
                                                       : static_cast<tibia::creatures::ECreatureLookDirection>(localPlayerDirection());
    intent->onCancelWalk(look);
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
