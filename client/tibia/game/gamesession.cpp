#include "gamesession.h"

#include <client/gameconfig.h>
#include <framework/core/clock.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/world.h>

namespace tibia::game {

uint64_t millis()
{
    return static_cast<uint64_t>(g_clock.millis());
}

uint8_t mapSeaFloor()
{
    return g_gameConfig.getMapSeaFloor();
}

uint8_t worldLightIntensity()
{
    return tibia::world::fields().getLight().intensity;
}

uint32_t mapItemRevision()
{
    return tibia::world::fields().getItemRevision();
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

Position localPlayerPosition()
{
    if (const LocalPlayerPtr player = localPlayer())
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
    if (const LocalPlayerPtr player = localPlayer())
        return player->getName();
    return {};
}

uint32_t localPlayerId()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getId();
    return 0;
}

int localPlayerLevel()
{
    if (const LocalPlayerPtr player = localPlayer())
        return static_cast<int>(player->getLevel());
    return 0;
}

uint16_t localPlayerLevelPercent()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getLevelPercent();
    return 0;
}

uint32_t localPlayerHealth()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getHealth();
    return 0;
}

uint32_t localPlayerMaxHealth()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getMaxHealth();
    return 0;
}

uint32_t localPlayerMana()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getMana();
    return 0;
}

uint32_t localPlayerMaxMana()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getMaxMana();
    return 0;
}

uint16_t localPlayerStamina()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getStamina();
    return 0;
}

uint8_t localPlayerVocation()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getVocation();
    return 0;
}

uint8_t localPlayerShield()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getShield();
    return 0;
}

bool localPlayerHasReachedMain()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->hasReachedMain();
    return false;
}

uint64_t localPlayerTotalMoney()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getTotalMoney();
    return 0;
}

uint32_t localPlayerFreeCapacity()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getFreeCapacity();
    return 0;
}

uint16_t localPlayerInventoryTypeId(const tibia::client::InventorySlot slot)
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->inventorySlot(slot).typeId;
    return 0;
}

uint32_t localPlayerInventoryCount(const uint16_t itemId, const uint8_t tier)
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getInventoryCount(itemId, tier);
    return 0;
}

InventorySlotInfo localPlayerInventorySlot(const tibia::client::InventorySlot slot)
{
    InventorySlotInfo info;
    if (const LocalPlayerPtr player = localPlayer()) {
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
    const LocalPlayerPtr player = localPlayer();
    if (!player)
        return;
    const tibia::container::TInventorySlot& item = player->inventorySlot(slot);
    if (item.empty())
        return;
    openAt(inventoryWireMapPos(slot), item.typeId, 0);
}

void localPlayerStopAutoWalk()
{
    if (const LocalPlayerPtr player = localPlayer())
        player->stopAutoWalk();
}

bool localPlayerIsAutoWalking()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->isAutoWalking();
    return false;
}

bool localPlayerAutoWalk(const Position& goal, const bool retry,
                         const tibia::client::AutoWalkPathMode pathMode)
{
    if (const LocalPlayerPtr player = localPlayer())
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
    if (const LocalPlayerPtr player = localPlayer())
        return player->getBlessVisualState();
    return 0;
}

void localPlayerSetInformationChannel(const uint32_t durationMs)
{
    if (const LocalPlayerPtr player = localPlayer())
        player->setInformationChannel(durationMs);
}

void localPlayerClearInformationChannel()
{
    if (const LocalPlayerPtr player = localPlayer())
        player->clearInformationChannel();
}

void localPlayerSetSoftDead(const bool softDead)
{
    if (const LocalPlayerPtr player = localPlayer())
        player->setSoftDead(softDead);
}

Outfit localPlayerOutfit()
{
    if (const LocalPlayerPtr player = localPlayer())
        return player->getOutfit();
    return {};
}

} // namespace tibia::game
