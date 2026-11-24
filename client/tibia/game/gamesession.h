#pragma once

#include "game/objectreference.h"
#include <optional>
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

// Classic nested braces (readable; matches qtclient style after compound-ns cleanup).
namespace tibia {
namespace creatures {
class TCreature;
class TCreatureStorage;
}
namespace game {

/// CipSoft-shaped session / wire façade for qtclient + packs.
/// Prefer this header over `#include "game/gamesessionstate.h"` at call sites.
/// Implementation: the shared session state, game::TGameSessionState (gamesessionstate.*).
/// migrates each method onto TGameClient / IGameSession / storages.

/// Pack-facing monotonic ms (the frame clock, shared::TClockService).
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

/// The session has a character: from the login until the logout.
bool hasLocalPlayer();
/// The local player's creature (null before the server described it).
std::shared_ptr<tibia::creatures::TCreature> localPlayerCreature();

/// The local player, read from its creature and the player data.
Position localPlayerPosition();
/// Pack-facing position peel (`MapPos` — prefer over `localPlayerPosition` / `Position`).
MapPos localPlayerMapPos();
std::string localPlayerName();
uint32_t localPlayerId();
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
/// The free capacity in hundredths of an oz, as the server sent it.
uint32_t localPlayerFreeCapacityHundredths();
uint16_t localPlayerInventoryTypeId(tibia::client::InventorySlot slot);
uint32_t localPlayerInventoryCount(uint16_t itemId, uint8_t tier = 0);
/// Pack-facing inventory slot peel (exercise waiting-weapon / backpack scan).
/// No `TInventorySlot` in the header.
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

/// Walking goes through the movement intent handler.
void localPlayerStopAutoWalk();
/// A walk goal is live.
bool localPlayerIsAutoWalking();
bool localPlayerHasAutoWalkDirs();
bool localPlayerIsWalking();
/// A predicted step awaits the server.
bool localPlayerIsPreWalking();
/// Walk to `goal` (map click, travel). `retry` and `pathMode` are kept for the pack ABI;
/// `enterUnwalkableGoal` lets the path end on a goal that cannot be stood on.
bool localPlayerAutoWalk(const Position& goal, bool retry = false,
                         tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap,
                         bool enterUnwalkableGoal = true);
bool localPlayerAutoWalk(MapPos goal, bool retry = false,
                         tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);
/// Held direction keys (keyboard walking).
void localPlayerHoldDirection(tibia::client::Direction direction);
void localPlayerReleaseDirection();
void localPlayerRotate(tibia::client::Direction direction);
/// Per-frame movement step.
void localPlayerMovementTick();
/// The server rejected a step; the player looks in `direction`.
void localPlayerCancelWalk(tibia::client::Direction direction);
/// The server asks to wait `ms` before the next step.

/// Bless chrome (attention).
uint8_t localPlayerBlessVisualState();

/// Player state icon bits (EPlayerStateFlag), from the player data.
uint64_t localPlayerStates();
uint8_t localPlayerSoul();
uint16_t localPlayerMagicLevel();
uint16_t localPlayerBaseMagicLevel();
uint16_t localPlayerMagicLevelPercent();
std::vector<uint16_t> localPlayerSpells();
tibia::client::Direction localPlayerDirection();
bool localPlayerIsSoftDead();
/// Same floor, at most one field away.
bool isOneStep(const Position& from, const Position& to);
/// A floor change or a jump: the motion and the walk goal end.
void localPlayerHandleTeleport();
/// The local player's step in flight stops where it is.
void localPlayerStopWalk();

uint8_t localPlayerSkull();
uint16_t localPlayerBlessings();
uint32_t localPlayerBaseCapacity();
uint32_t localPlayerTotalCapacity();
bool localPlayerHasEquippedItemId(uint16_t itemId, uint8_t tier = 0);
bool localPlayerIsStoreExpBoostPurchasable();

/// Instance channel / soft-death peels.
void localPlayerSetInformationChannel(uint32_t durationMs);
void localPlayerClearInformationChannel();
void localPlayerSetSoftDead(bool softDead);

/// Outfit for HUD chrome (teamhud).
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








void setProtocolVersion(const uint16_t version);


void setClientRevision(const uint16_t version);


/// Client bootstrap — `Client::init` / `terminate` (qtclient-bridge hub lifetime).
void initGameHub();
void terminateGameHub();

/// The game connection was lost (`clean` = peer close, no error reported).
void processConnectionError(std::string_view message, bool clean);

/// Sessiondump OTC packet replay (headless / devtools).
void startDecodedReplay();
void deliverReplayPacket(const uint8_t* data, size_t size);
void finishDecodedReplay();

std::string formatCreatureName(const std::string_view name);




uint32_t attackingCreatureId();


uint32_t followingCreatureId();


void setAttackingCreatureId(const uint32_t creatureId);


void talk(const std::string_view message);


void turn(const tibia::client::Direction direction);


bool walk(const tibia::client::Direction direction);


void interruptAutoWalk();


void safeLogout();


/// Login / world side effects (game connection + TGameProtocolMessageHandler).
/// Prefer over raw `g_game` in protocol translators.
void processGameStart();
void processWorldEntered();
void processLoginError(std::string_view error);
void processLoginAdvice(std::string_view message);
void processLoginWait(std::string_view message, uint8_t time);

/// Server message side effects — thin forwards until storages own the paths.
void processCharacterSwitch(std::string_view characterName);
void processPlayerHelpers(uint16_t helpers);
void processUpdateTarget(uint32_t seq, uint32_t targetId);
void processTeamFinderLeader(const TeamFinderLeaderData& data);
void processOpenContainer(uint8_t containerId, const ::tibia::container::TInventorySlot& containerItem,
                          std::string_view name, uint8_t capacity, bool hasParent,
                          const std::vector<::tibia::container::TInventorySlot>& items, bool isUnlocked, bool hasPages,
                          bool showSearch, uint16_t containerSize, uint16_t firstIndex);
void processCloseContainer(uint8_t containerId);
void processContainerAddItem(uint8_t containerId, const ::tibia::container::TInventorySlot& item, uint16_t slot);
void processContainerUpdateItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& item);
void processContainerRemoveItem(uint8_t containerId, uint16_t slot, const ::tibia::container::TInventorySlot& lastItem);
/// Server message side effects — thin hub forwards.
void processUpdateNeeded(std::string_view signature);
void processSessionDumpMarker(std::string_view marker);
void processWalkCancel(tibia::client::Direction direction);
void processTeamFinderList(bool rateLimited, const std::vector<TeamFinderListEntry>& entries);
void processTutorialHint(uint8_t id);
void processAddAutomapFlag(const Position& pos, uint8_t icon, std::string_view message);
void processRemoveAutomapFlag(const Position& pos, uint8_t icon, std::string_view message);
void processOwnTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items);
void processCounterTrade(std::string_view name, const std::vector<::tibia::container::TInventorySlot>& items);
void processModalDialog(uint32_t id, std::string_view title, std::string_view message,
                        const std::vector<std::tuple<uint8_t, std::string>>& buttonList, uint8_t enterButton,
                        uint8_t escapeButton, const std::vector<std::tuple<uint8_t, std::string>>& choiceList, bool priority);
