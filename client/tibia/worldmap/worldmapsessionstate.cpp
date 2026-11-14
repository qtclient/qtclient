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

#include "worldmap/worldmapsessionstate.h"
#include "effects/effectstorage.h"
#include "tibia/runtime/host.h"

#include "client/animatedtext.h"
#include "client/const.h"
#include <tibia/config/runtimegameconfig.h>
#include "client/item.h"
#include "game/localplayersessionthing.h"
#include "appearances/thing.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"
#include "creatures/creaturestorage.h"
#include "creatures/walkdebug.h"
#include "minimap/waypointcosts.h"

#include <framework/core/asyncdispatcher.h>
#include <tibia/game/gamesession.h>
#include <tibia/schedule/schedule.h>

#include <algorithm>

namespace
{
    // Death-scene freeze keeps tiles/creatures; transient spell visuals may still expire.
    bool freezeBlocksThingMutation(const ThingPtr& thing)
    {
        if (!tibia::game::isSceneFrozen())
            return false;
        // Field effects are appearance-only and expire via TEffectInstance life.
        return !thing;
    }

    void cleanNewSpectators(std::vector<CreaturePtr>& creatures, std::unordered_set<uint32_t>& seenIds, const std::size_t startIndex)
    {
        auto it = creatures.begin() + startIndex;
        while (it != creatures.end()) {
            if (!*it) {
                it = creatures.erase(it);
                continue;
            }
            ++it;
        }
    }
}


void TWorldMapSessionState::ensureMissileFloorsSized()
{
    m_floors.resize(tibia::config::mapMaxZ() + 1);
}

void TWorldMapSessionState::init()
{
    ensureMissileFloorsSized();
    // Requires a live TWorldMapStorage::instance — do not call from storage ctor.
    resetAwareRange();
}

void TWorldMapSessionState::terminate()
{
    clean();
}

void TWorldMapSessionState::notificateKeyRelease(const InputEvent&) const
{
}

void TWorldMapSessionState::notificateCameraMove(const Point&) const
{
    // CIP QSG world emit — no OTC draw-pool repaint.
}

void TWorldMapSessionState::addObserver(MapObserver* observer)
{
    if (!observer)
        return;
    if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end())
        m_observers.push_back(observer);
}

void TWorldMapSessionState::removeObserver(MapObserver* observer)
{
    m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), observer), m_observers.end());
}

void TWorldMapSessionState::setObserver(MapObserver* observer)
{
    m_observers.clear();
    if (observer)
        m_observers.push_back(observer);
}

void TWorldMapSessionState::notificateWindowChanged(const bool fullDescription) const
{
    for (MapObserver* observer : m_observers)
        observer->onWindowChanged(fullDescription);
}

void TWorldMapSessionState::invalidateCoverCachesFrom(const Position& pos)
{
    // isCompletelyCovered walks coveredUp from a field and tests a 2x2 at each
    // level. An opaque change at `pos` only affects tiles that look up through
    // that cell — invalidate those columns, not every tile in the world.
    Position cursor = pos;
    for (;;) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (tibia::worldmap::TWorldMapField* tile = getTile(cursor.translated(dx, dy)))
                    tile->invalidateCoverCache();
            }
        }
        if (!cursor.coveredDown())
            break;
    }
}

void TWorldMapSessionState::invalidateAllCoverCaches()
{
    for (tibia::worldmap::TWorldMapField* tile : tibia::worldmap::TWorldMapStorage::ensureStorage()->spatialFieldList(-1)) {
        if (tile)
            tile->invalidateCoverCache();
    }
    m_coverInvalidateFlood = false;
}

void TWorldMapSessionState::beginCoverProbeBatch()
{
    if (m_coverInvalidateFlood)
        invalidateAllCoverCaches();
    m_coverProbe.clear();
    m_coverProbeActive = true;
}

void TWorldMapSessionState::endCoverProbeBatch()
{
    m_coverProbeActive = false;
    m_coverProbe.clear();
}

namespace {

constexpr uint8_t ProbeSeen = 1;
constexpr uint8_t ProbeEmpty = 2;
constexpr uint8_t ProbeOpaque = 4;
constexpr uint8_t ProbeTopGround = 8;
constexpr uint8_t ProbeLoading = 16;

}

uint8_t TWorldMapSessionState::probeCoverTile(const Position& pos)
{
    if (m_coverProbeActive) {
        const auto it = m_coverProbe.find(pos);
        if (it != m_coverProbe.end())
            return it->second;
    }
    uint8_t flags = ProbeSeen;
    tibia::worldmap::TWorldMapField* tile = getTile(pos);
    if (!tile) {
        flags |= ProbeEmpty;
    } else {
        if (tile->isLoading())
            flags |= ProbeLoading;
        if (tile->isFullyOpaque())
            flags |= ProbeOpaque;
        if (tile->hasTopGround())
            flags |= ProbeTopGround;
    }
    if (m_coverProbeActive)
        m_coverProbe.emplace(pos, flags);
    return flags;
}

void TWorldMapSessionState::notificateTileUpdate(const Position& pos, const ThingPtr& thing, const tibia::client::Operation operation)
{
    if (!pos.isMapPosition())
        return;

    // Creature OPERATION_CLEAN is only a walking-tile draw hint (Creature::updateWalkingTile
    // for the camera follower) — it must not bump cover/item revisions or every step
    // forces a full static remap (~100ms tilesUs). Real field cleans pass thing=nullptr.
    const bool fieldClean = operation == tibia::client::OPERATION_CLEAN && !thing;
    const bool opaqueChurn = thing && thing->isOpaque()
        && (operation == tibia::client::OPERATION_REMOVE || operation == tibia::client::OPERATION_ADD);
    // Per-cell cover cache clear (not a global revision wipe — that cold-started
    // every tile each map-stream packet and dominated tilesUs while walking).
    // When staticDirty is already saturated, coalesce to one FOV wipe on the next
    // cover-probe batch instead of walking N columns during the stream flood.
    if (fieldClean || opaqueChurn) {
        m_coverRevision.fetch_add(1, std::memory_order_relaxed);
        if (m_staticDirty.size() >= MaxStaticDirty) {
            m_coverInvalidateFlood = true;
        } else {
            invalidateCoverCachesFrom(pos);
        }
    }
    if (fieldClean || (thing && thing->isItem()))
        m_itemRevision.fetch_add(1, std::memory_order_relaxed);
    if (operation == tibia::client::OPERATION_CLEAN || (thing && thing->isCreature()))
        m_dynamicRevision.fetch_add(1, std::memory_order_relaxed);

    // Static dirty for the world renderer (independent of the single MapObserver,
    // which the minimap owns). Skip unaware eviction floods and walk hints.
    if (!m_removingUnawareThings
        && (fieldClean
            || (thing && thing->isItem()
                && (operation == tibia::client::OPERATION_ADD || operation == tibia::client::OPERATION_REMOVE))
            || opaqueChurn)) {
        if (m_staticDirty.size() < MaxStaticDirty)
            m_staticDirty.push_back(pos);
    }

    // Walking-tile hints (CLEAN with a creature) and effects are not field changes.
    if (!m_removingUnawareThings
        && (thing ? operation != tibia::client::OPERATION_CLEAN && (thing->isItem() || thing->isCreature()) : true)) {
        if (tibia::worldmap::TWorldMapStorage* storage = tibia::worldmap::TWorldMapStorage::instance())
            storage->notifyCoordinateChanged(pos);
        else
            for (MapObserver* observer : m_observers)
                observer->onTileChanged(pos);
    }
}

size_t TWorldMapSessionState::clearStaticDirtyTiles()
{
    if (m_coverInvalidateFlood)
        invalidateAllCoverCaches();
    const size_t n = m_staticDirty.size();
    m_staticDirty.clear();
    return n;
}

void TWorldMapSessionState::clean()
{
    cleanDynamicThings();

    if (tibia::worldmap::TWorldMapStorage* storage = tibia::worldmap::TWorldMapStorage::instance())
        storage->clearAllFields();
    else
        tibia::worldmap::TWorldMapStorage::ensureStorage()->clearSpatialIndex();
}

