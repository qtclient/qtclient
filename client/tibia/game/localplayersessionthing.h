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

#pragma once

#include "appearances/thing.h"
#include "client/outfit.h"
#include "container/containerciptypes.h"

#include <framework/core/declarations.h>
#include <framework/graphics/cachedtext.h>

#include <memory>
#include <string_view>

#include <tibia/game/gamenotify.h>

namespace tibia::creatures {
class TCreature;
}

class LocalPlayerGoPath;

namespace tibia::game {
void wireLocalPlayerOnAppear(LocalPlayerSessionThing& player);
void wireLocalPlayerOnDisappear(LocalPlayerSessionThing& player);
}

 // @bindclass
class LocalPlayerSessionThing final : public Thing
{
public:
    LocalPlayerSessionThing();
    ~LocalPlayerSessionThing() override;

    void onCreate();
    void onAppear() override;
    void onDisappear() override;

    bool isCreature() const override { return true; }
    bool isPlayer() const override { return true; }
    bool isLocalPlayer() const override { return true; }
    ThingType* getThingType() const override;

    void setId(uint32_t id) override;
    uint32_t getId() override;
    void setName(std::string_view name);
    std::string getName();
    void setMasterId(uint32_t id) { m_masterId = id; }
    uint32_t getMasterId() const { return m_masterId; }
    void setType(uint8_t type) { m_creatureType = type; }
    void setDirection(tibia::client::Direction direction, bool syncFieldStack = true);
    tibia::client::Direction getDirection() const { return m_direction; }
    void turn(tibia::client::Direction direction);
    void setOutfit(const Outfit& outfit, bool fireEvent = true, bool syncFieldStack = true);
    const Outfit& getOutfit() const { return m_outfit; }
    void setBaseSpeed(uint16_t baseSpeed);
    void setSpeed(uint16_t speed);
    uint16_t getSpeed() const { return m_speed; }
    uint16_t getBaseSpeed() const { return m_baseSpeed; }
    uint8_t getShield() const { return m_shield; }
    uint8_t getSkull() const { return m_skull; }
    void setHealthPercent(uint8_t healthPercent);
    bool isDead() const;
    bool isRemoved() const { return m_removed; }
    bool isWalking();
    void stopWalk();
    Point getWalkOffset();
    uint16_t getStepDuration(bool ignoreDiagonal = false,
                             tibia::client::Direction dir = tibia::client::InvalidDirection);
    Position getServerPosition() const { return m_position; }

    void unlockWalk();
    /// Server Stop opcode (not SnapBack): clear auto-walk / Target; do not retry.
    void stopPendingWalk();
    void lockWalk(uint16_t millis = 250);
    bool isWalkLocked();
    /// Clear client GoPath / Target. `notifyServer` sends Stop (CipSoft takeover
    /// action 0x94 + sendStop) so the server aborts an in-flight GoPath.
    void stopAutoWalk(bool notifyServer = false);
    /// Armed GoPath dirs still to predict (replan “live” check).
    bool hasAutoWalkDirs();

    /// `pathMode`: Minimap (default click), Cyclopedia (Shift+map), Travel (TravelRouter).
    bool autoWalk(const Position& destination, bool retry = false,
                  tibia::client::AutoWalkPathMode pathMode = tibia::client::AutoWalkPathMinimap);
    bool canWalk(bool ignoreLock = false);

    void setStates(uint64_t states);
    auto getStates() { return m_states; }
    void setSoftDead(bool softDead);
    bool isSoftDead() const { return m_softDead; }

    // On-map nameplate channel bar (under HP/mana). Fills over durationMs, then holds at
    // 100% until cleared.
    void setInformationChannel(uint32_t durationMs);
    void clearInformationChannel() { m_informationChannelDuration = 0; }
    // 0-100 while a channel is set; <0 when none.
    float getInformationChannelProgress() const;

    void setKnown(const bool known) { m_known = known; }
    void setPendingGame(const bool pending) { m_pending = pending; }
    /// Phase 5.0-D: notify only — CIP `TInventoryContainer` owns slot data.
    void setInventoryItem(tibia::client::InventorySlot inventory);
    void setInventoryCountCache(std::map<std::pair<uint16_t, uint8_t>, uint32_t> counts);
    void takeScreenshot(uint8_t type);
    const tibia::container::TInventorySlot& inventorySlot(tibia::client::InventorySlot inventory) const;
    ItemPtr getInventoryItem(tibia::client::InventorySlot inventory);
    bool hasEquippedItemId(uint16_t itemId, uint8_t tier);
    uint32_t getInventoryCount(uint16_t itemId, uint8_t tier);

    bool hasSight(const Position& pos);
    bool isKnown() { return m_known; }
    bool isServerWalking() { return m_serverWalk; }
    bool isPreWalking();

