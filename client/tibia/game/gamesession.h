#pragma once

#include <client/declarations.h>
#include <client/const.h>
#include <client/outfit.h>
#include <client/position.h>

#include <tibia/game/game.h>
#include <tibia/game/gamesession_forward.h>

#include "mapsession.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tibia::game {

/// CipSoft-shaped session / wire façade for tip + packs.
/// Prefer this header over `#include <tibia/game/game.h>` at call sites.
/// Implementation still peels the private OTC hub (`g_game`) until Phase 2.9
/// migrates each method onto TGameClient / IGameSession / storages.

/// Pack-facing monotonic ms (`g_clock.millis`). Header free of `framework/core/clock.h`.
uint64_t millis();

/// Map sea floor (`g_gameConfig.getMapSeaFloor`). Header free of `gameconfig.h`.
uint8_t mapSeaFloor();
/// World ambient light intensity (`g_map.getLight().intensity`). Header free of `world.h`/`map.h`.
uint8_t worldLightIntensity();
/// Map item revision for roof/geometry cache (`g_map.getItemRevision`).
uint32_t mapItemRevision();

/// World-time / session-end hooks (`tibia::world::subscribe*`). Header free of `world.h`.
int subscribeWorldTime(std::function<void(int hour, int minute)> listener);
void unsubscribeWorldTime(int id);
int subscribeSessionEnd(std::function<void()> listener);
void unsubscribeSessionEnd(int id);

inline LocalPlayerPtr localPlayer()
{
    return g_game.getLocalPlayer();
}

/// Pack-facing LocalPlayer peels — prefer these over `#include <tibia/world/localplayer.h>`.
/// Out-of-line so the allowlisted header does not pull the full LocalPlayer type.
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
uint16_t localPlayerStamina();
uint8_t localPlayerVocation();
uint8_t localPlayerShield();
bool localPlayerHasReachedMain();
uint64_t localPlayerTotalMoney();
uint32_t localPlayerFreeCapacity();
uint16_t localPlayerInventoryTypeId(tibia::client::InventorySlot slot);
uint32_t localPlayerInventoryCount(uint16_t itemId, uint8_t tier = 0);
/// Pack-facing inventory slot peel (exercise waiting-weapon / backpack scan).
/// No `TInventorySlot` / LocalPlayer in the header.
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

/// Auto-walk peels (TravelRouter). Prefer over LocalPlayer::autoWalk / stopAutoWalk.
void localPlayerStopAutoWalk();
bool localPlayerIsAutoWalking();
bool localPlayerAutoWalk(const Position& goal, bool retry = false,
                         tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);
/// Pack-facing autoWalk (`MapPos` — prefer for travel / packs).
bool localPlayerAutoWalk(MapPos goal, bool retry = false,
                         tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);

/// Bless chrome (attention). Matches LocalPlayer::getBlessVisualState.
uint8_t localPlayerBlessVisualState();

/// Instance channel / soft-death peels.
void localPlayerSetInformationChannel(uint32_t durationMs);
void localPlayerClearInformationChannel();
void localPlayerSetSoftDead(bool softDead);

/// Outfit for HUD chrome (teamhud). Prefer over LocalPlayer::getOutfit.
Outfit localPlayerOutfit();

inline bool isOnline()
{
    return g_game.isOnline();
}

inline bool isDead()
{
    return g_game.isDead();
}

inline bool isSceneFrozen()
{
    return g_game.isSceneFrozen();
}

inline void setSceneFrozen(const bool frozen)
{
    g_game.setSceneFrozen(frozen);
}

inline bool isSwitchingCharacter()
{
    return g_game.isSwitchingCharacter();
}

inline std::string characterName()
{
    return g_game.getCharacterName();
}

inline int clientRevision()
{
    return g_game.getClientVersion();
}

/// CIP tip wire gate. Prefer over raw `g_game.getFeature`.
inline bool cipFeature(const tibia::client::GameFeature feature)
{
    return g_game.getFeature(feature);
}

inline void enableFeature(const tibia::client::GameFeature feature)
{
    g_game.enableFeature(feature);
}

inline void setCustomOs(const tibia::client::OperatingSystem_t os)
{
    g_game.setCustomOs(os);
}