void TWorldMapSessionState::cleanDynamicThings()
{
    // Field + storage first (play path). LocalPlayerSessionThing is a Game session Thing, not a Map registry.
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        const QList<tibia::creatures::TCreatureID> ids = storage->creatureIds();
        for (const tibia::creatures::TCreatureID id : ids)
            removeCreatureFromFields(id);
        for (const tibia::creatures::TCreatureID id : ids)
            storage->remove(id);
    }

    if (const LocalPlayerSessionThingPtr localPlayer = tibia::game::localPlayer()) {
        removeThing(localPlayer);
    }

    for (auto& floor : m_floors)

    cleanTexts();

}

bool TWorldMapSessionState::placeAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& pos,
                          const int16_t stackPos)
{
    if (!appearance || appearance->id() == 0 || !pos.isValid())
        return false;
    if (const auto& tile = getOrCreateTile(pos)) {
        tile->addAppearance(appearance, stackPos);
        notificateTileUpdate(pos, nullptr, tibia::client::OPERATION_ADD);
        m_itemRevision.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

const std::vector<tibia::appearances::TAppearanceInstancePtr>& TWorldMapSessionState::getFloorMissiles(const uint8_t z) const
{
    return tibia::effects::TEffectStorage::instance().missilesOn(z);
}

void TWorldMapSessionState::addMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance)
{
    if (dynamic_cast<tibia::appearances::TMissileEffectInstance*>(appearance.get())
        && tibia::effects::TEffectStorage::instance().add(appearance))
        m_dynamicRevision.fetch_add(1, std::memory_order_relaxed);
}

void TWorldMapSessionState::addEffectAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance, const Position& pos)
{
    if (!appearance || appearance->category() != ThingCategoryEffect)
        return;

    auto* effect = dynamic_cast<tibia::appearances::TEffectInstance*>(appearance.get());
    if (!effect)
        return;
    // The effect shows on its field even before the field has content.
    (void)getOrCreateTile(pos);
    if (tibia::effects::TEffectStorage::instance().add(appearance)) {
        notificateTileUpdate(pos, nullptr, tibia::client::OPERATION_ADD);
        m_dynamicRevision.fetch_add(1, std::memory_order_relaxed);
    }
}

void TWorldMapSessionState::addStaticText(const StaticTextPtr& txt, const Position& pos) {
    if (tibia::game::isSceneFrozen() || !tibia::host::app().isDrawingTexts())
        return;

    tibia::schedule::post([=, this] {
        for (const auto& other : m_staticTexts) {
            // try to combine messages
            if (other->getPosition() == pos && other->addMessage(txt->getName(), txt->getMessageMode(), txt->getFirstMessage())) {
                return;
            }
        }

        txt->setPosition(pos);
        m_staticTexts.emplace_back(txt);
    });
}

void TWorldMapSessionState::addAnimatedText(const AnimatedTextPtr& txt, const Position& pos) {
    if (tibia::game::isSceneFrozen() || !tibia::host::app().isDrawingTexts())
        return;

    tibia::schedule::post([=, this] {
        // this code will stack animated texts of the same color
        AnimatedTextPtr prevAnimatedText;

        bool merged = false;
        for (const auto& other : m_animatedTexts) {
            if (other->getPosition() == pos) {
                prevAnimatedText = other;
                if (other->merge(txt)) {
                    merged = true;
                    break;
                }
            }
        }

        if (!merged) {
            if (prevAnimatedText) {
                Point offset = prevAnimatedText->getOffset();
                if (const float t = prevAnimatedText->getTimer().ticksElapsed();
                    t < tibia::config::animatedTextDuration() / 4.0) { // didnt move 12 pixels
                    const int32_t y = 12 - 48 * t / static_cast<float>(tibia::config::animatedTextDuration());
                    offset += Point(0, y);
                }
                offset.y = std::min<int32_t>(offset.y, 12);
                txt->setOffset(offset);
            }
            m_animatedTexts.emplace_back(txt);
        }

        txt->setPosition(pos);
        txt->onAppear();
    });
}

ThingPtr TWorldMapSessionState::getThing(const Position& pos, const int16_t stackPos)
{
    if (const auto& tile = getTile(pos))
        return tile->getThing(stackPos);

    return nullptr;
}

bool TWorldMapSessionState::removeThing(const ThingPtr& thing)
{
    if (freezeBlocksThingMutation(thing))
        return true;

    if (!thing)
        return false;

    if (const auto& tile = thing->getTile()) {
        if (tile->removeThing(thing)) {
            notificateTileUpdate(thing->getServerPosition(), thing, tibia::client::OPERATION_REMOVE);
            return true;
        }
    }

    return false;
}

bool TWorldMapSessionState::removeThingByPos(const Position& pos, const int16_t stackPos)
{
    if (const auto& tile = getTile(pos)) {
        if (const ThingPtr thing = tile->getThing(stackPos))
            return removeThing(thing);
        return tile->removeThingAtStackPos(stackPos);
    }

    return false;
}

bool TWorldMapSessionState::removeStaticText(const StaticTextPtr& txt) {
    const auto it = std::ranges::find(m_staticTexts, txt);
    if (it == m_staticTexts.end())
        return false;

    m_staticTexts.erase(it);
    return true;
}

bool TWorldMapSessionState::removeAnimatedText(const AnimatedTextPtr& txt) {
    const auto it = std::ranges::find(m_animatedTexts, txt);
    if (it == m_animatedTexts.end())
        return false;

    m_animatedTexts.erase(it);
    return true;
}

StaticTextPtr TWorldMapSessionState::getStaticText(const Position& pos) const
{
    for (const auto& staticText : m_staticTexts) {
        // try to combine messages
        if (staticText->getPosition() == pos)
            return staticText;
    }

    return nullptr;
}

tibia::worldmap::TWorldMapField* TWorldMapSessionState::createTile(const Position& pos)
{
    if (!pos.isMapPosition())
        return nullptr;
    return tibia::worldmap::TWorldMapStorage::ensureStorage()->getOrCreateField(pos);
}

tibia::worldmap::TWorldMapField* TWorldMapSessionState::getOrCreateTile(const Position& pos)
{
    if (!pos.isMapPosition())
        return nullptr;
    return tibia::worldmap::TWorldMapStorage::ensureStorage()->getOrCreateField(pos);
}

tibia::worldmap::TWorldMapField* TWorldMapSessionState::fieldInBlock(const Position& pos)
{
    return tibia::worldmap::TWorldMapStorage::ensureStorage()->fieldInSpatialIndex(pos);
}

tibia::worldmap::TWorldMapField* TWorldMapSessionState::getTile(const Position& pos)
{
    if (!pos.isMapPosition())
        return nullptr;
    tibia::worldmap::TWorldMapStorage* storage = tibia::worldmap::TWorldMapStorage::ensureStorage();
    if (tibia::worldmap::TWorldMapField* field = storage->fieldAt(pos))
        return field;
    // Orphan spatial slot (pre-rebind) — pathfind iterates storage spatial blocks.
    return fieldInBlock(pos);
}

tibia::worldmap::TWorldMapField* TWorldMapSessionState::fieldAt(const Position& pos)
{
    if (!pos.isMapPosition())
        return nullptr;
    return tibia::worldmap::TWorldMapStorage::ensureStorage()->fieldAt(pos);
}

tibia::worldmap::TWorldMapField* TWorldMapSessionState::getOrCreateField(const Position& pos)
{
    return getOrCreateTile(pos);
}

void TWorldMapSessionState::setFieldInBlock(const Position& pos, tibia::worldmap::TWorldMapField* field)
{
    tibia::worldmap::TWorldMapStorage::ensureStorage()->bindFieldToSpatialIndex(pos, field);
}

WorldMapFieldList TWorldMapSessionState::getTiles(const int8_t floor/* = -1*/)
{
    return tibia::worldmap::TWorldMapStorage::ensureStorage()->spatialFieldList(floor);
}

