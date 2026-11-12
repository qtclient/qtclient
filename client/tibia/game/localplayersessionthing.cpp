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

#include "game/localplayersessionthing.h"

#include "game/localplayergopath.h"
#include <framework/core/gamehooks.h>

#include "container/containerciptypes.h"
#include "creatures/creaturespeedformula.h"
#include "creatures/creaturestorage.h"
#include "framework/core/scheduledevent.h"
#include <tibia/game/playerdata.h>
#include <tibia/game/thingsession_forward.h>
#include <tibia/schedule/schedule.h>
#include "tibia/assets/thingtype.h"
#include <tibia/config/runtimegameconfig.h>
#include "worldmap/worldmapfield.h"
#include "client/item.h"
#include <tibia/worldmap/worldmapstorage.h>
#include "framework/core/clock.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/worldmap/worldmapstorage.h>
#include "gamehub.h"
#include "gamehub_impl.h"

#include <cmath>
#include <limits>

namespace {

std::shared_ptr<tibia::creatures::TCreature> localWalkRow(LocalPlayerSessionThing& player)
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage || player.getId() == 0)
        return nullptr;
    return storage->getOrCreate(player.getId());
}

} // namespace

LocalPlayerSessionThing::LocalPlayerSessionThing()
{
    m_name.setAlign(Fw::AlignTopCenter);
}
LocalPlayerSessionThing::~LocalPlayerSessionThing() = default;

LocalPlayerGoPath& LocalPlayerSessionThing::goPath()
{
    LocalPlayerGoPath* path = tipGameHub().localPlayerGoPath();
    if (!path)
        throw Exception("LocalPlayerGoPath not bound to Game session");
    return *path;
}

void LocalPlayerSessionThing::lockWalk(const uint16_t millis) { goPath().lockWalk(millis); }
void LocalPlayerSessionThing::unlockWalk() { goPath().unlockWalk(); }
bool LocalPlayerSessionThing::canWalk(const bool ignoreLock) { return goPath().canWalk(ignoreLock); }

void LocalPlayerSessionThing::walk(const Position& oldPos, const Position& newPos)
{
    (void)oldPos;
    (void)newPos;
}

void LocalPlayerSessionThing::predictStep(const tibia::client::Direction direction) { goPath().predictStep(direction); }
void LocalPlayerSessionThing::armGoPathDirs(const std::vector<tibia::client::Direction>& dirs) { goPath().armGoPathDirs(dirs); }
bool LocalPlayerSessionThing::tryBeginNextGoPathStep() { return goPath().tryBeginNextGoPathStep(); }
void LocalPlayerSessionThing::onWalking() { goPath().onWalking(); }
void LocalPlayerSessionThing::registerAdjustInvalidPosEvent() { goPath().registerAdjustInvalidPosEvent(); }

void LocalPlayerSessionThing::cancelPredictedStepCollision()
{
    cancelWalk();
    tibia::worldmap::notifyTileUpdate(getPosition(), nullptr, tibia::client::OPERATION_CLEAN);
}

void LocalPlayerSessionThing::cancelWalk(const tibia::client::Direction direction) { goPath().cancelWalk(direction); }
bool LocalPlayerSessionThing::autoWalk(const Position& destination, const bool retry, const tibia::client::AutoWalkPathMode pathMode)
{
    return goPath().autoWalk(destination, retry, pathMode);
}

bool LocalPlayerSessionThing::isWalkLocked() { return goPath().isWalkLocked(); }
bool LocalPlayerSessionThing::isPreWalking() { return goPath().isPreWalking(); }
bool LocalPlayerSessionThing::isAutoWalking() { return goPath().isAutoWalking(); }

Position LocalPlayerSessionThing::getPosition()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this)) {
        if (row->isPreWalking())
            return row->predictedPosition();
        if (row->position().isValid())
            return row->position();
    }
    return m_position;
}

