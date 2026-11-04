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

#include "localplayer.h"

#include <framework/core/gamehooks.h>

#include "container/containerciptypes.h"
#include "creatures/creaturestorage.h"
#include "game.h"
#include "client/item.h"
#include "map.h"
#include "worldmap/worldmapfield.h"
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "input/playermovementgameactionhandler.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/gamesession.h>

namespace {

std::shared_ptr<tibia::creatures::TCreature> localWalkRow(LocalPlayer& player)
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage || player.getId() == 0)
        return nullptr;
    return storage->getOrCreate(player.getId());
}

}

void LocalPlayer::lockWalk(const uint16_t millis)
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        row->lockWalk(millis);
}

void LocalPlayer::unlockWalk()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        row->unlockWalk();
}

bool LocalPlayer::canWalk(const bool ignoreLock)
{
    // Storage row is authority (same rules as WalkCalculator::canStep).
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this)) {
        if (isDead())
            return false;
        if (row->isWalkLocked() && !ignoreLock)
            return false;
        if (g_game.getWalkMaxSteps() > 0) {
            if (row->preWalkingSize() > g_game.getWalkMaxSteps())
                return false;
        } else if (row->position() != row->serverPosition()) {
            return false;
        }
        if (row->isWalking()) {
            if (row->isAutoWalking())
                return true;
            if (row->isPreWalking())
                return false;
        }
        return row->walkTicksElapsed() >= row->stepDuration();
    }

    if (isDead())
        return false;
    if (isWalkLocked() && !ignoreLock)
        return false;
    if (g_game.getWalkMaxSteps() > 0) {
        if (getPreWalkingSize() > g_game.getWalkMaxSteps())
            return false;
    } else if (getPosition() != getServerPosition()) {
        return false;
    }
    if (isWalking()) {
        if (isAutoWalking())
            return true;
        if (isPreWalking())
            return false;
    }
    return m_walkTimer.ticksElapsed() >= getStepDuration();
}

void LocalPlayer::walk(const Position& oldPos, const Position& newPos)
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this)) {
        row->setAutoWalkRetries(0);

        if (row->isPreWalking() && !row->preWalks().empty() && newPos == row->preWalks().front()) {
            row->popPreWalkFront();
            return;
        }

        cancelAdjustInvalidPosEvent();
        row->clearPreWalks();
        m_serverWalk = true;

        Creature::walk(oldPos, newPos);
        return;
    }

    cancelAdjustInvalidPosEvent();
    m_serverWalk = true;
    Creature::walk(oldPos, newPos);
}

void LocalPlayer::preWalk(tibia::client::Direction direction)
{
    m_lastMapDuration = -1;

    const auto& oldPos = getPosition();
    const Position predicted = oldPos.translatedToDirection(direction);
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        Creature::walk(oldPos, row->pushPreWalk(predicted));
    else
        Creature::walk(oldPos, predicted);
    Creature::onPositionChange(getLastStepToPosition(), getLastStepFromPosition());
    registerAdjustInvalidPosEvent();
}

void LocalPlayer::onWalking()
{
    // Pre-walk impassable collision is owned by WalkCalculator::advance (storage check).
}

void LocalPlayer::cancelPreWalkCollision()
{
    cancelWalk();
    g_map.notificateTileUpdate(getPosition(), asLocalPlayer(), tibia::client::OPERATION_CLEAN);
}

void LocalPlayer::registerAdjustInvalidPosEvent() {
    cancelAdjustInvalidPosEvent();
    m_adjustInvalidPosEvent = tibia::schedule::after([this, self = asLocalPlayer()] {
        if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*self))
            row->clearPreWalks();
        g_game.resetMapUpdatedAt();
        m_adjustInvalidPosEvent = nullptr;
    }, std::min<int>(std::max<int>(getStepDuration(), g_game.getPing()) + 100, 1000));
}

void LocalPlayer::cancelAdjustInvalidPosEvent() {
    if (!m_adjustInvalidPosEvent) return;
    m_adjustInvalidPosEvent->cancel();
    m_adjustInvalidPosEvent = nullptr;
}