void TWorldMapSessionState::cleanTile(const Position& pos)
{
    if (tibia::game::isSceneFrozen())
        return;

    if (!pos.isMapPosition())
        return;

    // CIP has one field identity per coordinate. Unaware removal nulls the block
    // slot but may leave the TWorldMapStorage entry — clearField must scrub that
    // too or the next map description inserts onto the stale stack (duplicate
    // tables/mailboxes after leave/reenter).
    tibia::worldmap::TWorldMapField* storageField = nullptr;
    if (tibia::worldmap::TWorldMapStorage* storage = tibia::worldmap::TWorldMapStorage::instance())
        storageField = storage->fieldAt(pos);

    tibia::worldmap::TWorldMapField* blockField = fieldInBlock(pos);

    tibia::worldmap::TWorldMapField* tile = blockField ? blockField : storageField;
    if (tile) {
        tile->clean();
        if (tile->canErase()) {
            if (tibia::worldmap::TWorldMapStorage* storage = tibia::worldmap::TWorldMapStorage::instance()) {
                storage->removeFieldFromSpatialIndex(pos);
                storage->removeField(pos);
            } else {
                tibia::worldmap::TWorldMapStorage::ensureStorage()->removeFieldFromSpatialIndex(pos);
            }
        } else if (!blockField && storageField) {
            // Keep drawable field addressable via Map blocks after unaware scrub.
            setFieldInBlock(pos, storageField);
        }

        notificateTileUpdate(pos, nullptr, tibia::client::OPERATION_CLEAN);
    } else if (tibia::worldmap::TWorldMapStorage* storage = tibia::worldmap::TWorldMapStorage::instance()) {
        storage->notifyCoordinateChanged(pos);
    } else {
        for (MapObserver* observer : m_observers)
            observer->onTileChanged(pos);
    }

    tibia::schedule::post([=, this] {
        for (auto itt = m_staticTexts.begin(); itt != m_staticTexts.end();) {
            const auto& staticText = *itt;
            if (staticText->getPosition() == pos && staticText->getMessageMode() == tibia::client::MessageNone)
                itt = m_staticTexts.erase(itt);
            else
                ++itt;
        }
    });
}


void TWorldMapSessionState::beginGhostMode(const float) {}
void TWorldMapSessionState::endGhostMode() {}

stdext::map<Position, uint16_t, Position::Hasher> TWorldMapSessionState::findItemsById(const uint16_t clientId, const uint32_t max)
{
    stdext::map<Position, uint16_t, Position::Hasher> ret;
    uint32_t count = 0;
    for (tibia::worldmap::TWorldMapField* tile :
         tibia::worldmap::TWorldMapStorage::ensureStorage()->spatialFieldList(-1)) {
        if (unlikely(!tile || tile->isEmpty()))
            continue;
        for (const auto& hit : tile->itemHits()) {
            if (hit.typeId != clientId)
                continue;
            ret.emplace(tile->getPosition(), hit.typeId);
            if (++count >= max)
                return ret;
        }
    }

    return ret;
}

std::vector<TWorldMapSessionState::TItemHit> TWorldMapSessionState::findItemsInRange(const Position& center, const int rangeX, const int rangeY,
                                                 const std::vector<uint16_t>& clientIds)
{
    std::vector<TItemHit> ret;
    if (clientIds.empty() || !center.isValid())
        return ret;

    for (int x = center.x - rangeX; x <= center.x + rangeX; ++x) {
        for (int y = center.y - rangeY; y <= center.y + rangeY; ++y) {
            tibia::worldmap::TWorldMapField* tile = getTile(Position(x, y, center.z));
            if (!tile)
                continue;
            for (const auto& hit : tile->itemHits()) {
                if (std::find(clientIds.begin(), clientIds.end(), hit.typeId) == clientIds.end())
                    continue;
                ret.push_back({ tile->getPosition(), hit.typeId, hit.stackPos });
            }
        }
    }

    return ret;
}

CreaturePtr TWorldMapSessionState::getCreatureById(const uint32_t id)
{
    return tibia::game::creatureThingById(id);
}

bool TWorldMapSessionState::removeCreatureFromFields(const uint32_t creatureId, const Position& fieldPos,
                                                     const bool fireLocalDisappear)
{
    if (creatureId == 0)
        return false;

    const LocalPlayerSessionThingPtr local = tibia::game::localPlayer();
    const bool isLocal = local && creatureId == local->getId();
    bool removed = false;
    const auto removeAt = [&](const Position& at) {
        tibia::worldmap::TWorldMapField* tile = at.isValid() ? getTile(at) : nullptr;
        if (!tile || !tile->removeCreatureById(creatureId))
            return;
        removed = true;
        if (isLocal)
            local->setMarked(Color::white);
        notificateTileUpdate(at, isLocal ? local : nullptr, tibia::client::OPERATION_REMOVE);
    };

    // The field the message names, else the field the creature stands on.
    removeAt(fieldPos);
    if (!removed) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const auto row = storage->creatureAt(creatureId); row && row->position() != fieldPos)
                removeAt(row->position());
        }
    }

    if (removed && fireLocalDisappear && isLocal)
        local->onDisappear();

    // Fallback: LocalPlayerSessionThing session Thing may still sit on a tile.
    if (!removed && isLocal)
        return removeThing(local);
    return removed;
}

bool TWorldMapSessionState::placeCreatureId(const uint32_t creatureId, const Position& pos, const int16_t stackPos)
{
    if (creatureId == 0 || !pos.isValid())
        return false;
    tibia::worldmap::TWorldMapField* tile = getOrCreateTile(pos);
    if (!tile)
        return false;

    // A field stack holds creature ids, so a creature re-placed by a map
    // description leaves the field it was standing on.
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(creatureId); row && row->position().isValid() && row->position() != pos)
            removeCreatureFromFields(creatureId, row->position(), false);
    }

    tile->addCreatureId(creatureId, stackPos);
    const LocalPlayerSessionThingPtr local = tibia::game::localPlayer();
    if (local && creatureId == local->getId()) {
        const int placedStack = tile->creatureStackPos(creatureId);
        local->setPosition(pos, placedStack >= 0 ? placedStack : stackPos);
        local->onAppear();
        notificateTileUpdate(pos, local, tibia::client::OPERATION_ADD);
    } else {
        notificateTileUpdate(pos, nullptr, tibia::client::OPERATION_ADD);
    }
    return true;
}

bool TWorldMapSessionState::applyCreatureMove(const uint32_t creatureId, const Position& oldPos, const int oldStack,
                                              const Position& newPos, const bool byId)
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!creatureId || !newPos.isValid() || !storage)
        return false;
    const auto row = storage->getOrCreate(creatureId);
    if (!row)
        return false;
    tibia::worldmap::TWorldMapStorage* map = tibia::worldmap::TWorldMapStorage::ensureStorage();

    // The position form names the field and slot the creature leaves; the id
    // form moves it from where it stands.
    const Position from = byId ? row->position() : oldPos;
    int elevationFrom = 0;
    int elevationTo = 0;
    if (byId) {
        placeCreatureId(creatureId, newPos, -1);
        elevationFrom = map->elevationAt(from);
        elevationTo = map->elevationAt(newPos);
    } else {
        tibia::worldmap::TWorldMapField* fromTile = getTile(oldPos);
        if (!fromTile || fromTile->stackCreatureId(oldStack) != creatureId)
            return false;
        elevationFrom = map->elevationAt(oldPos);
        elevationTo = map->elevationAt(newPos);
        fromTile->removeThingAtStackPos(oldStack);
        const LocalPlayerSessionThingPtr local = tibia::game::localPlayer();
        notificateTileUpdate(oldPos, local && creatureId == local->getId() ? local : nullptr,
                             tibia::client::OPERATION_REMOVE);
        placeCreatureId(creatureId, newPos, -1);
        const auto look = tibia::creatures::lookDirectionAfter(
            static_cast<tibia::creatures::ECreatureLookDirection>(row->patternX()),
            tibia::creatures::directionBetween(tibia::worldmap::TWorldMapCoordinate(from),
                                               tibia::worldmap::TWorldMapCoordinate(newPos)));
        row->setDirection(static_cast<uint8_t>(look));
    }
    map->moveCreature(*row, from, newPos, elevationFrom, elevationTo);
    return true;
}

