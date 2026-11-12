#pragma once

#include <client/declarations.h>
#include <client/const.h>
#include <client/outfit.h>
#include <client/position.h>

#include <tibia/game/gamesession_forward.h>

#include "mapsession.h"

#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <system_error>
#include <string_view>
#include <vector>

// Classic nested braces (readable; matches tip style after compound-ns cleanup).
namespace tibia {
namespace creatures {
class TCreature;
class TCreatureStorage;
}
namespace game {

/// CipSoft-shaped session / wire façade for qtclient + packs.
/// Prefer this header over `#include "gamehub_impl.h"` at call sites.
/// Implementation peels the private OTC hub via `qtclientGameHub()` in `gamehub.*`.
/// migrates each method onto TGameClient / IGameSession / storages.

/// Pack-facing monotonic ms (`g_clock.millis`). Header free of `framework/core/clock.h`.
uint64_t millis();

/// Map sea floor (`tibia::config::mapSeaFloor`). Header free of engine gameconfig.
uint8_t mapSeaFloor();
/// World ambient light intensity (`mapsession::worldLight`). Header free of `world.h`/`map.h`.
uint8_t worldLightIntensity();
/// World ambient light color (`mapsession::worldLight`). Header free of `world.h`/`map.h`.
uint8_t worldLightColor();
/// Map item revision for roof/geometry cache (`mapsession::itemRevision`).
uint32_t mapItemRevision();
uint32_t mapDynamicRevision();

/// World-time / session-end hooks (`tibia::world::subscribe*` in `game/sessionhooks`).
/// Header free of `world.h`.
int subscribeWorldTime(std::function<void(int hour, int minute)> listener);
void unsubscribeWorldTime(int id);
int subscribeSessionEnd(std::function<void()> listener);
void unsubscribeSessionEnd(int id);

LocalPlayerSessionThingPtr localPlayer();


/// Pack-facing LocalPlayerSessionThing peels — prefer these over `#include <tibia/game/localplayersessionthing.h>`.
/// Out-of-line so the allowlisted header does not pull the full LocalPlayerSessionThing type.
Position localPlayerPosition();
/// Pack-facing position peel (`MapPos` — prefer over `localPlayerPosition` / `Position`).
MapPos localPlayerMapPos();
std::string localPlayerName();
uint32_t localPlayerId();
/// Retired Map Creature keep-alive (Phase 5.0-B). Always nullptr — use TCreatureStorage + LocalPlayerSessionThing peels.
CreaturePtr creatureThingById(uint32_t id);
int localPlayerLevel();
uint16_t localPlayerLevelPercent();
uint32_t localPlayerHealth();
uint32_t localPlayerMaxHealth();
uint32_t localPlayerMana();
uint32_t localPlayerMaxMana();
uint32_t localPlayerManaShield();
uint32_t localPlayerMaxManaShield();
uint64_t localPlayerExperience();
uint16_t localPlayerStamina();
uint8_t localPlayerVocation();
uint8_t localPlayerShield();
bool localPlayerHasReachedMain();
bool localPlayerIsPremium();
bool localPlayerIsMagicShieldVocation();
uint8_t localPlayerHarmony();
bool localPlayerIsSerene();
uint64_t localPlayerResourceBalance(tibia::client::ResourceTypes_t type);
bool localPlayerHasResourceBalance(tibia::client::ResourceTypes_t type);
uint64_t localPlayerTotalMoney();
uint32_t localPlayerFreeCapacity();
uint16_t localPlayerInventoryTypeId(tibia::client::InventorySlot slot);
uint32_t localPlayerInventoryCount(uint16_t itemId, uint8_t tier = 0);
/// Pack-facing inventory slot peel (exercise waiting-weapon / backpack scan).
/// No `TInventorySlot` / LocalPlayerSessionThing in the header.
struct InventorySlotInfo {
    bool empty{ true };
    uint16_t typeId{ 0 };
    bool stackable{ false };
    uint16_t count{ 0 };
    uint32_t charges{ 0 };
};
InventorySlotInfo localPlayerInventorySlot(tibia::client::InventorySlot slot);
/// Open inventory container at `slot` via `openAt` when the slot is non-empty.
void openLocalPlayerInventory(tibia::client::InventorySlot slot);
/// Wire form for inventory open/use: `Position(UINT16_MAX, slot, 0)` as `MapPos`.
MapPos inventoryWireMapPos(tibia::client::InventorySlot slot);

/// Auto-walk peels (TravelRouter). Prefer over LocalPlayerSessionThing::autoWalk / stopAutoWalk.
void localPlayerStopAutoWalk();
bool localPlayerIsAutoWalking();
bool localPlayerHasAutoWalkDirs();
bool localPlayerIsWalking();
bool localPlayerIsPreWalking();
void localPlayerResetPreWalk();
/// Predicted-step collision cancel (TCreatureMovementTween). Prefer over LocalPlayerSessionThing::cancelPredictedStepCollision.
void localPlayerCancelPredictedStepCollision();
/// Chain next GoPath client prediction at tween end / NFU. Prefer over LocalPlayerSessionThing::tryBeginNextGoPathStep.
void localPlayerTryBeginNextGoPathStep();
/// CipSoft: TCreatureMovementTween terminate → GoPath chain / deferred chunk continue.
void onLocalPlayerMovementTweenTerminated();
/// CipSoft client prediction: push prewalk + begin TCreatureMovementTween (no Map Creature clock).
bool predictLocalPlayerStep(tibia::client::Direction direction);
/// Arm remaining GoPath dirs for client-side step prediction.
void armLocalPlayerGoPathDirs(const std::vector<tibia::client::Direction>& dirs);
bool localPlayerAutoWalk(const Position& goal, bool retry = false,
                         tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);
/// Pack-facing autoWalk (`MapPos` — prefer for travel / packs).
bool localPlayerAutoWalk(MapPos goal, bool retry = false,
                         tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);

/// Bless chrome (attention). Matches LocalPlayerSessionThing::getBlessVisualState.
uint8_t localPlayerBlessVisualState();

/// Player state icon bits (EPlayerStateFlag). LocalPlayerSessionThing authority until TPlayerData stores states.
uint64_t localPlayerStates();
uint8_t localPlayerSoul();
uint16_t localPlayerMagicLevel();
uint16_t localPlayerBaseMagicLevel();
uint16_t localPlayerMagicLevelPercent();
const std::vector<uint16_t>& localPlayerSpells();
tibia::client::Direction localPlayerDirection();
void localPlayerStopPendingWalk();
void localPlayerHandleTeleport();
void localPlayerRegisterAdjustInvalidPosEvent();
bool localPlayerIsSoftDead();
/// Keep-alive Map Creature → TCreature HUD mirror (Phase 5.0-C).
void syncLocalPlayerStorageRow(tibia::creatures::TCreature& row);
void syncLocalPlayerStorageWorldDraw();

uint8_t localPlayerSkull();
uint16_t localPlayerBlessings();
uint32_t localPlayerBaseCapacity();
uint32_t localPlayerTotalCapacity();
bool localPlayerHasEquippedItemId(uint16_t itemId, uint8_t tier = 0);
bool localPlayerIsMarketAvailable();
bool localPlayerIsSupplyStashAvailable();
bool localPlayerIsStoreExpBoostPurchasable();

/// Instance channel / soft-death peels.
void localPlayerSetInformationChannel(uint32_t durationMs);
void localPlayerClearInformationChannel();
void localPlayerSetSoftDead(bool softDead);

/// Outfit for HUD chrome (teamhud). Prefer over LocalPlayerSessionThing::getOutfit.
Outfit localPlayerOutfit();

bool isOnline();


bool isDead();


bool isSceneFrozen();


void setSceneFrozen(const bool frozen);


bool isSwitchingCharacter();


std::string characterName();


int clientRevision();


/// Login wire OS field (`OperatingSystem_t` as u16).
tibia::client::OperatingSystem_t clientOs();


/// Feature flags / client & protocol revision peels. `Game` remains SoT until a
/// dedicated session storage owns wire gates (peels read through here only).
/// CIP tip wire gate (feature flags on the private hub).
bool cipFeature(const tibia::client::GameFeature feature);


void enableFeature(const tibia::client::GameFeature feature);


void disableFeature(const tibia::client::GameFeature feature);


void setProtocolVersion(const uint16_t version);


void setClientRevision(const uint16_t version);


/// Client bootstrap — `Client::init` / `terminate` (tip-bridge hub lifetime).
void initGameHub();
void terminateGameHub();

/// ProtocolGame socket error → hub disconnect peel.
void processConnectionError(const std::error_code& ec);

/// Sessiondump OTC packet replay (headless / devtools).
void startDecodedReplay();
void deliverReplayPacket(const uint8_t* data, size_t size);
void finishDecodedReplay();

std::string formatCreatureName(const std::string_view name);


void setCustomOs(const tibia::client::OperatingSystem_t os);


uint32_t attackingCreatureId();


uint32_t followingCreatureId();


void setAttackingCreatureId(const uint32_t creatureId);


void talk(const std::string_view message);


void turn(const tibia::client::Direction direction);


bool walk(const tibia::client::Direction direction);


void interruptAutoWalk();


void safeLogout();


/// Login / world wire side effects (ProtocolGame + TGameProtocolMessageHandler).
/// Prefer over raw `g_game` in protocol translators.
void processLogin();
void processGameStart();
void processPendingGame();
void processWorldEntered();
void processPing();
void processPingBack();
void processDeath(uint8_t deathType, uint8_t penality);
void processLoginError(std::string_view error);
void processLoginAdvice(std::string_view message);
void processLoginWait(std::string_view message, uint8_t time);
void processSessionEnd(uint8_t reason);

/// Wire handlers (ProtocolGame) — thin forwards until storages own the paths.
void processCharacterSwitch(std::string_view characterName);
void processPlayerHelpers(uint16_t helpers);
void processAttackCancel(uint32_t seq);
void processPlayerModes(tibia::client::FightModes fightMode, tibia::client::ChaseModes chaseMode, bool safeMode,
                        tibia::client::PVPModes pvpMode);
void processNumericalEffect(tibia::client::MessageMode mode, const Position& pos, uint64_t value, uint8_t color);
void processTextMessage(tibia::client::MessageMode mode, std::string_view text);
void processTalk(std::string_view name, uint16_t level, tibia::client::MessageMode mode, std::string_view text,
                 uint16_t channelId, const Position& pos, uint32_t statement = 0);
void processVipStateChange(uint32_t id, uint32_t status);
void processTeamFinderLeader(const TeamFinderLeaderData& data);
void processOpenContainer(uint8_t containerId, const ::tibia::container::TInventorySlot& containerItem,
                          std::string_view name, uint8_t capacity, bool hasParent,
                          const std::vector<::tibia::container::TInventorySlot>& items, bool isUnlocked, bool hasPages,
                          bool showSearch, uint16_t containerSize, uint16_t firstIndex);
void processCloseContainer(uint8_t containerId);
void processContainerAddItem(uint8_t containerId, const ::tibia::container::TInventorySlot& item, uint16_t slot);
void processContainerUpdateItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& item);
void processContainerRemoveItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& lastItem);
/// ProtocolGame wire handlers — thin `g_game` forwards (server parse peels).
void processUpdateNeeded(std::string_view signature);
void processSessionDumpStart(std::string_view sessionKey);
void processSessionDumpMarker(std::string_view marker);
void processSoundTrigger(uint8_t type, uint16_t soundId);
void processReadyForSecondaryConnection(std::string_view token);
void processOutfitMemorial(const OutfitMemorialData& data);
void processWalkCancel(tibia::client::Direction direction);
void processTeamFinderList(bool rateLimited, const std::vector<TeamFinderListEntry>& entries);
void processDepotSearchResults(const std::vector<DepotSearchResultItem>& items);
void processDepotSearchDetail(const DepotSearchDetail& detail);
void processCloseDepotSearch();
void processChannelList(const std::vector<std::tuple<uint16_t, std::string>>& channelList);
void processOpenChannel(uint16_t channelId, std::string_view name);
void processOpenPrivateChannel(std::string_view name);
void processOpenOwnPrivateChannel(uint16_t channelId, std::string_view name);
void processCloseChannel(uint16_t channelId);
void processVipAdd(uint32_t id, std::string_view name, uint32_t status, std::string_view description, uint32_t iconId,
                  bool notifyLogin, const std::vector<uint8_t>& groupID);
