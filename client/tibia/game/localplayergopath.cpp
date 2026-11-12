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

#include "game/localplayergopath.h"
#include "game/localplayersessionthing.h"
#include "creatures/creaturestorage.h"
#include "creatures/movementtweens.h"
#include "input/playermovementgameactionhandler.h"
#include "worldmap/pathfindtypes.h"
#include "worldmap/worldmapfield.h"
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/gamesession_forward.h>
#include <tibia/game/mapsession.h>
#include <tibia/schedule/schedule.h>

#include <deque>

namespace {

std::shared_ptr<tibia::creatures::TCreature> localWalkRow(LocalPlayerSessionThing& player)
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage || player.getId() == 0)
        return nullptr;
    return storage->getOrCreate(player.getId());
}

} // namespace

LocalPlayerGoPath::LocalPlayerGoPath(LocalPlayerSessionThing& owner) : m_owner(owner) {}

void LocalPlayerGoPath::lockWalk(const uint16_t millis)
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        row->lockWalk(millis);
}

void LocalPlayerGoPath::unlockWalk()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        row->unlockWalk();
}

bool LocalPlayerGoPath::canWalk(const bool ignoreLock)
{
    return tibia::creatures::canBeginStep(ignoreLock) && !m_owner.isDead();
}

void LocalPlayerGoPath::predictStep(tibia::client::Direction direction)
{
    const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner);
    if (!row)
        return;
    m_owner.goPathResetLastMapDuration();

    const Position oldPos = row->isPreWalking() ? row->predictedPosition() : row->position();
    const Position from = oldPos.isValid() ? oldPos : m_owner.getPosition();
    const Position predicted = from.translatedToDirection(direction);
    row->pushPreWalk(predicted);
    row->beginWalk(from, predicted);
    m_owner.setPosition(predicted);
    m_owner.setDirection(from.getDirectionFromPosition(predicted));
    tibia::game::notify("onPositionChange", predicted, from);
    registerAdjustInvalidPosEvent();
}

void LocalPlayerGoPath::armGoPathDirs(const std::vector<tibia::client::Direction>& dirs)
{
    const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner);
    if (!row)
        return;
    std::deque<tibia::client::Direction> queue;
    for (const tibia::client::Direction dir : dirs) {
        if (dir != tibia::client::InvalidDirection)
            queue.push_back(dir);
    }
    row->setAutoWalkDirs(std::move(queue));
}

bool LocalPlayerGoPath::tryBeginNextGoPathStep()
{
    const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner);
    if (!row || !row->isAutoWalking() || !row->hasAutoWalkDirs())
        return false;
    if (!canWalk(true))
        return false;

    const tibia::client::Direction direction = row->peekAutoWalkDir();
    if (direction == tibia::client::InvalidDirection)
        return false;

    const Position from = m_owner.getPosition();
    const Position to = from.translatedToDirection(direction);
    const auto& toTile = tibia::worldmap::fieldAt(to);
    if (!toTile || !toTile->isWalkable())
        return false;

    row->popAutoWalkDirFront();
    predictStep(direction);
    return true;
}

void LocalPlayerGoPath::onWalking()
{
    tryBeginNextGoPathStep();
}

void LocalPlayerGoPath::registerAdjustInvalidPosEvent()
{
    cancelAdjustInvalidPosEvent();
    m_adjustInvalidPosEvent = tibia::schedule::after([this, self = m_owner.sessionThingPtr()] {
        if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(*self))
            row->clearPreWalks();
        tibia::game::resetMapUpdatedAt();
        m_adjustInvalidPosEvent = nullptr;
    }, std::min<int>(std::max<int>(m_owner.getStepDuration(), tibia::game::getPing()) + 100, 1000));
}

void LocalPlayerGoPath::cancelAdjustInvalidPosEvent()
{
    if (!m_adjustInvalidPosEvent)
        return;
    m_adjustInvalidPosEvent->cancel();
    m_adjustInvalidPosEvent = nullptr;
}