void TWorldMapSessionState::removeUnawareThings()
{
    // remove creatures from tiles that we are not aware of anymore (storage positions)
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        for (const tibia::creatures::TCreatureID id : storage->creatureIds()) {
            const auto row = storage->creatureAt(id);
            if (!row || !row->position().isValid())
                continue;
            if (!isAwareOfPosition(row->position()))
                removeCreatureFromFields(id);
        }
    }
    if (const LocalPlayerSessionThingPtr localPlayer = tibia::game::localPlayer()) {
        if (localPlayer->getId() != 0 && !isAwareOfPosition(localPlayer->getPosition()))
            removeThing(localPlayer);
    }

    tibia::schedule::post([=, this] {
        // remove static texts from tiles that we are not aware anymore
        for (auto it = m_staticTexts.begin(); it != m_staticTexts.end();) {
            const auto& staticText = *it;
            if (staticText->getMessageMode() == tibia::client::MessageNone && !isAwareOfPosition(staticText->getPosition()))
                it = m_staticTexts.erase(it);
            else
                ++it;
        }
    });

    if (!tibia::game::cipFeature(tibia::client::GameKeepUnawareTiles)) {
        m_removingUnawareThings = true;
        const auto& customAwareRange = tibia::game::cipFeature(tibia::client::GameMapCache) ? AwareRange{
            .left = static_cast<uint8_t>(m_awareRange.left * 4),
            .top = static_cast<uint8_t>(m_awareRange.top * 4),
            .right = static_cast<uint8_t>(m_awareRange.right * 4),
            .bottom = static_cast<uint8_t>(m_awareRange.bottom * 4),
        } : m_awareRange;

        // Prefer instance — ensureStorage during storage construction re-enters ctor.
        if (tibia::worldmap::TWorldMapStorage* storage = tibia::worldmap::TWorldMapStorage::instance()) {
            storage->pruneUnawareSpatialFields(
                [&](const Position& pos) { return isAwareOfPosition(pos, customAwareRange); },
                [&](const Position& pos) { notificateTileUpdate(pos, nullptr, tibia::client::OPERATION_CLEAN); });
        }
        m_removingUnawareThings = false;
    }
}

void TWorldMapSessionState::setCentralPosition(const Position& centralPosition)
{
    if (tibia::game::isSceneFrozen())
        return;

    if (m_centralPosition == centralPosition)
        return;

    m_centralPosition = centralPosition;

    removeUnawareThings();

    // this fixes local player position when the local player is removed from the map,
    // the local player is removed from the map when there are too many creatures on his tile,
    // so there is no enough stackpos to the server send him
    tibia::schedule::post([this] {
        const auto& localPlayer = tibia::game::localPlayer();
        if (!localPlayer || localPlayer->getPosition() == m_centralPosition)
            return;

        if (const auto& tile = localPlayer->getTile()) {
            if (tile->hasThing(localPlayer))
                return;
        }

        const auto& oldPos = localPlayer->getPosition();
        const auto& pos = m_centralPosition;
        if (oldPos != pos) {
            if (!localPlayer->isRemoved())
                localPlayer->onDisappear();
            localPlayer->setPosition(pos);
            localPlayer->onAppear();
            g_logger.debug("Forced player position update");
        }
    });
}

void TWorldMapSessionState::setLight(const Light& light)
{
    m_light = light;
}

std::vector<CreaturePtr> TWorldMapSessionState::getSpectatorsInRangeEx(const Position& centerPos, const bool multiFloor, const int32_t minXRange, const int32_t maxXRange, const int32_t minYRange, const int32_t maxYRange)
{
    // CreaturePtr spectators are LocalPlayerSessionThing-only; gameplay uses TCreatureStorage::spectatorsNear.
    std::vector<CreaturePtr> creatures;
    creatures.reserve(1);
    std::unordered_set<uint32_t> seenIds;
    seenIds.reserve(1);

    uint8_t minZRange = 0;
    uint8_t maxZRange = 0;

    if (multiFloor) {
        minZRange = centerPos.z - getFirstAwareFloor();
        maxZRange = getLastAwareFloor() - centerPos.z;
    }

    const int startZ = centerPos.z - minZRange;
    const int endZ = centerPos.z + maxZRange;
    const int startY = centerPos.y - minYRange;
    const int endY = centerPos.y + maxYRange;
    const int startX = centerPos.x - minXRange;
    const int endX = centerPos.x + maxXRange;

    const auto appendSpectatorsFromLayer = [&](const int z) {
        for (int y = startY; y <= endY; ++y) {
            for (int x = startX; x <= endX; ++x) {
                const auto tile = getTile(Position(x, y, z));
                if (!tile || !tile->hasCreatures()) {
                    continue;
                }

                const auto sizeBeforeAppend = creatures.size();
                tile->appendSpectators(creatures);
                cleanNewSpectators(creatures, seenIds, sizeBeforeAppend);
            }
        }
    };

    for (int z = startZ; z <= endZ; ++z) {
        appendSpectatorsFromLayer(z);
    }
    return creatures;
}

bool TWorldMapSessionState::isLookPossible(const Position& pos)
{
    const auto& tile = getTile(pos);
    return tile && tile->isLookPossible();
}

bool TWorldMapSessionState::isCovered(const Position& pos, bool& isLoading, const uint8_t firstFloor)
{
    // check for tiles on top of the postion
    Position tilePos = pos;
    while (tilePos.coveredUp() && tilePos.z >= firstFloor) {
        const uint8_t here = probeCoverTile(tilePos);
        if (here & ProbeLoading) {
            isLoading = true;
            return false;
        }
        if (here & ProbeOpaque)
            return true;

        if (probeCoverTile(tilePos.translated(1, 1)) & ProbeTopGround)
            return true;
    }

    return false;
}

bool TWorldMapSessionState::isCompletelyCovered(const Position& pos, bool& isLoading, const uint8_t firstFloor)
{
    tibia::worldmap::TWorldMapField* checkTile = getTile(pos);
    const bool single = !checkTile || checkTile->isSingleDimension();
    Position tilePos = pos;
    while (tilePos.coveredUp() && tilePos.z >= firstFloor) {
        bool covered = true;
        bool done = false;

        // Check is Top Ground
        for (auto x = -1; ++x < 2 && !done;) {
            for (auto y = -1; ++y < 2 && !done;) {
                const uint8_t flags = probeCoverTile(tilePos.translated(x, y));
                if ((flags & ProbeEmpty) || !(flags & ProbeTopGround)) {
                    covered = false;
                    done = true;
                } else if (x == 1 && y == 1 && single) {
                    done = true;
                }
            }
        }

        if (covered)
            return true;

        covered = true;
        done = false;

        // check in 2x2 range tiles that has no transparent pixels
        for (auto x = -1; ++x < 2 && !done;) {
            for (auto y = -1; ++y < 2 && !done;) {
                const uint8_t flags = probeCoverTile(tilePos.translated(-x, -y));
                if (flags & ProbeLoading) {
                    isLoading = true;
                    return false;
                }

                if ((flags & ProbeEmpty) || !(flags & ProbeOpaque)) {
                    covered = false;
                    done = true;
                } else if (x == 0 && y == 0 && single) {
                    done = true;
                }
            }
        }

        if (covered)
            return true;
    }
    return false;
}

bool TWorldMapSessionState::isAwareOfPosition(const Position& pos, const AwareRange& awareRange) const
{
    if ((pos.z < getFirstAwareFloor() || pos.z > getLastAwareFloor()) && awareRange == m_awareRange)
        return false;

    Position groundedPos = pos;
    while (groundedPos.z != m_centralPosition.z) {
        if (groundedPos.z > m_centralPosition.z) {
            if (groundedPos.x == UINT16_MAX || groundedPos.y == UINT16_MAX) // When pos == 65535,65535,15 we cant go up to 65536,65536,14
                break;
            groundedPos.coveredUp();
        } else {
            if (groundedPos.x == 0 || groundedPos.y == 0) // When pos == 0,0,0 we cant go down to -1,-1,1
                break;
            groundedPos.coveredDown();
        }
    }

    return m_centralPosition.isInRange(groundedPos, awareRange.left,
                                       awareRange.right,
                                       awareRange.top,
                                       awareRange.bottom);
}