void processVipGroupChange(const std::vector<std::tuple<uint8_t, std::string, bool>>& vipGroups, uint8_t groupsAmountLeft);
void processTutorialHint(uint8_t id);
void processAddAutomapFlag(const Position& pos, uint8_t icon, std::string_view message);
void processRemoveAutomapFlag(const Position& pos, uint8_t icon, std::string_view message);
void processOpenOutfitWindow(const Outfit& currentOutfit,
                             const std::vector<std::tuple<uint16_t, std::string, uint8_t, uint8_t>>& outfitList,
                             const std::vector<std::tuple<uint16_t, std::string, uint8_t>>& mountList,
                             const std::vector<std::tuple<uint16_t, std::string>>& familiarList,
                             const std::vector<std::tuple<uint16_t, std::string>>& wingsList,
                             const std::vector<std::tuple<uint16_t, std::string>>& aurasList,
                             const std::vector<std::tuple<uint16_t, std::string>>& effectsList,
                             const std::vector<std::tuple<uint16_t, std::string>>& shaderList);
void processOpenNpcTrade(
    const std::vector<std::tuple<::tibia::container::TInventorySlot, std::string, uint32_t, uint32_t, uint32_t>>& items);
void processPlayerGoods(uint64_t money, const std::vector<std::pair<uint16_t, uint16_t>>& goods);
void processCloseNpcTrade();
void processOwnTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items);
void processCounterTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items);
void processCloseTrade();
void processEditText(uint32_t id, uint32_t itemId, uint16_t maxLength, std::string_view text, std::string_view writer,
                     std::string_view date, bool recentlyTraded = false);