inline uint32_t attackingCreatureId()
{
    return g_game.getAttackingCreatureId();
}

inline void setAttackingCreatureId(const uint32_t creatureId)
{
    g_game.setAttackingCreatureId(creatureId);
}

inline void talk(const std::string_view message)
{
    g_game.talk(message);
}

inline void safeLogout()
{
    g_game.safeLogout();
}

inline void requestBless()
{
    g_game.requestBless();
}

inline void partyJoin(const uint32_t creatureId)
{
    g_game.partyJoin(creatureId);
}

inline int openAt(const Position& position, const uint16_t itemId, const int stackPos,
                  const int previousContainerId = -1)
{
    return g_game.openAt(position, itemId, stackPos, previousContainerId);
}

inline int openAt(const MapPos pos, const uint16_t itemId, const int stackPos,
                  const int previousContainerId = -1)
{
    return openAt(Position(pos.x, pos.y, static_cast<uint8_t>(pos.z)), itemId, stackPos,
                  previousContainerId);
}

inline void openContainerQuickLoot(const uint8_t action, const uint8_t category, const Position& pos,
                                   const uint16_t itemId, const uint8_t stackpos, const bool useMainAsFallback)
{
    g_game.openContainerQuickLoot(action, category, pos, itemId, stackpos, useMainAsFallback);
}

inline void useMapObject(const Position& position, const uint16_t itemId, const int stackPos)
{
    g_game.useMapObject(position, itemId, stackPos);
}

inline void useMapObject(const MapPos pos, const uint16_t itemId, const int stackPos)
{
    g_game.useMapObject(Position(pos.x, pos.y, static_cast<uint8_t>(pos.z)), itemId, stackPos);
}

inline void useInventoryItemWithMapObject(const uint16_t itemId, const Position& to, const uint16_t toId,
                                          const int toStack)
{
    g_game.useInventoryItemWithMapObject(itemId, to, toId, toStack);
}

inline void useInventoryItemWithMapObject(const uint16_t itemId, const MapPos to, const uint16_t toId,
                                          const int toStack)
{
    g_game.useInventoryItemWithMapObject(itemId, Position(to.x, to.y, static_cast<uint8_t>(to.z)), toId, toStack);
}

inline void useInventoryItemOnCreature(const uint16_t itemId, const uint32_t creatureId)
{
    g_game.useInventoryItemOnCreature(itemId, creatureId);
}

inline ItemPtr findPlayerItem(const uint32_t itemId, const int subType)
{
    return g_game.findPlayerItem(itemId, subType);
}

inline void requestQuickLootBlackWhiteList(const uint8_t filter, const uint16_t size,
                                           const std::vector<uint16_t>& listedItems)
{
    g_game.requestQuickLootBlackWhiteList(filter, size, listedItems);
}

inline void sendStatusTrackerBestiary(const uint16_t raceId, const bool status)
{
    g_game.sendStatusTrackerBestiary(raceId, status);
}

inline void requestBestiarySearch(const uint16_t raceId)
{
    g_game.requestBestiarySearch(raceId);
}

inline void sendFriendSystemPayload(const std::string& json)
{
    g_game.sendFriendSystemPayload(json);
}

inline void openPrivateChannel(const std::string_view receiver)
{
    g_game.openPrivateChannel(receiver);
}

inline void requestTeamFinderList()
{
    g_game.requestTeamFinderList();
}

inline void requestTeamFinderLeader()
{
    g_game.requestTeamFinderLeader();
}

inline void joinTeamFinder(const uint32_t leaderId)
{
    g_game.joinTeamFinder(leaderId);
}

inline void leaveTeamFinder(const uint32_t leaderId)
{
    g_game.leaveTeamFinder(leaderId);
}

inline void createTeamFinder(const TeamFinderAssemble& assemble)
{
    g_game.createTeamFinder(assemble);
}

inline void resetTeamFinderLeader()
{
    g_game.resetTeamFinderLeader();
}

inline void updateTeamFinderMember(const uint32_t memberId, const uint8_t status)
{
    g_game.updateTeamFinderMember(memberId, status);
}

} // namespace tibia::game