void TWorldMapSessionState::setAwareRange(const AwareRange& range)
{
    m_awareRange = range;
    removeUnawareThings();
}

void TWorldMapSessionState::resetAwareRange() {
    setAwareRange({
        .left = static_cast<uint8_t>(tibia::config::mapViewPortWidth()) ,
        .top = static_cast<uint8_t>(tibia::config::mapViewPortHeight()),
        .right = static_cast<uint8_t>(tibia::config::mapViewPortWidth() + 1),
        .bottom = static_cast<uint8_t>(tibia::config::mapViewPortHeight() + 1)
    });
}

uint8_t TWorldMapSessionState::getFirstAwareFloor() const
{
    if (m_centralPosition.z <= tibia::config::mapSeaFloor())
        return 0;

    return m_centralPosition.z - tibia::config::mapAwareUndergroundFloorRange();
}

uint8_t TWorldMapSessionState::getLastAwareFloor() const
{
    if (m_centralPosition.z <= tibia::config::mapSeaFloor())
        return tibia::config::mapSeaFloor();

    return std::min<uint8_t >(m_centralPosition.z + tibia::config::mapAwareUndergroundFloorRange(), tibia::config::mapMaxZ());
}

namespace {

std::shared_ptr<tibia::minimap::TWaypointCosts> resolveSoftCosts(
    const std::shared_ptr<tibia::minimap::TWaypointCosts>& soft,
    const TWorldMapSessionState::WaypointCostFactory& factory)
{
    if (soft)
        return soft;
    if (factory)
        return factory();
    // Disk-only Indexed8 (AppData/minimap) when the CIP session factory is cleared.
    return tibia::minimap::snapshotWaypointCosts(nullptr);
}

} // namespace

std::tuple<std::vector<tibia::client::Direction>, tibia::client::PathFindResult> TWorldMapSessionState::findPath(const Position& startPos, const Position& goalPos, const int maxComplexity, const int flags)
{
    // pathfinding using dijkstra search algorithm

    struct SNode
    {
        SNode(const Position& pos) :
            pos(pos) {}
        float cost{ 0 };
        float totalCost{ 0 };
        Position pos;
        SNode* prev{ nullptr };
        tibia::client::Direction dir{ tibia::client::InvalidDirection };
    };

    struct LessNode
    {
        bool operator()(const std::pair<SNode*, float> a, const std::pair<SNode*, float> b) const
        {
            return b.second < a.second;
        }
    };

    std::tuple<std::vector<tibia::client::Direction>, tibia::client::PathFindResult> ret;
    std::vector<tibia::client::Direction>& dirs = std::get<0>(ret);
    tibia::client::PathFindResult& result = std::get<1>(ret);

    result = tibia::client::PathFindResultNoWay;

    if (startPos == goalPos) {
        result = tibia::client::PathFindResultSamePosition;
        return ret;
    }

    if (startPos.z != goalPos.z) {
        result = tibia::client::PathFindResultImpossible;
        return ret;
    }

    const auto softCosts = resolveSoftCosts(nullptr, m_waypointCostFactory);

    // check the goal pos is walkable
    if (isAwareOfPosition(goalPos)) {
        const auto& goalTile = getTile(goalPos);
        if (!goalTile || (!goalTile->isWalkable(flags & tibia::client::PathFindIgnoreCreatures))) {
            return ret;
        }
    } else {
        const uint8_t goalCost = softCosts->costAt(goalPos.x, goalPos.y, goalPos.z);
        if (goalCost == tibia::minimap::TWaypointCosts::BlockedCost)
            return ret;
        if (goalCost == tibia::minimap::TWaypointCosts::UnknownCost
            && !(flags & tibia::client::PathFindAllowNotSeenTiles))
            return ret;
    }

    stdext::map<Position, SNode*, Position::Hasher> nodes;
    std::priority_queue<std::pair<SNode*, float>, std::vector<std::pair<SNode*, float>>, LessNode> searchList;

    auto* currentNode = new SNode(startPos);
    currentNode->pos = startPos;
    nodes[startPos] = currentNode;
    SNode* foundNode = nullptr;
    while (currentNode) {
        if (static_cast<int>(nodes.size()) > maxComplexity) {
            result = tibia::client::PathFindResultTooFar;
            break;
        }

        // path found
        if (currentNode->pos == goalPos && (!foundNode || currentNode->cost < foundNode->cost))
            foundNode = currentNode;

        // cost too high
        if (foundNode && currentNode->totalCost >= foundNode->cost)
            break;

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0)
                    continue;
                if ((flags & tibia::client::PathFindOrthogonalOnly) && i != 0 && j != 0)
                    continue;

                bool wasSeen = false;
                bool hasCreature = false;
                bool isNotWalkable = true;
                bool isNotPathable = true;
                int speed = 100;

                Position neighborPos = currentNode->pos.translated(i, j);
                if (neighborPos.x < 0 || neighborPos.y < 0) continue;
                if (isAwareOfPosition(neighborPos)) {
                    wasSeen = true;
                    if (const auto& tile = getTile(neighborPos)) {
                        hasCreature = tile->hasCreatures() && (!(flags & tibia::client::PathFindIgnoreCreatures));
                        isNotWalkable = !tile->isWalkable(flags & tibia::client::PathFindIgnoreCreatures);
                        isNotPathable = !tile->isPathable();
                        speed = tile->getGroundSpeed();
                    }
                } else {
                    // Indexed8: 0..250 walkable, 0xfe fog, 0xff blocked (no separate pathable bit).
                    const uint8_t wc = softCosts->costAt(neighborPos.x, neighborPos.y, neighborPos.z);
                    const bool softFog = wc == tibia::minimap::TWaypointCosts::UnknownCost;
                    const bool blocked = wc == tibia::minimap::TWaypointCosts::BlockedCost;
                    wasSeen = !softFog;
                    isNotWalkable = blocked || softFog;
                    isNotPathable = blocked;
                    if (isNotWalkable || isNotPathable)
                        wasSeen = true;
                    speed = wc <= tibia::minimap::TWaypointCosts::MaxCost
                        ? static_cast<int>(std::max<uint8_t>(1, wc))
                        : 100;
                }

                float walkFactor = 0;
                if (neighborPos != goalPos) {
                    if (!(flags & tibia::client::PathFindAllowNotSeenTiles) && !wasSeen)
                        continue;
                    if (wasSeen) {
                        if (!(flags & tibia::client::PathFindAllowCreatures) && hasCreature)
                            continue;
                        if (!(flags & tibia::client::PathFindAllowNonPathable) && isNotPathable)
                            continue;
                        if (!(flags & tibia::client::PathFindAllowNonWalkable) && isNotWalkable)
                            continue;
                    }
                } else {
                    if (!(flags & tibia::client::PathFindAllowNotSeenTiles) && !wasSeen)
                        continue;
                    if (wasSeen) {
                        if (!(flags & tibia::client::PathFindAllowNonWalkable) && isNotWalkable)
                            continue;
                    }
                }

                const tibia::client::Direction walkDir = currentNode->pos.getDirectionFromPosition(neighborPos);
                if (walkDir >= tibia::client::NorthEast)
                    walkFactor += 3.0f;
                else
                    walkFactor += 1.0f;

                const float cost = currentNode->cost + (speed * walkFactor) / 100.0f;

                SNode* neighborNode;
                if (!nodes.contains(neighborPos)) {
                    neighborNode = new SNode(neighborPos);
                    nodes[neighborPos] = neighborNode;
                } else {
                    neighborNode = nodes[neighborPos];
                    if (neighborNode->cost <= cost)
                        continue;
                }

                neighborNode->prev = currentNode;
                neighborNode->cost = cost;
                neighborNode->totalCost = neighborNode->cost + neighborPos.distance(goalPos);
                neighborNode->dir = walkDir;
                searchList.emplace(neighborNode, neighborNode->totalCost);
            }
        }

        if (!searchList.empty()) {
            currentNode = searchList.top().first;
            searchList.pop();
        } else
            currentNode = nullptr;
    }

    if (foundNode) {
        currentNode = foundNode;
        while (currentNode) {
            dirs.push_back(currentNode->dir);
            currentNode = currentNode->prev;
        }
        dirs.pop_back();
        std::ranges::reverse(dirs);
        result = tibia::client::PathFindResultOk;
    }

    for (const auto& it : nodes)
        delete it.second;

    return ret;
}