void LocalPlayerSessionThing::resetPreWalk() { goPath().resetPreWalk(); }

void LocalPlayerSessionThing::syncStorageRow(tibia::creatures::TCreature& row)
{
    row.m_isLocalPlayer = true;
    if (tibia::game::TPlayerData* data = tibia::game::TPlayerData::liveInstance()) {
        row.m_mana = data->mana();
        row.m_maxMana = data->maxMana();
        row.m_manaShield = data->manaShield();
        row.m_maxManaShield = data->maxManaShield();
        row.m_harmony = data->harmony();
        row.m_serene = data->serene();
        row.m_vocation = data->vocation();
    }
    if (const std::string& name = getName(); !name.empty())
        row.m_name = QString::fromStdString(name);
  // Walk mutation (preWalks / lock expiry / auto-walk) is storage-owned — do not
    // overwrite from LocalPlayerSessionThing. Keep-alive still mirrors confirmed server tile.
    row.m_serverPosition = getServerPosition();
    if (!row.m_walking)
        row.m_stepDuration = getStepDuration();
}

void LocalPlayerSessionThing::handleTeleport() { goPath().handleTeleport(); }
size_t LocalPlayerSessionThing::getPreWalkingSize() { return goPath().getPreWalkingSize(); }
void LocalPlayerSessionThing::stopAutoWalk(const bool notifyServer) { goPath().stopAutoWalk(notifyServer); }
bool LocalPlayerSessionThing::hasAutoWalkDirs() { return goPath().hasAutoWalkDirs(); }
void LocalPlayerSessionThing::stopPendingWalk() { goPath().stopPendingWalk(); }

void LocalPlayerSessionThing::terminateWalk()
{
    onMovementTweenTerminated();

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(getId()); row && row->isWalking())
            row->terminateWalk();
    }
    goPathSetServerWalk(false);
}

void LocalPlayerSessionThing::onMovementTweenTerminated() { goPath().onMovementTweenTerminated(); }

void LocalPlayerSessionThing::onPositionChange(const Position& newPos, const Position& oldPos)
{
    if (!isPreWalking())
        tibia::game::notify("onPositionChange", newPos, oldPos);

    goPath().onConfirmedPosition(newPos);

    if (!isPreWalking())
        goPathSetServerWalk(false);
}

void LocalPlayerSessionThing::setStates(const uint64_t states)
{
    if (m_states == states)
        return;

    const uint64_t oldStates = m_states;
    m_states = states;
    publish("onStatesChange", states, oldStates);
}

void LocalPlayerSessionThing::setInformationChannel(const uint32_t durationMs)
{
    m_informationChannelStart = g_clock.millis();
    m_informationChannelDuration = std::max<uint32_t>(durationMs, 1);
}

float LocalPlayerSessionThing::getInformationChannelProgress() const
{
    if (m_informationChannelDuration == 0)
        return -1.f;

    const auto elapsed = static_cast<float>(g_clock.millis() - m_informationChannelStart);
    return std::min(100.f, elapsed * 100.f / static_cast<float>(m_informationChannelDuration));
}

void LocalPlayerSessionThing::setSoftDead(const bool softDead)
{
    if (m_softDead == softDead)
        return;
    m_softDead = softDead;
    if (softDead) {
        tibia::game::localPlayerSetHealth(0, tibia::game::localPlayerMaxHealth());
        setHealthPercent(0);
    }
}

void LocalPlayerSessionThing::setInventoryItem(const tibia::client::InventorySlot inventory)
{
    (void)inventory;
    publish("onInventoryChange", inventory);
}

const tibia::container::TInventorySlot& LocalPlayerSessionThing::inventorySlot(
    const tibia::client::InventorySlot inventory) const
{
    static const tibia::container::TInventorySlot kEmpty;
    if (tibia::container::TInventoryContainer* inventoryContainer = tibia::container::TInventoryContainer::instance())
        return inventoryContainer->slotAt(static_cast<int>(inventory));
    return kEmpty;
}