    bool isSupplyStashAvailable() const { return m_isSupplyStashAvailable; }
    void setSupplyStashAvailable(bool available) {
        m_isSupplyStashAvailable = available;
    }
    bool isMarketAvailable() const { return m_isMarketAvailable; }
    void setMarketAvailable(bool available) { m_isMarketAvailable = available; }

    bool isAutoWalking();
    bool isPendingGame() const { return m_pending; }
    bool isParalyzed() const { return (m_states & tibia::client::IconParalyze) == tibia::client::IconParalyze; }

    LocalPlayerSessionThingPtr sessionThingPtr() { return static_self_cast<LocalPlayerSessionThing>(); }

    void onPositionChange(const Position& newPos, const Position& oldPos) override;

    /// CipSoft client prediction: begin TCreatureMovementTween before wire confirm.
    void predictStep(tibia::client::Direction direction);
    /// Cancel predicted step after NFU finds an impassable storage row on the tile.
    void cancelPredictedStepCollision();
    /// CIP GoPath: arm remaining dirs for client-side step prediction (≤ walkMaxSteps ahead).
    void armGoPathDirs(const std::vector<tibia::client::Direction>& dirs);
    /// Predict the next armed GoPath step when canWalk allows (tween end / NFU).
    bool tryBeginNextGoPathStep();
    /// CipSoft: TCreatureMovementTween terminate → GoPath chain / deferred chunk.
    void onMovementTweenTerminated();

    Position getPosition() override;
    void resetPreWalk();
    /// CipSoft floor change / map description teleport — abort tween + prewalks.
    void handleTeleport();
    size_t getPreWalkingSize();
    /// Mirror HUD/session stats onto the storage row (keep-alive); walk state stays on TCreature.
    void syncStorageRow(tibia::creatures::TCreature& row);
    void registerAdjustInvalidPosEvent();

private:
    void goPathResetLastMapDuration() { m_lastMapDuration = -1; }
    void goPathClearWalkSessionMirror()
    {
        m_serverWalk = false;
        m_walking = false;
        m_walkOffset = {};
    }
    void goPathSetServerWalk(const bool walking) { m_serverWalk = walking; }

    friend class LocalPlayerGoPath;
    friend class Game;
    LocalPlayerGoPath& goPath();
    template<typename... T>
    void publish(const std::string_view field, const T&... args)
    {
        tibia::game::notify(field, args...);
    }

    void onWalking();

    void walk(const Position& oldPos, const Position& newPos);
    void terminateWalk();
    void cancelWalk(tibia::client::Direction direction = tibia::client::InvalidDirection);

    uint32_t m_id{ 0 };
    uint32_t m_masterId{ 0 };
    uint8_t m_creatureType{ 0 };
    uint8_t m_shield{ tibia::client::ShieldNone };
    uint8_t m_skull{ tibia::client::SkullNone };
    uint8_t m_healthPercent{ 101 };
    uint16_t m_speed{ 0 };
    uint16_t m_baseSpeed{ 0 };
    uint16_t m_calculatedStepSpeed{ 0 };
    tibia::client::Direction m_direction{ tibia::client::South };
    tibia::client::Direction m_walkTurnDirection{ tibia::client::InvalidDirection };
    Outfit m_outfit;
    CachedText m_name;
    Point m_walkOffset;
    bool m_removed{ true };
    Position m_oldPosition;
    EventPtr m_disappearEvent;
    struct StepDurationCache
    {
        uint16_t speed{ 0 };
        uint16_t groundSpeed{ 0 };
        uint16_t duration{ 0 };
        uint16_t walkDuration{ 0 };
        uint16_t diagonalDuration{ 0 };
        uint16_t getDuration(tibia::client::Direction dir) const
        {
            return Position::isDiagonal(dir) ? diagonalDuration : duration;
        }
    };
    StepDurationCache m_stepCache;

    bool m_walking{ false };
    int16_t m_lastMapDuration{ -1 };

    bool m_known{ false };
    bool m_pending{ false };
    bool m_serverWalk{ false };
    bool m_softDead{ false };
    ticks_t m_informationChannelStart{ 0 };
    uint32_t m_informationChannelDuration{ 0 };

    bool m_isSupplyStashAvailable{ false };
    bool m_isMarketAvailable{ false };
    std::map<std::pair<uint16_t, uint8_t>, uint32_t> m_inventoryCountCache;

    uint64_t m_states{ 0 };

    friend class Game;
    friend class tibia::creatures::TCreature;
    friend void tibia::game::wireLocalPlayerOnAppear(LocalPlayerSessionThing& player);
    friend void tibia::game::wireLocalPlayerOnDisappear(LocalPlayerSessionThing& player);
};