void TWorldMapSessionState::resetLastCamera() const
{
}

PathFindResult_ptr TWorldMapSessionState::newFindPath(const Position& start, const Position& goal,
                                    const std::shared_ptr<std::list<Node*>>& visibleNodes,
                                    const std::shared_ptr<tibia::minimap::TWaypointCosts>& softCosts,
                                    const tibia::client::AutoWalkPathMode pathMode,
                                    const int maxComplexity)
{
    auto ret = std::make_shared<PathFindResult>();
    ret->start = start;
    ret->destination = goal;

    if (start == goal) {
        ret->status = tibia::client::PathFindResultSamePosition;
        return ret;
    }

    if (goal.z != start.z) {
        return ret;
    }

    // CIP : |dx|/|dy| ≤ 0x6e (110) or fail.
    constexpr int kCipPathWindow = 0x6e;
    if (std::abs(goal.x - start.x) > kCipPathWindow || std::abs(goal.y - start.y) > kCipPathWindow) {
        return ret;
    }

    const bool allowUnknownSoft = pathMode != tibia::client::AutoWalkPathMinimap;
    // CIP / : 0xff neighbors impassable; 0xfe fog walkable when
    // allowUnknownSoft (raw cost 254). Travel may still *goal* on Indexed8 0xff hub/climb stands.
    const auto costs = resolveSoftCosts(softCosts, m_waypointCostFactory);

    // Goal walkability: live aware tile first, else Indexed8 costs (CIP client minimap).
    // Cyclopedia/Travel: path-unknown (0xfe) goals OK; Travel also CipSoft-blocked (0xff) goals.
    // Live fire / splash / parcels: hard-block when aware (no soft-blocker bypass).
    if (isAwareOfPosition(goal)) {
        const auto& goalTile = getTile(goal);
        if (!goalTile || !goalTile->isWalkable())
            return ret;
    } else {
        const uint8_t goalCost = costs->costAt(goal.x, goal.y, goal.z);
        if (goalCost <= tibia::minimap::TWaypointCosts::MaxCost) {
            // walkable Indexed8
        } else if (goalCost == tibia::minimap::TWaypointCosts::UnknownCost && allowUnknownSoft) {
            // Cyclopedia/Travel fog goal (CIP treats 0xfe as cost 254)
        } else if (goalCost == tibia::minimap::TWaypointCosts::BlockedCost
            && pathMode == tibia::client::AutoWalkPathTravel) {
            // Travel CipSoft-blocked hub/climb stand
        } else {
            return ret;
        }
    }

    // CIP : min tile cost over ±0x6e window (admissible heuristic scale).
    uint32_t minCost = 0xffffffffu;
    for (int dy = -kCipPathWindow; dy <= kCipPathWindow; ++dy) {
        for (int dx = -kCipPathWindow; dx <= kCipPathWindow; ++dx) {
            const int wx = start.x + dx;
            const int wy = start.y + dy;
            if (wx < 0 || wy < 0)
                continue;
            const uint8_t wc = costs->costAt(wx, wy, start.z);
            if (wc < minCost)
                minCost = wc;
        }
    }
    if (minCost == 0xffffffffu || minCost == 0)
        minCost = 1;

    // CIP h = (manhattan − 1) × minCost + tileCost (0 at goal).
    const auto cipHeuristic = [minCost](const Position& pos, const Position& dest, const float tileCost) -> float {
        const int manh = std::abs(pos.x - dest.x) + std::abs(pos.y - dest.y);
        if (manh <= 0)
            return 0.f;
        return static_cast<float>((manh - 1) * static_cast<int>(minCost)) + tileCost;
    };

    const auto tileCostAt = [&](const Position& pos) -> float {
        const uint8_t wc = costs->costAt(pos.x, pos.y, pos.z);
        if (wc <= tibia::minimap::TWaypointCosts::MaxCost)
            return static_cast<float>(std::max<uint8_t>(1, wc));
        // CIP Indexed8 0xfe stays 254 (stores the raw byte).
        if (wc == tibia::minimap::TWaypointCosts::UnknownCost)
            return static_cast<float>(tibia::minimap::TWaypointCosts::UnknownCost);
        return 1.f;
    };

    struct LessNode
    {
        bool operator()(const Node* a, const Node* b) const
        {
            return b->totalCost < a->totalCost;
        }
    };

    stdext::map<Position, Node*, Position::Hasher> nodes;
    std::priority_queue<Node*, std::vector<Node*>, LessNode> searchList;

    if (visibleNodes) {
        for (auto& node : *visibleNodes) {
            // cost < 0: impassable aware-tile sentinel from buildTravelVisibleNodes.
            // Emplacing a real Node here let A* expand through blocked live tiles and
            // the server answered "Sorry, not possible."
            if (node->cost < 0.f) {
                nodes.emplace(node->pos, nullptr);
                delete node;
            } else {
                node->pathCost = 1e9f;
                node->totalCost = 1e9f;
                nodes.emplace(node->pos, node);
            }
        }
    }

    const float startTileCost = tileCostAt(start);
    auto* initNode = new Node{ .cost = startTileCost,
                               .totalCost = cipHeuristic(start, goal, startTileCost),
                               .pathCost = 0.f,
                               .pos = start,
                               .prev = nullptr,
                               .distance = 0,
                               .unseen = 0 };
    nodes[start] = initNode;
    searchList.push(initNode);

    int limit = maxComplexity > 0 ? maxComplexity : 1;
    const int complexityBudget = limit;

    const Node* dstNode = nullptr;
    while (!searchList.empty() && --limit) {
        Node* node = searchList.top();
        searchList.pop();
        if (node->pos == goal) {
            dstNode = node;
            break;
        }
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0)
                    continue;
                Position neighbor = node->pos.translated(i, j);
                if (neighbor.x < 0 || neighbor.y < 0)
                    continue;
                // Stay inside CIP ±0x6e window relative to start.
                if (std::abs(neighbor.x - start.x) > kCipPathWindow
                    || std::abs(neighbor.y - start.y) > kCipPathWindow)
                    continue;
                auto it = nodes.find(neighbor);
                if (it == nodes.end()) {
                    // pathfinder avoid set: treat as impassable (CIP minimap path avoid).
                    if (isPathAvoided(neighbor) && neighbor != goal) {
                        it = nodes.emplace(neighbor, nullptr).first;
                    } else {
                        // CIP TWaypointCostMap: 0..250 walkable, 0xfe unknown, 0xff blocked.
                        // Cyclopedia/Travel: 0xfe expensive (CIP cost 254). 0xff impassable.
                        // Live aware tiles still win via visibleNodes.
                        const uint8_t wc = costs->costAt(neighbor.x, neighbor.y, neighbor.z);
                        const bool softFog = wc == tibia::minimap::TWaypointCosts::UnknownCost;
                        const bool passFog = (softFog && allowUnknownSoft) || neighbor == goal;
                        if (wc > tibia::minimap::TWaypointCosts::MaxCost && !passFog) {
                            it = nodes.emplace(neighbor, nullptr).first;
                        } else {
                            const float speed = tileCostAt(neighbor);
                            it = nodes.emplace(neighbor,
                                               new Node{ .cost = speed,
                                                         .totalCost = 1e9f,
                                                         .pathCost = 1e9f,
                                                         .pos = neighbor,
                                                         .prev = node,
                                                         .distance = node->distance + 1,
                                                         .unseen = 0 })
                                     .first;
                        }
                    }
                }
                if (!it->second) // no way
                    continue;

                if (it->second->unseen > 50)
                    continue;

                // CIP : leave current tile —
                // ortho g' = g + tileCost[current]
                // diag g' = g + tileCost[current] + 2×tileCost[current] = g + 3×tileCost
                const bool ortho = (i == 0 || j == 0);
                const float stepCost = ortho ? node->cost : (node->cost * 3.f);
                const float g = node->pathCost + stepCost;
                if (g >= it->second->pathCost)
                    continue;

                it->second->pathCost = g;
                it->second->totalCost = g + cipHeuristic(neighbor, goal, it->second->cost);
                it->second->prev = node;
                if (it->second->unseen)
                    it->second->unseen = node->unseen + 1;
                it->second->distance = node->distance + 1;
                searchList.push(it->second);
            }
        }
    }

    if (dstNode) {
        while (dstNode && dstNode->prev) {
            // Legacy fog unseen-prefix truncate (Indexed8 soft paths keep unseen=0).
            if (dstNode->unseen) {
                ret->path.clear();
            } else {
                ret->path.push_back(dstNode->prev->pos.getDirectionFromPosition(dstNode->pos));
            }
            dstNode = dstNode->prev;
        }
        std::reverse(ret->path.begin(), ret->path.end());
        ret->status = tibia::client::PathFindResultOk;
    }
    ret->complexity = complexityBudget - limit;

    for (const auto& node : nodes) {
        delete node.second;
    }

    return ret;
}