void processEditList(uint32_t id, uint8_t doorId, std::string_view text);
void processQuestLog(const std::vector<std::tuple<uint16_t, std::string, bool>>& questList);
void processQuestLine(uint16_t questId, const std::vector<std::tuple<std::string, std::string, uint16_t>>& questMissions);
void processModalDialog(uint32_t id, std::string_view title, std::string_view message,
                        const std::vector<std::tuple<uint8_t, std::string>>& buttonList, uint8_t enterButton,
                        uint8_t escapeButton, const std::vector<std::tuple<uint8_t, std::string>>& choiceList, bool priority);
void processItemDetail(const ItemInspectionData& data);
void processInspectionState(uint32_t creatureId, uint8_t state);
void processCyclopediaCharacterGeneralStats(const CyclopediaCharacterGeneralStats& stats,
                                            const std::vector<std::vector<uint16_t>>& skills,
                                            const std::vector<std::tuple<uint8_t, uint16_t>>& combats);
void processCyclopediaCharacterCombatStats(const CyclopediaCharacterCombatStats& data, double mitigation,
                                           const std::vector<std::vector<uint16_t>>& additionalSkillsArray,
                                           const std::vector<std::vector<uint16_t>>& forgeSkillsArray,
                                           const std::vector<uint16_t>& perfectShotDamageRangesArray,
                                           const std::vector<std::tuple<uint8_t, uint16_t>>& combatsArray,
                                           const std::vector<std::tuple<uint16_t, uint16_t>>& concoctionsArray);