ItemPtr LocalPlayerSessionThing::getInventoryItem(const tibia::client::InventorySlot inventory)
{
    const tibia::container::TInventorySlot& slot = inventorySlot(inventory);
    if (slot.empty())
        return nullptr;
    return Item::create(slot.typeId);
}

void LocalPlayerSessionThing::setInventoryCountCache(std::map<std::pair<uint16_t, uint8_t>, uint32_t> counts)
{
    m_inventoryCountCache = std::move(counts);
}

bool LocalPlayerSessionThing::hasEquippedItemId(const uint16_t itemId, const uint8_t tier)
{
    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance()) {
        for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot) {
            const tibia::container::TInventorySlot& row = inventory->slotAt(slot);
            if (!row.empty() && row.typeId == itemId && row.tier == tier)
                return true;
        }
    }
    return false;
}

uint32_t LocalPlayerSessionThing::getInventoryCount(const uint16_t itemId, const uint8_t tier)
{
    const auto key = std::make_pair(itemId, tier);
    const auto it = m_inventoryCountCache.find(key);
    if (it != m_inventoryCountCache.end())
        return it->second;

    uint32_t total = 0;

    if (tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance()) {
        for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot) {
            const tibia::container::TInventorySlot& row = inventory->slotAt(slot);
            if (!row.empty() && row.typeId == itemId && row.tier == tier)
                total += row.stackable ? std::max<uint16_t>(1, row.count) : 1;
        }
    }

    if (tibia::container::TContainerStorage* storage = tibia::container::TContainerStorage::instance()) {
        for (const tibia::container::TContainerID containerId : storage->openIds()) {
            const tibia::container::TStoredContainer* row = storage->containerAt(containerId);
            if (!row)
                continue;
            for (const tibia::container::TInventorySlot& slot : row->items) {
                if (!slot.empty() && slot.typeId == itemId && slot.tier == tier)
                    total += slot.stackable ? std::max<uint16_t>(1, slot.count) : 1;
            }
        }
    }

    if (const uint64_t maxUint32 = std::numeric_limits<uint32_t>::max(); total > maxUint32)
        total = maxUint32;

    return total;
}

void LocalPlayerSessionThing::takeScreenshot(const uint8_t type)
{
    auto action = tibia::input::mutableClone(*tibia::input::makeAction(tibia::input::EGameAction::TAKE_AUTOMATIC_SCREENSHOT));
    action->parameter = static_cast<int>(type);
    tibia::input::publish(action);
}

bool LocalPlayerSessionThing::hasSight(const Position& pos)
{
    const tibia::game::MapAwareRange range = tibia::game::awareRange();
    return m_position.isInRange(pos, range.left - 1, range.top - 1);
}

namespace tibia::game {

void syncLocalPlayerStorageWorldDraw()
{
    const LocalPlayerSessionThingPtr player = localPlayer();
    if (!player || player->getId() == 0)
        return;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return;
    storage->syncWorldDrawFrom(*player);
    if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(player->getId()))
        storage->publishCreatureUpdated(row);
}

} // namespace tibia::game

void LocalPlayerSessionThing::onCreate() {}

ThingType* LocalPlayerSessionThing::getThingType() const
{
    return tibia::game::rawThingType(m_outfit.isCreature() ? m_outfit.getId() : m_outfit.getAuxId(),
                                   m_outfit.getCategory());
}

void LocalPlayerSessionThing::setId(const uint32_t id)
{
    m_id = id;
}

uint32_t LocalPlayerSessionThing::getId()
{
    return m_id;
}

void LocalPlayerSessionThing::setName(const std::string_view name)
{
    if (name == m_name.getText())
        return;
    m_name.setText(name);
    if (tibia::game::TPlayerData* data = tibia::game::TPlayerData::liveInstance())
        data->setCharacterName(name);
}