void TWorldMapSessionState::setPathAvoid(const Position& pos, const bool avoid)
{
    if (!pos.isValid())
        return;
    if (avoid)
        m_pathAvoids.insert(pos);
    else
        m_pathAvoids.erase(pos);
}

void TWorldMapSessionState::clearPathAvoids() { m_pathAvoids.clear(); }

bool TWorldMapSessionState::isPathAvoided(const Position& pos) const { return m_pathAvoids.contains(pos); }

void TWorldMapSessionState::setWaypointCostFactory(WaypointCostFactory factory) { m_waypointCostFactory = std::move(factory); }

void TWorldMapSessionState::clearWaypointCostFactory() { m_waypointCostFactory = {}; }

namespace {

std::shared_ptr<std::list<Node*>> buildTravelVisibleNodes(const Position& start, const Position& goal,
 const tibia::client::AutoWalkPathMode /*pathMode*/)
{
    // CIP auto-path: aware tiles use live walkable/pathable (fire, splash, parcels
    // block when in view). No soft-blocker bypass — path fails → SnapBack avoid /
    // TravelRouter replan or stop.
    auto visibleNodes = std::make_shared<std::list<Node*>>();
    for (const auto& tile : tibia::worldmap::fieldsOnFloor(start.z)) {
        if (tile->getPosition() == start)
            continue;
        const bool isNotWalkable = !tile->isWalkable(false);
        const bool isNotPathable = !tile->isPathable();
        const float baseSpeed = static_cast<float>(tile->getGroundSpeed());
        if ((isNotWalkable || isNotPathable) && tile->getPosition() != goal) {
            // cost < 0 → newFindPath stores nullptr (must not expand).
            visibleNodes->push_back(new Node{ .cost = -1.f, .totalCost = 0, .pathCost = 0, .pos = tile->getPosition(),
                                              .prev = nullptr, .distance = 0, .unseen = 0 });
        } else {
            visibleNodes->push_back(new Node{ .cost = baseSpeed, .totalCost = 1e9f, .pathCost = 1e9f,
                                              .pos = tile->getPosition(), .prev = nullptr, .distance = 0, .unseen = 0 });
        }
    }
    return visibleNodes;
}

} // namespace

void TWorldMapSessionState::findPathAsync(const Position& start, const Position& goal, const std::function<void(PathFindResult_ptr)>&
                        callback, const tibia::client::AutoWalkPathMode pathMode)
{
    const auto visibleNodes = buildTravelVisibleNodes(start, goal, pathMode);

    // CIP costs: snapshot on the GUI thread (tile images), path on a worker.
    const std::shared_ptr<tibia::minimap::TWaypointCosts> softCosts =
        m_waypointCostFactory ? m_waypointCostFactory() : nullptr;

    g_asyncDispatcher->detach_task([=, this] {
        const auto ret = newFindPath(start, goal, visibleNodes, softCosts, pathMode);
        tibia::schedule::post(std::bind(callback, ret));
    });
}

bool TWorldMapSessionState::canTravelPath(const Position& start, const Position& goal, const int maxComplexity)
{
    if (!start.isValid() || !goal.isValid())
        return false;
    if (start == goal)
        return true;
    if (start.z != goal.z)
        return false;
    const auto pathMode = tibia::client::AutoWalkPathTravel;
    const auto visibleNodes = buildTravelVisibleNodes(start, goal, pathMode);
    const std::shared_ptr<tibia::minimap::TWaypointCosts> softCosts =
        m_waypointCostFactory ? m_waypointCostFactory() : nullptr;
    const auto ret = newFindPath(start, goal, visibleNodes, softCosts, pathMode, maxComplexity);
    return ret && (ret->status == tibia::client::PathFindResultOk || ret->status == tibia::client::PathFindResultSamePosition);
}

int TWorldMapSessionState::getMinimapColor(const Position& pos)
{
    int color = 0;
    if (tibia::worldmap::TWorldMapField* tile = getTile(pos)) {
        color = tile->getMinimapColorByte();
    }
    if (color == 0) {
        const auto softCosts = resolveSoftCosts(nullptr, m_waypointCostFactory);
        color = softCosts->colorIndexAt(pos.x, pos.y, pos.z);
    }
    return color;
}

bool TWorldMapSessionState::isSightClear(const Position& fromPos, const Position& toPos)
{
    if (fromPos == toPos) {
        return true;
    }

    Position start(fromPos.z > toPos.z ? toPos : fromPos);
    const Position destination(fromPos.z > toPos.z ? fromPos : toPos);

    const int8_t mx = start.x < destination.x ? 1 : start.x == destination.x ? 0 : -1;
    const int8_t my = start.y < destination.y ? 1 : start.y == destination.y ? 0 : -1;

    const int32_t A = destination.y - start.y;
    const int32_t B = start.x - destination.x;
    const int32_t C = -(A * destination.x + B * destination.y);

    while (start.x != destination.x || start.y != destination.y) {
        const int32_t move_hor = std::abs(A * (start.x + mx) + B * (start.y) + C);
        const int32_t move_ver = std::abs(A * (start.x) + B * (start.y + my) + C);
        const int32_t move_cross = std::abs(A * (start.x + mx) + B * (start.y + my) + C);

        if (start.y != destination.y && (start.x == destination.x || move_hor > move_ver || move_hor > move_cross)) {
            start.y += my;
        }

        if (start.x != destination.x && (start.y == destination.y || move_ver > move_hor || move_ver > move_cross)) {
            start.x += mx;
        }

        const auto tile = getTile(Position(start.x, start.y, start.z));
        if (tile && !tile->isLookPossible()) {
            return false;
        }
    }

    while (start.z != destination.z) {
        const auto tile = getTile(Position(start.x, start.y, start.z));
        if (tile && tile->getThingCount() > 0) {
            return false;
        }
        start.z++;
    }

    return true;
}