void processCyclopediaCharacterGeneralStatsBadge(uint8_t showAccountInformation, uint8_t playerOnline,
                                                 uint8_t playerPremium, std::string_view loyaltyTitle,
                                                 const std::vector<std::tuple<uint32_t, std::string>>& badgesVector);
void processCyclopediaCharacterInspection(const CyclopediaCharacterInspection& data);
void processCyclopediaCharacterItemSummary(const CyclopediaCharacterItemSummary& data);
void processCyclopediaCharacterAppearances(const OutfitColorStruct& currentOutfit,
                                           const std::vector<CharacterInfoOutfits>& outfits,
                                           const std::vector<CharacterInfoMounts>& mounts,
                                           const std::vector<CharacterInfoFamiliar>& familiars);
void processCyclopediaCharacterRecentDeaths(const CyclopediaCharacterRecentDeaths& data);
void processCyclopediaCharacterRecentPvpKills(const CyclopediaCharacterRecentPvPKills& data);
void processParseBestiaryRaces(const std::vector<CyclopediaBestiaryRace>& bestiaryData);
void processParseBestiaryOverview(std::string_view raceName, const std::vector<BestiaryOverviewMonsters>& data,
                                  uint16_t animusMasteryPoints);
void processUpdateBestiaryMonsterData(const BestiaryMonsterData& data);
void processUpdateBestiaryCharmsData(const BestiaryCharmsData& charmData);
void processBosstiaryInfo(const std::vector<BosstiaryData>& boss);
void processBosstiarySlots(const BosstiarySlotsData& data);
void processHighscore(std::string_view serverName, std::string_view world, uint8_t worldType, uint8_t battlEye,
                      const std::vector<std::tuple<uint32_t, std::string>>& vocations,
                      const std::vector<std::tuple<uint8_t, std::string>>& categories, uint16_t page, uint16_t totalPages,
                      const std::vector<std::tuple<uint32_t, std::string, std::string, uint8_t, std::string, uint16_t, uint8_t, uint64_t>>& highscores,
                      uint32_t entriesTs, const std::vector<std::string>& worlds = {}, std::string_view allVocationsName = {},
                      uint32_t selectedVocation = 0xFFFFFFFF, uint8_t selectedCategory = 0, uint8_t selectedIndex = 0xFF,
                      bool showLoyaltyTitle = false, uint8_t valueType = 1);