bool LocalPlayer::retryAutoWalk()
{
    const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this);
    if (!row || !row->isAutoWalking())
        return false;

    // Client keeps the destination across SnapBack and repaths with backoff.
    // Do not call Game::stop() here — that clears auto-walk intent.
    constexpr int MaxAutoWalkRetries = 100;
    if (row->autoWalkRetries() >= MaxAutoWalkRetries) {
        stopAutoWalk();
        return false;
    }

    if (m_autoWalkContinueEvent)
        m_autoWalkContinueEvent->cancel();

    const Position autoWalkDest = row->autoWalkDestination();
    const int delayMs = 200 + static_cast<int>(row->autoWalkRetries()) * 100;
    m_autoWalkContinueEvent = tibia::schedule::after(
        [thisPtr = asLocalPlayer(), autoWalkDest] { thisPtr->autoWalk(autoWalkDest, true); }, delayMs);

    row->setAutoWalkRetries(static_cast<uint8_t>(row->autoWalkRetries() + 1));
    return true;
}

void LocalPlayer::cancelWalk(const tibia::client::Direction direction)
{
    // only cancel client side walks
    if (isWalking() && isPreWalking())
        stopWalk();

    g_map.notificateCameraMove(m_walkOffset);

    if (m_adjustInvalidPosEvent) {
        m_adjustInvalidPosEvent->execute();
    }

    lockWalk();
    if (retryAutoWalk()) return;

    // turn to the cancel direction
    if (direction != tibia::client::InvalidDirection)
        setDirection(direction);

    (void)direction;
}

bool LocalPlayer::autoWalk(const Position& destination, const bool retry, const tibia::client::AutoWalkPathMode pathMode)
{
    const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this);

    // reset state
    if (row) {
        row->setAutoWalkDestination({});
        row->setLastAutoWalkPosition({});
    }
    if (m_autoWalkContinueEvent)
        m_autoWalkContinueEvent->cancel();
    m_autoWalkContinueEvent = nullptr;

    if (!retry && row) {
        row->setAutoWalkRetries(0);
        row->setAutoWalkPathMode(pathMode);
    }

    if (destination == m_position)
        return true;

    if (row)
        row->setAutoWalkDestination(destination);

    const tibia::client::AutoWalkPathMode findMode =
        row ? row->autoWalkPathMode() : pathMode;

    g_map.findPathAsync(m_position, destination, [self = asLocalPlayer()](const auto& result) {
        const std::shared_ptr<tibia::creatures::TCreature> stored = localWalkRow(*self);
        if (!stored || stored->autoWalkDestination() != result->destination)
            return;

        if (result->status != tibia::client::PathFindResultOk) {
            // qtclient: keep destination and repath until arrival or user cancel.
            constexpr int MaxAutoWalkRetries = 100;
            if (stored->autoWalkRetries() < MaxAutoWalkRetries) {
                if (self->m_autoWalkContinueEvent)
                    self->m_autoWalkContinueEvent->cancel();
                const Position dest = result->destination;
                const int delayMs = 200 + static_cast<int>(stored->autoWalkRetries()) * 100;
                self->m_autoWalkContinueEvent =
                    tibia::schedule::after([self, dest] { self->autoWalk(dest, true); }, delayMs);
                stored->setAutoWalkRetries(static_cast<uint8_t>(stored->autoWalkRetries() + 1));
                return;
            }
            stored->setAutoWalkDestination({});
            return;
        }

        if (result->path.size() > 127)
            result->path.resize(127);

        if (result->path.empty()) {
            // qtclient/CIP: keep destination and repath — empty can be a transient
            // pathfinder cost miss or a fog-truncated leftover, not a final failure.
            constexpr int MaxAutoWalkRetries = 100;
            if (stored->autoWalkRetries() < MaxAutoWalkRetries) {
                if (self->m_autoWalkContinueEvent)
                    self->m_autoWalkContinueEvent->cancel();
                const Position dest = result->destination;
                const int delayMs = 200 + static_cast<int>(stored->autoWalkRetries()) * 100;
                self->m_autoWalkContinueEvent =
                    tibia::schedule::after([self, dest] { self->autoWalk(dest, true); }, delayMs);
                stored->setAutoWalkRetries(static_cast<uint8_t>(stored->autoWalkRetries() + 1));
                return;
            }
            stored->setAutoWalkDestination({});
            return;
        }

        // Client continue: last tile of this ≤127 chunk, not the final goal.
        // onPositionChange resumes autoWalk when the player reaches that tile.
        const std::vector<Position> waypoints = result->start.translatedToDirections(result->path);
        if (!waypoints.empty()) {
            const Position& last = waypoints.back();
            stored->setLastAutoWalkPosition(
                (last != stored->autoWalkDestination()) ? last : Position{});
        }

        g_game.autoWalk(result->path, result->start);
    }, findMode);

    if (!retry)
        lockWalk();

    return true;
}