std::string LocalPlayerSessionThing::getName()
{
    return m_name.getText();
}

void LocalPlayerSessionThing::setDirection(const tibia::client::Direction direction, const bool syncFieldStack)
{
    if (direction == tibia::client::InvalidDirection)
        return;

    m_direction = direction;

    if (direction == tibia::client::NorthEast || direction == tibia::client::SouthEast)
        m_numPatternX = tibia::client::East;
    else if (direction == tibia::client::NorthWest || direction == tibia::client::SouthWest)
        m_numPatternX = tibia::client::West;
    else
        m_numPatternX = direction;

    setAttachedEffectDirection(static_cast<tibia::client::Direction>(m_numPatternX));

    if (syncFieldStack) {
        if (tibia::worldmap::TWorldMapField* field = getTile())
            field->syncCreatureStackAppearance(getId());
    }
}

void LocalPlayerSessionThing::turn(const tibia::client::Direction direction)
{
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(getId())) {
            if (row->isWalking()) {
                m_walkTurnDirection = direction;
                row->setWalkTurnDirection(direction);
                return;
            }
        }
    }
    if (m_walking) {
        m_walkTurnDirection = direction;
        return;
    }
    setDirection(direction);
}

void LocalPlayerSessionThing::setOutfit(const Outfit& outfit, const bool fireEvent, const bool syncFieldStack)
{
    if (m_outfit == outfit)
        return;

    Outfit newOutfit = outfit;
    if (newOutfit.isInvalid())
        newOutfit.setCategory(newOutfit.getAuxId() > 0 ? ThingCategoryItem : ThingCategoryCreature);

    const Outfit oldOutfit = m_outfit;
    m_outfit = newOutfit;
    m_numPatternZ = 0;

    if (m_outfit.isInvalid())
        m_outfit.setCategory(m_outfit.getAuxId() > 0 ? ThingCategoryItem : ThingCategoryCreature);

    const auto* thingType = getThingType();
    if (!thingType) {
        g_logger.error("LocalPlayerSessionThing::setOutfit - invalid thing type for player {}.", getId());
        m_outfit = oldOutfit;
        return;
    }

    m_clientId = thingType->getId();

    if (m_outfit.hasMount())
        m_numPatternZ = std::min<int>(1, getNumPatternZ() - 1);

    if (fireEvent)
        publish("onOutfitChange", m_outfit, oldOutfit);

    if (syncFieldStack) {
        if (tibia::worldmap::TWorldMapField* field = getTile())
            field->syncCreatureStackAppearance(getId());
    }

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(getId())) {
            row->setOutfit(m_outfit);
            storage->publishCreatureUpdated(row, tibia::creatures::ECreatureUpdateAppearanceType::FullOutfit);
        }
    }
}

void LocalPlayerSessionThing::setBaseSpeed(const uint16_t baseSpeed)
{
    if (m_baseSpeed == baseSpeed)
        return;
    m_baseSpeed = baseSpeed;
    m_calculatedStepSpeed = tibia::creatures::calculatedStepSpeed(m_speed > 0 ? m_speed : baseSpeed);
}

void LocalPlayerSessionThing::setSpeed(const uint16_t speed)
{
    if (speed == m_speed)
        return;
    m_speed = speed;

    m_calculatedStepSpeed = tibia::creatures::calculatedStepSpeed(speed);

    tibia::game::localPlayerRegisterAdjustInvalidPosEvent();
}

void LocalPlayerSessionThing::setHealthPercent(const uint8_t healthPercent)
{
    m_healthPercent = healthPercent;
}

bool LocalPlayerSessionThing::isDead() const
{
    return m_healthPercent <= 0;
}

bool LocalPlayerSessionThing::isWalking()
{
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(getId()))
            return row->isWalking();
    }
    return m_walking;
}