void processCyclopediaCharacterOffenceStats(const CyclopediaCharacterOffenceStats& data);
void processCyclopediaCharacterDefenceStats(const CyclopediaCharacterDefenceStats& data);
void processCyclopediaCharacterMiscStats(const CyclopediaCharacterMiscStats& data);


void localPlayerSetId(uint32_t id);
void localPlayerSetPendingGame(bool pending);
void localPlayerSetMarketAvailable(bool available);
void localPlayerSetSupplyStashAvailable(bool available);

/// Protocol vitals/resources: TPlayerData authority (gamesession setters). Combat: TPlayerSkillStats.
void localPlayerSetHealth(uint32_t health, uint32_t maxHealth);
void localPlayerSetMana(uint32_t mana, uint32_t maxMana);
void localPlayerSetManaShield(uint32_t manaShield, uint32_t maxManaShield);
void localPlayerSetLevel(uint16_t level, uint16_t levelPercent);
void localPlayerSetExperience(uint64_t experience);
void localPlayerSetStamina(uint16_t stamina);
void localPlayerSetSoul(uint8_t soul);
void localPlayerSetFreeCapacity(uint32_t freeCapacity);
void localPlayerSetPremium(bool premium);
void localPlayerSetVocation(uint8_t vocation);
void localPlayerSetSpells(const std::vector<uint16_t>& spells);
void localPlayerSetBlessings(uint16_t blessings, uint8_t blessVisualState = 0);
void localPlayerSetResourceBalance(tibia::client::ResourceTypes_t type, uint64_t value);
void localPlayerSetHasReachedMain(bool reached);
void localPlayerSetMagicShieldVocation(bool enabled);
void localPlayerSetHarmony(uint8_t harmony);
void localPlayerSetSerene(bool serene);
void localPlayerSetExperienceRate(tibia::client::ExperienceRate_t type, uint16_t value);
void localPlayerSetMagicLevel(uint16_t magicLevel, uint16_t magicLevelPercent);
void localPlayerSetBaseMagicLevel(uint16_t baseMagicLevel);
void localPlayerSetMagicLevelDetail(uint16_t loyaltyLevel, uint16_t percentHundredths);
void localPlayerSetSkill(tibia::client::Skill skill, uint16_t level, uint16_t levelPercent);
void localPlayerSetSkillDetail(tibia::client::Skill skill, uint16_t loyaltyLevel, uint16_t percentHundredths);
void localPlayerSetBaseSkill(tibia::client::Skill skill, uint16_t baseLevel);
void localPlayerSetBaseSpeed(uint16_t baseSpeed);
void localPlayerSetRegenerationTime(uint16_t regenerationTime);
void localPlayerSetOfflineTrainingTime(uint16_t offlineTrainingTime);
void localPlayerSetStoreExpBoostTime(uint16_t storeExpBoostTime);
void localPlayerSetStoreExpBoostPurchasable(bool purchasable);
void localPlayerSetStatesAndRottenBlood(uint64_t states, uint8_t rottenBloodState);
void localPlayerSetTotalCapacity(uint32_t totalCapacity);
void localPlayerSetBaseCapacity(uint32_t baseCapacity);
void localPlayerSetKnown(bool known);