bool LocalPlayer::isWalkLocked()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        return row->isWalkLocked();
    return false;
}

bool LocalPlayer::isPreWalking()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        return row->isPreWalking();
    return false;
}

bool LocalPlayer::isAutoWalking()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        return row->isAutoWalking();
    return false;
}

Position LocalPlayer::getPosition()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this)) {
        if (row->isPreWalking())
            return row->predictedPosition();
    }
    return m_position;
}

void LocalPlayer::resetPreWalk()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        row->clearPreWalks();
}

size_t LocalPlayer::getPreWalkingSize()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        return row->preWalkingSize();
    return 0;
}

void LocalPlayer::stopAutoWalk()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this))
        row->clearAutoWalk();
    m_knownCompletePath = false;

    if (m_autoWalkContinueEvent)
        m_autoWalkContinueEvent->cancel();

    // CIP: stopAutoWalk / arrival / server cancel clear the live Target intent so
    // TPlayerMovementIntentHandler stops replanning.
    if (tibia::input::TPlayerMovementGameActionHandler* movement =
            tibia::input::TPlayerMovementGameActionHandler::instance())
        movement->clearTargetIntent();
}

void LocalPlayer::terminateWalk()
{
    Creature::terminateWalk();
    m_serverWalk = false;
}

void LocalPlayer::applyStorageWalkTerminated()
{
    Creature::applyStorageWalkTerminated();
    m_serverWalk = false;
}

void LocalPlayer::onPositionChange(const Position& newPos, const Position& oldPos)
{
    // Pre-walk already emitted Creature::onPositionChange for the predicted step;
    // still run qtclient/CIP autoWalk continue against the confirmed tile.
    if (!isPreWalking())
        Creature::onPositionChange(newPos, oldPos);

    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*this)) {
        if (newPos == row->autoWalkDestination())
            stopAutoWalk();
        else if (row->isAutoWalking() && newPos == row->lastAutoWalkPosition())
            // retry=true: do not lockWalk again mid-route (CIP Target replan).
            autoWalk(row->autoWalkDestination(), true);
    }

    if (!isPreWalking())
        m_serverWalk = false;
}

void LocalPlayer::setStatesAndRottenBlood(const uint64_t states, const uint8_t rottenBloodState)
{
    if (m_states == states && m_rottenBloodState == rottenBloodState)
        return;

    const uint64_t oldStates = m_states;
    m_states = states;
    m_rottenBloodState = rottenBloodState;

    if (isParalyzed() && isWalking() && m_serverWalk)
        updateWalk();

    publish("onStatesChange", states, oldStates);
}

void LocalPlayer::setSkill(const tibia::client::Skill skillId, const uint16_t level, const uint16_t levelPercent)
{
    if (skillId >= tibia::client::LastSkill) {
        g_logger.traceError("invalid skill");
        return;
    }

    auto& skill = m_skills[skillId];

    const uint16_t oldLevel = skill.level;
    const uint16_t oldLevelPercent = skill.levelPercent;

    if (level == oldLevel && levelPercent == oldLevelPercent)
        return;

    skill.level = level;
    skill.levelPercent = levelPercent;

    publish("onSkillChange", skillId, level, levelPercent, oldLevel, oldLevelPercent);
}