std::map<std::string, std::tuple<int, int, int, std::string>> TWorldMapSessionState::findEveryPath(const Position& start, int maxDistance, const std::map<std::string, std::string>& params)
{
    // using Dijkstra's algorithm
    struct LessNode
    {
        bool operator()(Node* a, Node* b) const
        {
            return b->totalCost < a->totalCost;
        }
    };

    std::map<std::string, std::string>::const_iterator it;
    it = params.find("ignoreLastCreature");
    bool ignoreLastCreature = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("ignoreCreatures");
    bool ignoreCreatures = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("ignoreNonPathable");
    bool ignoreNonPathable = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("ignoreNonWalkable");
    bool ignoreNonWalkable = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("ignoreStairs");
    bool ignoreStairs = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("ignoreCost");
    bool ignoreCost = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("allowUnseen");
    bool allowUnseen = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("allowOnlyVisibleTiles");
    bool allowOnlyVisibleTiles = it != params.end() && it->second != "0" && it->second != "";
    it = params.find("marginMin");
    bool hasMargin = it != params.end();
    it = params.find("marginMax");
    hasMargin = hasMargin || (it != params.end());

    Position destPos;
    it = params.find("destination");
    if (it != params.end()) {
        std::vector<int32_t> pos = stdext::split<int32_t>(it->second, ",");
        if (pos.size() == 3) {
            destPos = Position(pos[0], pos[1], pos[2]);
        }
    }

    Position maxDistanceFromPos;
    int maxDistanceFrom = 0;
    it = params.find("maxDistanceFrom");
    if (it != params.end()) {
        std::vector<int32_t> pos = stdext::split<int32_t>(it->second, ",");
        if (pos.size() == 4) {
            maxDistanceFromPos = Position(pos[0], pos[1], pos[2]);
            maxDistanceFrom = pos[3];
        }
    }

    std::map<std::string, std::tuple<int, int, int, std::string>> ret;
    std::unordered_map<Position, Node*, Position::Hasher> nodes;
    std::priority_queue<Node*, std::vector<Node*>, LessNode> searchList;

    const auto softCosts = resolveSoftCosts(nullptr, m_waypointCostFactory);

    Node* initNode = new Node{ 1.f, 0.f, 0.f, start, nullptr, 0, 0 };
    nodes[start] = initNode;
    searchList.push(initNode);

    while (!searchList.empty()) {
        Node* node = searchList.top();
        searchList.pop();
        ret[node->pos.toString()] = std::make_tuple(node->totalCost, node->distance,
                                                    node->prev ? node->prev->pos.getDirectionFromPosition(node->pos) : -1,
                                                    node->prev ? node->prev->pos.toString() : "");
        if (node->pos == destPos) {
            if (hasMargin) {
                maxDistance = std::min<int>(node->distance + 4, maxDistance);
            } else {
                break;
            }
        }
        if (node->distance >= maxDistance)
            continue;
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0)
                    continue;
                Position neighbor = node->pos.translated(i, j);
                if (neighbor.x < 0 || neighbor.y < 0) continue;
                auto it = nodes.find(neighbor);
                if (it == nodes.end()) {
                    bool wasSeen = false;
                    bool hasCreature = false;
                    bool isNotWalkable = true;
                    bool isNotPathable = true;
                    int mapColor = 0;
                    int speed = 1000;
                    if (isAwareOfPosition(neighbor)) {
                        if (tibia::worldmap::TWorldMapField* tile = getTile(neighbor)) {
                            wasSeen = true;
                            hasCreature = tile->hasBlockingCreature();
                            isNotWalkable = !tile->isWalkable(true);
                            isNotPathable = !tile->isPathable();
                            mapColor = tile->getMinimapColorByte();
                            speed = tile->getGroundSpeed();
                        }
                    } else if (!allowOnlyVisibleTiles) {
                        const uint8_t wc = softCosts->costAt(neighbor.x, neighbor.y, neighbor.z);
                        const bool softFog = wc == tibia::minimap::TWaypointCosts::UnknownCost;
                        const bool blocked = wc == tibia::minimap::TWaypointCosts::BlockedCost;
                        wasSeen = !softFog;
                        isNotWalkable = blocked || softFog;
                        isNotPathable = blocked;
                        mapColor = softCosts->colorIndexAt(neighbor.x, neighbor.y, neighbor.z);
                        if (isNotWalkable || isNotPathable)
                            wasSeen = true;
                        speed = wc <= tibia::minimap::TWaypointCosts::MaxCost
                            ? static_cast<int>(std::max<uint8_t>(1, wc))
                            : static_cast<int>(tibia::minimap::TWaypointCosts::UnknownCost);
                    }
                    bool hasStairs = isNotPathable && mapColor >= 210 && mapColor <= 213;
                    bool hasReachedMaxDistance = maxDistanceFrom && maxDistanceFromPos.isValid() && maxDistanceFromPos.distance(neighbor) > maxDistanceFrom;
                    if ((!wasSeen && !allowUnseen) || (hasStairs && !ignoreStairs && neighbor != destPos) ||
                        (isNotPathable && !ignoreNonPathable && neighbor != destPos) || (isNotWalkable && !ignoreNonWalkable) ||
                        hasReachedMaxDistance) {
                        it = nodes.emplace(neighbor, nullptr).first;
                    } else if ((hasCreature && !ignoreCreatures)) {
                        it = nodes.emplace(neighbor, nullptr).first;
                        if (ignoreLastCreature) {
                            ret[neighbor.toString()] = std::make_tuple(node->totalCost + 100, node->distance + 1,
                                                                       node->pos.getDirectionFromPosition(neighbor),
                                                                       node->pos.toString());
                        }
                    } else {
                        it = nodes.emplace(neighbor,
                                           new Node{ static_cast<float>(speed), 1e9f, 1e9f, neighbor, node,
                                                     node->distance + 1, wasSeen ? 0 : 1 })
                                 .first;
                    }
                }

                if (!it->second) {
                    continue;
                }

                // CIP : ortho 1× / diag 3× leaving tile cost.
                const bool ortho = (i == 0 || j == 0);
                float stepCost = ortho ? node->cost : (node->cost * 3.f);
                if (ignoreCost)
                    stepCost = 1.f;
                const float g = node->pathCost + stepCost;
                if (g >= it->second->pathCost)
                    continue;
                it->second->pathCost = g;
                it->second->totalCost = g;
                it->second->prev = node;
                if (it->second->unseen)
                    it->second->unseen = node->unseen + 1;
                it->second->distance = node->distance + 1;
                searchList.push(it->second);
            }
        }
    }

    for (auto& node : nodes) {
        if (node.second)
            delete node.second;
    }

    return ret;
}

std::vector<CreaturePtr> TWorldMapSessionState::getSpectatorsByPattern(const Position& centerPos, const std::string& pattern, tibia::client::Direction direction)
{
    std::vector<bool> finalPattern(pattern.size(), false);
    std::vector<CreaturePtr> creatures;
    int width = 0, height = 0, lineLength = 0, p = 0;
    for (auto& c : pattern) {
        lineLength += 1;
        if (c == '0' || c == '-') {
            p += 1;
        } else if (c == '1' || c == '+') {
            finalPattern[p++] = true;
        } else if (c == 'N' || c == 'n') {
            finalPattern[p++] = direction == tibia::client::North;
        } else if (c == 'E' || c == 'e') {
            finalPattern[p++] = direction == tibia::client::East;
        } else if (c == 'W' || c == 'w') {
            finalPattern[p++] = direction == tibia::client::West;
        } else if (c == 'S' || c == 's') {
            finalPattern[p++] = direction == tibia::client::South;
        } else {
            lineLength -= 1;
            if (lineLength > 1) {
                if (width == 0)
                    width = lineLength;
                if (width != lineLength) {
                    g_logger.error("Invalid pattern for getSpectatorsByPattern: {}", pattern);
                    return creatures;
                }
                height += 1;
                lineLength = 0;
            }
        }
    }
    if (lineLength > 0) {
        if (width == 0)
            width = lineLength;
        if (width != lineLength) {
            g_logger.error("Invalid pattern for getSpectatorsByPattern: {}", pattern);
            return creatures;
        }
        height += 1;
    }
    if (width % 2 != 1 || height % 2 != 1) {
        g_logger.error("Invalid pattern for getSpectatorsByPattern, width and height should be odd (height: %i width: %i)", height, width);
        return creatures;
    }

    p = 0;
    std::unordered_set<uint32_t> seenIds;
    seenIds.reserve(1);
    for (int y = centerPos.y - height / 2, endy = centerPos.y + height / 2; y <= endy; ++y) {
        for (int x = centerPos.x - width / 2, endx = centerPos.x + width / 2; x <= endx; ++x) {
            const auto enabled = finalPattern[p];
            ++p;
            if (!enabled) {
                continue;
            }

            const auto tile = getTile(Position(x, y, centerPos.z));
            if (!tile || !tile->hasCreatures()) {
                continue;
            }

            const auto sizeBeforeAppend = creatures.size();
            tile->appendSpectators(creatures);
            cleanNewSpectators(creatures, seenIds, sizeBeforeAppend);
        }
    }
    return creatures;
}