bool LocalPlayerGoPath::retryAutoWalk()
{
    const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner);
    if (!row || !row->isAutoWalking())
        return false;

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
        [thisPtr = m_owner.sessionThingPtr(), autoWalkDest] { thisPtr->autoWalk(autoWalkDest, true); }, delayMs);

    row->setAutoWalkRetries(static_cast<uint8_t>(row->autoWalkRetries() + 1));
    return true;
}

void LocalPlayerGoPath::cancelWalk(const tibia::client::Direction direction)
{
    if (m_owner.isWalking() && isPreWalking())
        m_owner.stopWalk();

    const Point offset = m_owner.getWalkOffset();
    tibia::game::notifyCameraMove({ offset.x, offset.y });

    if (m_adjustInvalidPosEvent)
        m_adjustInvalidPosEvent->execute();

    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        row->clearAutoWalkDirs();
    m_pendingChunkContinue = {};

    if (direction != tibia::client::InvalidDirection) {
        const Position avoid = m_owner.getPosition().translatedToDirection(direction);
        tibia::game::setPathAvoid({ avoid.x, avoid.y, avoid.z }, true);
    }

    lockWalk();
    if (retryAutoWalk())
        return;

    if (direction != tibia::client::InvalidDirection)
        m_owner.setDirection(direction);
}

bool LocalPlayerGoPath::autoWalk(const Position& destination, const bool retry,
                                 const tibia::client::AutoWalkPathMode pathMode)
{
    const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner);

    if (row) {
        row->setAutoWalkDestination({});
        row->setLastAutoWalkPosition({});
        row->clearAutoWalkDirs();
    }
    m_pendingChunkContinue = {};
    if (m_autoWalkContinueEvent)
        m_autoWalkContinueEvent->cancel();
    m_autoWalkContinueEvent = nullptr;

    if (!retry) {
        tibia::game::clearPathAvoids();
        if (row) {
            row->setAutoWalkRetries(0);
            row->setAutoWalkPathMode(pathMode);
        }
    }

    const Position& here = m_owner.getServerPosition();
    if (destination == here)
        return true;

    if (row)
        row->setAutoWalkDestination(destination);

    const tibia::client::AutoWalkPathMode findMode = row ? row->autoWalkPathMode() : pathMode;

    tibia::game::findPathAsync({ here.x, here.y, here.z },
                               { destination.x, destination.y, destination.z },
                               [self = m_owner.sessionThingPtr(), goPath = this](const auto& result) {
        const std::shared_ptr<tibia::creatures::TCreature> stored = localWalkRow(*self);
        if (!stored || stored->autoWalkDestination() != result->destination)
            return;

        if (result->status != tibia::client::PathFindResultOk) {
            constexpr int MaxAutoWalkRetries = 100;
            if (stored->autoWalkRetries() < MaxAutoWalkRetries) {
                if (goPath->m_autoWalkContinueEvent)
                    goPath->m_autoWalkContinueEvent->cancel();
                const Position dest = result->destination;
                const int delayMs = 200 + static_cast<int>(stored->autoWalkRetries()) * 100;
                goPath->m_autoWalkContinueEvent =
                    tibia::schedule::after([self, dest] { self->autoWalk(dest, true); }, delayMs);
                stored->setAutoWalkRetries(static_cast<uint8_t>(stored->autoWalkRetries() + 1));
                return;
            }
            stored->setAutoWalkDestination({});
            return;
        }

        constexpr size_t CipGoPathMaxSteps = 9;
        if (result->path.size() > CipGoPathMaxSteps)
            result->path.resize(CipGoPathMaxSteps);

        if (result->path.empty()) {
            constexpr int MaxAutoWalkRetries = 100;
            if (stored->autoWalkRetries() < MaxAutoWalkRetries) {
                if (goPath->m_autoWalkContinueEvent)
                    goPath->m_autoWalkContinueEvent->cancel();
                const Position dest = result->destination;
                const int delayMs = 200 + static_cast<int>(stored->autoWalkRetries()) * 100;
                goPath->m_autoWalkContinueEvent =
                    tibia::schedule::after([self, dest] { self->autoWalk(dest, true); }, delayMs);
                stored->setAutoWalkRetries(static_cast<uint8_t>(stored->autoWalkRetries() + 1));
                return;
            }
            stored->setAutoWalkDestination({});
            return;
        }

        if (result->start != self->getServerPosition()) {
            if (goPath->m_autoWalkContinueEvent)
                goPath->m_autoWalkContinueEvent->cancel();
            const Position dest = result->destination;
            goPath->m_autoWalkContinueEvent =
                tibia::schedule::after([self, dest] { self->autoWalk(dest, true); }, 50);
            return;
        }

        const std::vector<Position> waypoints = result->start.translatedToDirections(result->path);
        if (!waypoints.empty()) {
            const Position& last = waypoints.back();
            stored->setLastAutoWalkPosition(
                (last != stored->autoWalkDestination()) ? last : Position{});
        }

        tibia::game::wireAutoWalk(result->path, result->start);
    }, findMode);

    if (!retry)
        lockWalk();

    return true;
}