void LocalPlayer::setSkillDetail(const tibia::client::Skill skillId, const uint16_t loyaltyLevel, const uint16_t percentHundredths)
{
    if (skillId >= tibia::client::LastSkill)
        return;
    m_skills[skillId].loyaltyLevel = loyaltyLevel;
    m_skills[skillId].percentHundredths = percentHundredths;
}

void LocalPlayer::setBaseSkill(const tibia::client::Skill skill, const uint16_t baseLevel)
{
    if (skill >= tibia::client::LastSkill) {
        g_logger.traceError("invalid skill");
        return;
    }

    const uint16_t oldBaseLevel = m_skills[skill].baseLevel;
    if (baseLevel == oldBaseLevel)
        return;

    m_skills[skill].baseLevel = baseLevel;

    publish("onBaseSkillChange", skill, baseLevel, oldBaseLevel);
}

void LocalPlayer::setHealth(const uint32_t health, const uint32_t maxHealth)
{
    if (m_health != health || m_maxHealth != maxHealth) {
        const uint32_t oldHealth = m_health;
        const uint32_t oldMaxHealth = m_maxHealth;
        m_health = health;
        m_maxHealth = maxHealth;

        publish("onHealthChange", health, maxHealth, oldHealth, oldMaxHealth);

        if (isDead()) {
            if (isPreWalking())
                stopWalk();
            lockWalk();
        }
    }
}

void LocalPlayer::setFreeCapacity(const uint32_t freeCapacity)
{
    if (m_freeCapacity == freeCapacity)
        return;

    const uint32_t oldFreeCapacity = m_freeCapacity;
    m_freeCapacity = freeCapacity;

    publish("onFreeCapacityChange", freeCapacity, oldFreeCapacity);
}

void LocalPlayer::setTotalCapacity(const uint32_t totalCapacity)
{
    if (m_totalCapacity == totalCapacity)
        return;

    const uint32_t oldTotalCapacity = m_totalCapacity;
    m_totalCapacity = totalCapacity;

    publish("onTotalCapacityChange", totalCapacity, oldTotalCapacity);
}

void LocalPlayer::setBaseCapacity(const uint32_t baseCapacity)
{
    if (m_baseCapacity == baseCapacity)
        return;

    m_baseCapacity = baseCapacity;
    publish("onBaseCapacityChange", baseCapacity);
}

void LocalPlayer::setExperience(const uint64_t experience)
{
    if (m_experience == experience)
        return;

    const uint64_t oldExperience = m_experience;
    m_experience = experience;

    publish("onExperienceChange", experience, oldExperience);
}

void LocalPlayer::setLevel(const uint16_t level, const uint16_t levelPercent)
{
    if (m_level == level && m_levelPercent == levelPercent)
        return;

    const uint16_t oldLevel = m_level;
    const uint16_t oldLevelPercent = m_levelPercent;

    m_level = level;
    m_levelPercent = levelPercent;

    publish("onLevelChange", level, levelPercent, oldLevel, oldLevelPercent);
}

uint16_t LocalPlayer::getLevelPercent()
{
    return g_game.getFeature(tibia::client::GameLevelPercentU16) ? m_levelPercent / 100 : m_levelPercent;
}

uint16_t LocalPlayer::getLevelPercentHundredths()
{
    return g_game.getFeature(tibia::client::GameLevelPercentU16) ? m_levelPercent : m_levelPercent * 100;
}

void LocalPlayer::setMana(const uint32_t mana, const uint32_t maxMana)
{
    if (m_mana == mana && m_maxMana == maxMana)
        return;

    const uint32_t oldMana = m_mana;
    const uint32_t oldMaxMana = m_maxMana;
    m_mana = mana;
    m_maxMana = maxMana;

    publish("onManaChange", mana, maxMana, oldMana, oldMaxMana);
}