void processItemDetail(const ItemInspectionData& data);
void processInspectionState(uint32_t creatureId, uint8_t state);
void processParseBestiaryRaces(const std::vector<CyclopediaBestiaryRace>& bestiaryData);
void processParseBestiaryOverview(std::string_view raceName, const std::vector<BestiaryOverviewMonsters>& data,
                                  uint16_t animusMasteryPoints);
void processUpdateBestiaryMonsterData(const BestiaryMonsterData& data);
void processUpdateBestiaryCharmsData(const BestiaryCharmsData& charmData);
void processBosstiaryInfo(const std::vector<BosstiaryData>& boss);
void processBosstiarySlots(const BosstiarySlotsData& data);



/// Protocol vitals/resources: TPlayerData authority (gamesession setters). Combat: TPlayerSkillStats.
void localPlayerSetHealth(uint32_t health, uint32_t maxHealth);
void localPlayerSetMana(uint32_t mana, uint32_t maxMana);
void localPlayerSetManaShield(uint32_t manaShield, uint32_t maxManaShield);
void localPlayerSetLevel(uint16_t level, uint16_t levelPercent);
void localPlayerSetExperience(uint64_t experience);
void localPlayerSetStamina(uint16_t stamina);
void localPlayerSetSoul(uint8_t soul);
/// `hundredths`: the server's free capacity in hundredths of an oz.
void localPlayerSetFreeCapacity(uint32_t hundredths);
void localPlayerSetBlessings(uint16_t blessings, uint8_t blessVisualState = 0);
void localPlayerSetResourceBalance(tibia::client::ResourceTypes_t type, uint64_t value);
void localPlayerSetHarmony(uint8_t harmony);
void localPlayerSetSerene(bool serene);
void localPlayerSetExperienceRate(tibia::client::ExperienceRate_t type, uint16_t value);
/// A skill of the skills message by skill type (4 magic level, 10..16 the
/// fighting skills and fishing).
void localPlayerSetSkill(int skillType, double level, double baseLevel, double loyalty, double percent);
void localPlayerSetBaseSpeed(uint16_t baseSpeed);
void localPlayerSetRegenerationTime(uint16_t regenerationTime);
void localPlayerSetOfflineTrainingTime(uint16_t offlineTrainingTime);
void localPlayerSetStoreExpBoostTime(uint16_t storeExpBoostTime);
void localPlayerSetDailyRewardExpBoostTime(uint16_t dailyRewardExpBoostTime);
void localPlayerSetStoreExpBoostPurchasable(bool purchasable);
void localPlayerSetStatesAndRottenBlood(uint64_t states, uint8_t rottenBloodState);
void localPlayerSetTotalCapacity(uint32_t totalCapacity);
void localPlayerSetBaseCapacity(uint32_t baseCapacity);

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
uint16_t localPlayerDailyRewardExpBoostTime();
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


/// The first inventory or open-container object of `itemId` (any subtype when -1).
std::optional<TObjectReference> findPlayerItem(uint32_t itemId, int subType);


void requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t size, const std::vector<uint16_t>& listedItems);


void sendStatusTrackerBestiary(const uint16_t raceId, const bool status);


void requestBestiarySearch(const uint16_t raceId);




void openPrivateChannel(const std::string_view receiver);


void requestTeamFinderList();


void requestTeamFinderLeader();


void joinTeamFinder(const uint32_t leaderId);


void leaveTeamFinder(const uint32_t leaderId);


void createTeamFinder(const TeamFinderAssemble& assemble);


void resetTeamFinderLeader();


void updateTeamFinderMember(const uint32_t memberId, const uint8_t status);



} // namespace game
} // namespace tibia