bool LocalPlayerGoPath::isWalkLocked()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        return row->isWalkLocked();
    return false;
}

bool LocalPlayerGoPath::isPreWalking()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        return row->isPreWalking();
    return false;
}

bool LocalPlayerGoPath::isAutoWalking()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        return row->isAutoWalking();
    return false;
}

void LocalPlayerGoPath::resetPreWalk()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        row->clearPreWalks();
}

void LocalPlayerGoPath::handleTeleport()
{
    cancelAdjustInvalidPosEvent();
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner)) {
        row->clearAutoWalkDirs();
        row->abortWalkForTeleport();
    } else {
        resetPreWalk();
        if (m_owner.isWalking())
            m_owner.stopWalk();
    }
    m_pendingChunkContinue = {};
    m_owner.goPathSetServerWalk(false);
    tibia::game::notifyCameraMove({});
}

size_t LocalPlayerGoPath::getPreWalkingSize()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        return row->preWalkingSize();
    return 0;
}

void LocalPlayerGoPath::stopAutoWalk(const bool notifyServer)
{
    const bool hadPath = isAutoWalking() || hasAutoWalkDirs() || m_pendingChunkContinue.isValid()
        || (m_autoWalkContinueEvent != nullptr);

    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        row->clearAutoWalk();
    m_knownCompletePath = false;
    m_pendingChunkContinue = {};

    if (m_autoWalkContinueEvent)
        m_autoWalkContinueEvent->cancel();
    m_autoWalkContinueEvent = nullptr;

    if (tibia::input::TPlayerMovementGameActionHandler* movement =
            tibia::input::TPlayerMovementGameActionHandler::instance())
        movement->clearTargetIntent();

    if (notifyServer && hadPath)
        tibia::game::requestStop();
}

bool LocalPlayerGoPath::hasAutoWalkDirs()
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner))
        return row->hasAutoWalkDirs();
    return false;
}

void LocalPlayerGoPath::stopPendingWalk()
{
    stopAutoWalk(false);
    if (m_owner.isWalking() && isPreWalking())
        m_owner.stopWalk();
}

void LocalPlayerGoPath::onMovementTweenTerminated()
{
    m_owner.goPathClearWalkSessionMirror();
    tryBeginNextGoPathStep();
    if (m_pendingChunkContinue.isValid()) {
        const Position dest = m_pendingChunkContinue;
        m_pendingChunkContinue = {};
        autoWalk(dest, true);
    }
}

void LocalPlayerGoPath::onConfirmedPosition(const Position& newPos)
{
    if (const std::shared_ptr<tibia::creatures::TCreature> row = localWalkRow(m_owner)) {
        if (newPos == row->autoWalkDestination())
            stopAutoWalk();
        else if (row->isAutoWalking() && newPos == row->lastAutoWalkPosition()) {
            const Position dest = row->autoWalkDestination();
            row->setLastAutoWalkPosition({});
            if (m_owner.isWalking() || isPreWalking())
                m_pendingChunkContinue = dest;
            else
                autoWalk(dest, true);
        }
    }
}