void LocalPlayer::setManaShield(const uint32_t manaShield, const uint32_t maxManaShield)
{
    if (m_manaShield == manaShield && m_maxManaShield == maxManaShield)
        return;

    const uint32_t oldManaShield = m_manaShield;
    const uint32_t oldMaxManaShield = m_maxManaShield;
    m_manaShield = manaShield;
    m_maxManaShield = maxManaShield;

    publish("onManaShieldChange", manaShield, maxManaShield, oldManaShield, oldMaxManaShield);
}

void LocalPlayer::setMagicLevel(const uint16_t magicLevel, const uint16_t magicLevelPercent)
{
    if (m_magicLevel == magicLevel && m_magicLevelPercent == magicLevelPercent)
        return;

    const uint16_t oldMagicLevel = m_magicLevel;
    const uint16_t oldMagicLevelPercent = m_magicLevelPercent;

    m_magicLevel = magicLevel;
    m_magicLevelPercent = magicLevelPercent;

    publish("onMagicLevelChange", magicLevel, magicLevelPercent, oldMagicLevel, oldMagicLevelPercent);
}

void LocalPlayer::setBaseMagicLevel(const uint16_t baseMagicLevel)
{
    if (m_baseMagicLevel == baseMagicLevel)
        return;

    const uint16_t oldBaseMagicLevel = m_baseMagicLevel;
    m_baseMagicLevel = baseMagicLevel;

    publish("onBaseMagicLevelChange", baseMagicLevel, oldBaseMagicLevel);
}

void LocalPlayer::setSoul(const uint8_t soul)
{
    if (m_soul == soul)
        return;

    const uint8_t oldSoul = m_soul;
    m_soul = soul;

    publish("onSoulChange", soul, oldSoul);
}

void LocalPlayer::setStamina(const uint16_t stamina)
{
    if (m_stamina == stamina)
        return;

    const uint16_t oldStamina = m_stamina;
    m_stamina = stamina;

    publish("onStaminaChange", stamina, oldStamina);
}

void LocalPlayer::setInventoryItem(const tibia::client::InventorySlot inventory)
{
    if (inventory >= tibia::client::LastInventorySlot) {
        g_logger.traceError("invalid slot");
        return;
    }

    // Phase 5.0-D: no LocalPlayer ItemPtr array — TInventoryContainer is authority.
    publish("onInventoryChange", inventory);
}

const tibia::container::TInventorySlot& LocalPlayer::inventorySlot(const tibia::client::InventorySlot inventory) const
{
    static const tibia::container::TInventorySlot empty;
    if (inventory >= tibia::client::LastInventorySlot)
        return empty;
    if (tibia::container::TInventoryContainer* storage = tibia::container::TInventoryContainer::instance())
        return storage->slotAt(static_cast<int>(inventory));
    return empty;
}

ItemPtr LocalPlayer::getInventoryItem(const tibia::client::InventorySlot inventory)
{
    const tibia::container::TInventorySlot& slot = inventorySlot(inventory);
    if (slot.empty())
        return nullptr;
    return g_game.ephemeralItemForWire(slot.typeId, Position(UINT16_MAX, static_cast<uint16_t>(inventory), 0), 0,
                                       slot.stackable ? slot.count : 0, slot.tier,
                                       slot.fluidOrSplash ? slot.fluid : 0);
}

void LocalPlayer::setInventoryCountCache(std::map<std::pair<uint16_t, uint8_t>, uint32_t> counts)
{
    m_inventoryCountCache = std::move(counts);
}

bool LocalPlayer::hasEquippedItemId(const uint16_t itemId, const uint8_t tier)
{
    if (itemId == 0)
        return false;

    tibia::container::TInventoryContainer* inventory = tibia::container::TInventoryContainer::instance();
    if (!inventory)
        return false;
    for (int slot = tibia::client::InventorySlotHead; slot < tibia::client::LastInventorySlot; ++slot) {
        const tibia::container::TInventorySlot& row = inventory->slotAt(slot);
        if (!row.empty() && row.typeId == itemId && row.tier == tier)
            return true;
    }
    return false;
}