/// Cyclopedia offence/defence stats (TPlayerSkillStats authority).
void localPlayerSetFlatDamageHealing(uint16_t flatBonus);
void localPlayerSetAttackInfo(uint16_t attackValue, uint8_t attackElement);
void localPlayerSetConvertedDamage(double convertedDamage, uint8_t convertedElement);
void localPlayerSetImbuements(double lifeLeech, double manaLeech, double critChance, double critDamage,
                              double onslaught);
void localPlayerSetDefenseInfo(uint16_t defense, uint16_t armor, uint16_t mantra, double mitigation, double dodge,
                               uint16_t damageReflection);
void localPlayerSetCombatAbsorbValues(const std::map<uint8_t, double>& absorbValues);
void localPlayerSetForgeBonuses(double momentum, double transcendence, double amplification);

/// Skills / movement stats (`TPlayerData`; combat via `TPlayerSkillStats`).
uint16_t localPlayerSkillLevel(tibia::client::Skill skill);
uint16_t localPlayerSkillBaseLevel(tibia::client::Skill skill);
uint16_t localPlayerSkillLoyaltyLevel(tibia::client::Skill skill);
uint16_t localPlayerSkillPercentHundredths(tibia::client::Skill skill);
uint16_t localPlayerMagicLevelLoyalty();
uint16_t localPlayerMagicLevelPercentHundredths();
uint16_t localPlayerLevelPercentHundredths();
uint16_t localPlayerExperienceRate(tibia::client::ExperienceRate_t type);
uint16_t localPlayerSpeed();
uint16_t localPlayerBaseSpeed();
uint16_t localPlayerRegenerationTime();
uint16_t localPlayerOfflineTrainingTime();
uint16_t localPlayerStoreExpBoostTime();
uint8_t localPlayerRottenBloodState();

void requestBless();


void partyJoin(const uint32_t creatureId);


int openAt(const Position& position, const uint16_t itemId, const int stackPos, const int previousContainerId = -1);


int openAt(const MapPos pos, const uint16_t itemId, const int stackPos, const int previousContainerId = -1);


void openContainerQuickLoot(const uint8_t action, const uint8_t category, const Position& pos, const uint16_t itemId, const uint8_t stackpos, const bool useMainAsFallback);


void useMapObject(const Position& position, const uint16_t itemId, const int stackPos);


void useMapObject(const MapPos pos, const uint16_t itemId, const int stackPos);