void LocalPlayerSessionThing::stopWalk()
{
    if (!m_walking && !isWalking())
        return;
    terminateWalk();
}

Point LocalPlayerSessionThing::getWalkOffset()
{
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(getId()))
            return row->walkOffset();
    }
    return m_walkOffset;
}

uint16_t LocalPlayerSessionThing::getStepDuration(const bool ignoreDiagonal, const tibia::client::Direction dir)
{
    if (m_speed < 1)
        return 0;

    const auto& tilePos = dir == tibia::client::InvalidDirection ? m_position
                                                                 : getPosition().translatedToDirection(dir);

    auto* tile = tibia::worldmap::fieldAt(tilePos.isValid() ? tilePos : getPosition());

    const int serverBeat = tibia::game::getServerBeat();

    int groundSpeed = 0;
    if (tile)
        groundSpeed = tile->getGroundSpeed();
    if (groundSpeed == 0)
        groundSpeed = 150;

    if (groundSpeed != static_cast<int>(m_stepCache.groundSpeed) || m_speed != m_stepCache.speed) {
        m_stepCache.speed = m_speed;
        m_stepCache.groundSpeed = static_cast<uint16_t>(groundSpeed);

        uint32_t stepDuration = 1000 * groundSpeed;
        if (tibia::creatures::hasWireSpeedFormula())
            stepDuration /= std::max<uint16_t>(1, m_calculatedStepSpeed);
        else
            stepDuration /= m_speed;

        stepDuration = (stepDuration / serverBeat) * serverBeat;
        if (stepDuration == 0)
            stepDuration = serverBeat;

        m_stepCache.duration = static_cast<uint16_t>(stepDuration);
        m_stepCache.walkDuration = static_cast<uint16_t>(std::max<int>(stepDuration, serverBeat * 2));

        m_stepCache.diagonalDuration = stepDuration *
            (ignoreDiagonal ? 1
                            : (tibia::game::clientRevision() > 810 || tibia::config::forcingNewWalkingFormula()
                                   ? tibia::config::playerDiagonalWalkSpeed()
                                   : 3));
    }

    return m_stepCache.getDuration(dir);
}

namespace tibia::game {

void wireLocalPlayerOnAppear(LocalPlayerSessionThing& player)
{
    if (player.m_disappearEvent) {
        player.m_disappearEvent->cancel();
        player.m_disappearEvent = nullptr;
    }

    const Position pos = player.getPosition();
    if (player.m_removed) {
        if (!player.isWalking())
            player.stopWalk();
        player.m_removed = false;
    } else if (player.m_oldPosition != pos) {
        if (player.isWalking() && player.m_oldPosition.z == pos.z
            && player.m_oldPosition.isInRange(pos, 1, 1, /*ignoreZ=*/true)) {
            player.m_oldPosition = pos;
        } else {
            localPlayerHandleTeleport();
            player.m_oldPosition = pos;
        }
    }

    notify("onCreatureAppear", player.getId());
}

void wireLocalPlayerOnDisappear(LocalPlayerSessionThing& player)
{
    if (player.m_disappearEvent)
        player.m_disappearEvent->cancel();

    player.m_oldPosition = player.getPosition();
    notify("onCreatureDisappear", player.getId());

    const auto self = player.sessionThingPtr();
    player.m_disappearEvent = tibia::schedule::post([self] {
        self->m_removed = true;
        self->stopWalk();
        self->m_oldPosition = {};
        self->m_disappearEvent = nullptr;

        if (attackingCreatureId() == self->getId())
            cancelAttack();
        else if (followingCreatureId() == self->getId())
            cancelFollow();
    });

    player.Thing::onDisappear();
}

} // namespace tibia::game

void LocalPlayerSessionThing::onAppear()
{
    tibia::game::wireLocalPlayerOnAppear(*this);
}

void LocalPlayerSessionThing::onDisappear()
{
    tibia::game::wireLocalPlayerOnDisappear(*this);
}