uint32_t LocalPlayer::getInventoryCount(const uint16_t itemId, const uint8_t tier)
{
    if (std::cmp_equal(itemId, 0))
        return 0;

    const auto key = std::make_pair(itemId, tier);
    const auto it = m_inventoryCountCache.find(key);
    if (it != m_inventoryCountCache.end()) {
        return it->second;
    }

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

    if (const uint64_t maxUint32 = std::numeric_limits<uint32_t>::max(); total > maxUint32) {
        total = maxUint32;
    }

    return total;
}

void LocalPlayer::setPremium(const bool premium)
{
    if (m_premium == premium)
        return;

    m_premium = premium;

    publish("onPremiumChange", premium);
}

void LocalPlayer::setRegenerationTime(const uint16_t regenerationTime)
{
    if (m_regenerationTime == regenerationTime)
        return;

    const uint16_t oldRegenerationTime = m_regenerationTime;
    m_regenerationTime = regenerationTime;

    publish("onRegenerationChange", regenerationTime, oldRegenerationTime);
}

void LocalPlayer::setOfflineTrainingTime(const uint16_t offlineTrainingTime)
{
    if (m_offlineTrainingTime == offlineTrainingTime)
        return;

    const uint16_t oldOfflineTrainingTime = m_offlineTrainingTime;
    m_offlineTrainingTime = offlineTrainingTime;

    publish("onOfflineTrainingChange", offlineTrainingTime, oldOfflineTrainingTime);
}

void LocalPlayer::setSpells(const std::vector<uint16_t>& spells)
{
    if (m_spells == spells)
        return;

    const std::vector<uint16_t> oldSpells = m_spells;
    m_spells = spells;

    publish("onSpellsChange", spells, oldSpells);
}

void LocalPlayer::setBlessings(const uint16_t blessings, const uint8_t blessVisualState)
{
    if (blessings == m_blessings && blessVisualState == m_blessVisualState)
        return;

    const uint16_t oldBlessings = m_blessings;
    m_blessings = blessings;
    m_blessVisualState = blessVisualState;

    publish("onBlessingsChange", blessings, oldBlessings, blessVisualState);
}

void LocalPlayer::takeScreenshot(const uint8_t type)
{
    auto action = tibia::input::mutableClone(*tibia::input::makeAction(tibia::input::EGameAction::TAKE_AUTOMATIC_SCREENSHOT));
    action->parameter = static_cast<int>(type);
    tibia::input::publish(action);
}

void LocalPlayer::setResourceBalance(const tibia::client::ResourceTypes_t type, const uint64_t value)
{
    const auto it = m_resourcesBalance.find(type);
    const uint64_t oldBalance = it != m_resourcesBalance.end() ? it->second : 0;
    // First receive must stick even when value is 0 (CIP setGuild sets hasGuild on
    // ResourceBalance 0x02 regardless of whether the balance changed).
    if (it != m_resourcesBalance.end() && value == oldBalance)
        return;

    m_resourcesBalance[type] = value;
    tibia::game::notify("onResourcesBalanceChange", value, oldBalance, type);
    tibia::game::TGameNotify::instance()->emitResourcesBalanceChanged(
        static_cast<qint64>(value), static_cast<qint64>(oldBalance), static_cast<int>(type));
}

bool LocalPlayer::hasSight(const Position& pos)
{
    return m_position.isInRange(pos, g_map.getAwareRange().left - 1, g_map.getAwareRange().top - 1);
}

void LocalPlayer::setFlatDamageHealing(uint16_t flatBonus)
{
    if (m_flatDamageHealing == flatBonus)
        return;

    m_flatDamageHealing = flatBonus;

    publish("onFlatDamageHealingChange", flatBonus);
}

void LocalPlayer::setAttackInfo(uint16_t attackValue, uint8_t attackElement)
{
    if (m_attackValue == attackValue && m_attackElement == attackElement)
        return;

    m_attackValue = attackValue;
    m_attackElement = attackElement;

    publish("onAttackInfoChange", attackValue, attackElement);
}

void LocalPlayer::setConvertedDamage(double convertedDamage, uint8_t convertedElement)
{
    if (m_convertedDamage == convertedDamage && m_convertedElement == convertedElement)
        return;

    m_convertedDamage = convertedDamage;
    m_convertedElement = convertedElement;

    publish("onConvertedDamageChange", convertedDamage, convertedElement);
}