void useInventoryItemWithMapObject(const uint16_t itemId, const Position& to, const uint16_t toId, const int toStack);


void useInventoryItemWithMapObject(const uint16_t itemId, const MapPos to, const uint16_t toId, const int toStack);


void useInventoryItemOnCreature(const uint16_t itemId, const uint32_t creatureId);


ItemPtr findPlayerItem(const uint32_t itemId, const int subType);


void requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t size, const std::vector<uint16_t>& listedItems);


void sendStatusTrackerBestiary(const uint16_t raceId, const bool status);


void requestBestiarySearch(const uint16_t raceId);


void sendFriendSystemPayload(const std::string& json);


void openPrivateChannel(const std::string_view receiver);


void requestTeamFinderList();


void requestTeamFinderLeader();


void joinTeamFinder(const uint32_t leaderId);


void leaveTeamFinder(const uint32_t leaderId);


void createTeamFinder(const TeamFinderAssemble& assemble);


void resetTeamFinderLeader();


void updateTeamFinderMember(const uint32_t memberId, const uint8_t status);


/// CIP `game::IGameSession` moc surface (contract signals + tip-safe skipped
/// args + TGameSessionBase slot floor). Declared after free-function peels so
/// tip scanners do not attribute those decls to this class. Live login/world
/// still rides `TGameserverGameSession` / `g_game`; this qtclient twin re-emits the
/// same notify bus (no `loginWorld` / disconnect ownership). Frame prep stays
/// on `TGameClient::tick` / `TNextFrameUpdater` — `onPrepareNextFrameToRender`
/// is a CIP-named no-op so we never double-advance frames.
class IGameSession : public cip::tibia::game::IGameSession
{
    Q_OBJECT
public:
    explicit IGameSession(QObject* parent = nullptr);
    ~IGameSession() override;

public Q_SLOTS:
    /// CIP slot (skipped `::shared::TMilliseconds` → `qint64` ms).
    void onPrepareNextFrameToRender(qint64 nextFramePredictedTimestampMs);

    /// CIP `TGameSessionBase` slots (skipped/weak types → tip-safe). Fan into
    /// tip signals / `g_game` peels; do not own login.
    void onPlayerCreatureAddedToCreatureStorage();
    void onSettingHintsFromServerChanged(bool canChangePvPFrameSetting);
    void onMessageAtEndOfLoginRampUpReceived();
    void onWorldTypeReceived(int worldType);
    void onCharacterConfigurationLoaded();

Q_SIGNALS:
    /// CIP skipped `weak_ptr<TCreature>` — tip emits name-only floor.
    void playerCreatureAddedToCreatureStorage();
    /// CIP skipped `ESessionEndInformation` → `int` (same as TGameserverGameSession).
    void sessionEndInformation(int Reason);
    /// CIP skipped `ELoginErrorAction` → `int`.
    void gameLoginErrorMessage(const QString& ErrorMessage, int Action);
    /// CIP skipped `::shared::TSeconds` → `int` seconds.
    void gameLoginWaitMessage(const QString& WaitMessage, int WaitSeconds);

    /// CIP base signals re-owned on qtclient for moc + SoftHook fan-out.
    /// Same signatures as `cip::tibia::game::IGameSession` (not login owners).
    void loginRampUpFinished();
    void connected();
    void disconnected();
    void connectionError(const QString& ErrorDescription);
    void gameLoginSuccessful();
    void worldEntered();
    void deadMessage();
    void worldTypeSet();
    void gameLoginChallengeMessage();
    void gameLoginAdviceMessage(const QString& AdviceMessage);
    void pingTimeout();
    void pingTimeoutRecovered();
    void showCharacterSelectionDialog();
    void startTargetSelection();

private:
    void bindLiveNotifyFans();

    struct LiveFans;
    LiveFans* m_liveFans = nullptr;
};

/// Process-lifetime tip IGameSession (moc/RTTI keep). Not the live GameClient session.
IGameSession* tipIGameSession();

} // namespace game
} // namespace tibia