void LocalPlayer::setImbuements(double lifeLeech, double manaLeech, double critChance, double critDamage, double onslaught)
{
    if (m_lifeLeech == lifeLeech && m_manaLeech == manaLeech && m_critChance == critChance &&
        m_critDamage == critDamage && m_onslaught == onslaught)
        return;

    m_lifeLeech = lifeLeech;
    m_manaLeech = manaLeech;
    m_critChance = critChance;
    m_critDamage = critDamage;
    m_onslaught = onslaught;

    publish("onImbuementsChange", lifeLeech, manaLeech, critChance, critDamage, onslaught);
}

void LocalPlayer::setDefenseInfo(uint16_t defense, uint16_t armor, uint16_t mantra, double mitigation, double dodge, uint16_t damageReflection)
{
    if (m_defense == defense && m_armor == armor && m_mantra == mantra && m_mitigation == mitigation &&
        m_dodge == dodge && m_damageReflection == damageReflection)
        return;

    m_defense = defense;
    m_armor = armor;
    m_mantra = mantra;
    m_mitigation = mitigation;
    m_dodge = dodge;
    m_damageReflection = damageReflection;

    publish("onDefenseInfoChange", defense, armor,mantra, mitigation, dodge, damageReflection);
}

void LocalPlayer::setCombatAbsorbValues(const std::map<uint8_t, double>& absorbValues)
{
    if (m_combatAbsorbValues == absorbValues)
        return;

    m_combatAbsorbValues = absorbValues;

    publish("onCombatAbsorbValuesChange", absorbValues);
}

void LocalPlayer::setForgeBonuses(double momentum, double transcendence, double amplification)
{
    if (m_momentum == momentum && m_transcendence == transcendence && m_amplification == amplification)
        return;

    m_momentum = momentum;
    m_transcendence = transcendence;
    m_amplification = amplification;

    publish("onForgeBonusesChange", momentum, transcendence, amplification);
}

void LocalPlayer::setExperienceRate(tibia::client::ExperienceRate_t type, uint16_t value)
{
    if (m_experienceRates[type] == value)
        return;

    m_experienceRates[type] = value;

    publish("onExperienceRateChange", type, value);
}

void LocalPlayer::setStoreExpBoostTime(uint16_t value)
{
    if (m_storeExpBoostTime == value)
        return;

    m_storeExpBoostTime = value;
}

void LocalPlayer::setStoreExpBoostPurchasable(const bool purchasable)
{
    if (m_storeExpBoostPurchasable == purchasable)
        return;

    m_storeExpBoostPurchasable = purchasable;
    publish("onStoreExpBoostPurchasableChange", purchasable);
}

void LocalPlayer::setHarmony(const uint8_t harmony)
{
    const uint8_t oldHarmony = m_harmony;
    m_harmony = harmony;

    publish("onHarmonyChange", harmony, oldHarmony);
}

void LocalPlayer::setSerene(const bool serene)
{
    const bool oldSerene = m_serene;
    m_serene = serene;

    publish("onSereneChange", serene, oldSerene);
}

void LocalPlayer::setInformationChannel(const uint32_t durationMs)
{
    m_informationChannelStart = g_clock.millis();
    m_informationChannelDuration = std::max<uint32_t>(durationMs, 1);
}

float LocalPlayer::getInformationChannelProgress() const
{
    if (m_informationChannelDuration == 0)
        return -1.f;

    const auto elapsed = static_cast<float>(g_clock.millis() - m_informationChannelStart);
    return std::min(100.f, elapsed * 100.f / static_cast<float>(m_informationChannelDuration));
}

void LocalPlayer::setSoftDead(const bool softDead)
{
    if (m_softDead == softDead)
        return;
    m_softDead = softDead;
    if (softDead) {
        // Match server softDespawn clamp so any remaining HUD reads 0 HP immediately.
        setHealth(0, m_maxHealth > 0 ? m_maxHealth : m_health);
        setHealthPercent(0);
    }
}
