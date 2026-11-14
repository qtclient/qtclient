/*
 * Copyright (c) 2024-2026 qtclient contributors
 * (see tile.cpp header — ported to TWorldMapField)
 */

#include "worldmapfield.h"
#include "effects/effectstorage.h"
#include "tibia/runtime/host.h"

#include "appearances/appearanceinstance.h"
#include "creatures/creaturestorage.h"
#include "client/const.h"
#include "tibia/assets/thingtype.h"
#include "tibia/assets/things.h"
#include <tibia/config/runtimegameconfig.h>
#include "client/item.h"
#include <tibia/game/mapsession.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/appearances/thing.h>
#include "framework/core/clock.h"
#include <tibia/schedule/schedule.h>
#include "framework/core/scheduledevent.h"
#include <tibia/game/gamesession.h>

namespace tibia::worldmap {

CreaturePtr TWorldMapField::stackCreature(const TFieldStackEntry& entry) const
{
    (void)entry;
    return nullptr;
}

int TWorldMapField::stackPriorityFor(const TFieldStackEntry& entry) const
{
    // Creature stack slots are always CREATURE priority — no Map CreaturePtr resolve.
    if (entry.creatureId != 0)
        return CREATURE;
    ThingType* type = entry.appearance ? entry.appearance->thingType() : nullptr;
    if (!type)
        return 0;
    if (tibia::game::clientRevision() <= 800 && type->isSplash())
        return GROUND;
    if (type->isGround())
        return GROUND;
    if (type->isGroundBorder())
        return GROUND_BORDER;
    if (type->isOnBottom())
        return ON_BOTTOM;
    if (type->isOnTop())
        return ON_TOP;
    return COMMON_ITEMS;
}

int TWorldMapField::stackPriorityAt(const int stackIndex) const
{
    if (stackIndex < 0 || stackIndex >= static_cast<int>(m_stack.size()))
        return 0;
    return stackPriorityFor(m_stack[stackIndex]);
}

uint16_t TWorldMapField::itemClientIdAt(const int stackPos) const
{
    if (const appearances::TAppearanceInstancePtr appearance = appearanceAt(stackPos))
        return appearance->id();
    return 0;
}

bool TWorldMapField::stackEntryMatchesThing(const TFieldStackEntry& entry, const ThingPtr& thing) const
{
    if (!thing)
        return false;
    if (thing->isCreature())
        return entry.creatureId == thing->getId();
    if (entry.creatureId != 0)
        return false;
    if (auto* object = dynamic_cast<appearances::TObjectAppearanceInstance*>(entry.appearance.get()))
        return object->matchesItemIdentity(*thing);
    return entry.appearance && entry.appearance->id() == thing->getId();
}

void TWorldMapField::rebuildAppearanceCache()
{
    m_appearanceCache.clear();
    m_appearanceCache.reserve(m_stack.size());
    for (const auto& entry : m_stack)
        m_appearanceCache.push_back(entry.appearance);
}

bool TWorldMapField::hasThing(const ThingPtr& thing)
{
    if (!thing)
        return false;
    for (const auto& entry : m_stack) {
        if (stackEntryMatchesThing(entry, thing))
            return true;
    }
    return false;
}

void TWorldMapField::recalculateThingFlag()
{
    m_thingTypeFlag = 0;
    m_elevation = 0;
    m_drawElevation = 0;
    rebuildCreatureRange();
    for (const auto& entry : m_stack)
        setStackEntryFlag(entry);
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        for (const uint32_t walkingId : m_walkingCreatureIds) {
            if (walkingId == 0)
                continue;
            if (const auto row = storage->creatureAt(walkingId)) {
                ThingType* outfitType = row->outfitAppearance() ? row->outfitAppearance()->thingType() : nullptr;
                applyCreatureStackFlags(*row, outfitType);
            }
        }
    }
    for (const auto& appearance : effectAppearances()) {
        if (appearance && appearance->thingType() && appearance->thingType()->hasLight())
            m_thingTypeFlag |= HAS_LIGHT;
    }
}

appearances::TAppearanceInstancePtr TWorldMapField::appearanceAt(const int stackPos) const
{
    if (stackPos >= 0 && stackPos < static_cast<int>(m_stack.size()))
        return m_stack[stackPos].appearance;
    return {};
}

uint32_t TWorldMapField::stackCreatureId(const int stackPos) const
{
    if (stackPos >= 0 && stackPos < static_cast<int>(m_stack.size()))
        return m_stack[stackPos].creatureId;
    return 0;
}

void TWorldMapField::syncCreatureStackAppearance(const uint32_t creatureId)
{
    if (creatureId == 0)
        return;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return;
    const auto row = storage->creatureAt(creatureId);
    if (!row)
        return;

    const appearances::TAppearanceInstancePtr appearance =
        appearances::TAppearanceInstanceFactory::fromStoredCreature(*row);
    if (!appearance)
        return;

    bool changed = false;
    for (auto& entry : m_stack) {
        if (entry.creatureId != creatureId)
            continue;
        entry.appearance = appearance;
        changed = true;
    }
    if (!changed)
        return;
    rebuildAppearanceCache();
    notifyAppearanceInstancesChanged();
}

TWorldMapField::TWorldMapField(QObject* parent)
    : cip::tibia::worldmap::TWorldMapField(parent)
{
}

void updateElevation(const ThingPtr& thing, uint8_t& drawElevation) {
    if (thing->hasElevation())
        drawElevation = std::min<uint8_t>(drawElevation + thing->getElevation(), tibia::config::tileMaxElevation());
}

namespace
{
    ThingType* entryThingType(const appearances::TAppearanceInstancePtr& appearance)
    {
        return appearance ? appearance->thingType() : nullptr;
    }

    CreaturePtr entryCreature(const uint32_t creatureId)
    {
        (void)creatureId;
        return nullptr;
    }

    const appearances::TObjectAppearanceInstance* entryObject(const appearances::TAppearanceInstancePtr& appearance)
    {
        return dynamic_cast<const appearances::TObjectAppearanceInstance*>(appearance.get());
    }

    bool typeIsCommon(const ThingType* type)
    {
        return type && !type->isGround() && !type->isGroundBorder() && !type->isOnTop() && !type->isOnBottom();
    }

    bool entryDrawable(const appearances::TAppearanceInstancePtr& appearance)
    {
        const auto* object = entryObject(appearance);
        return !object || object->drawable();
    }

    // Things drawn in scope stand on the floor: they stamp the height maps (shadows, light rays).
    struct ShadowCasterScope
    {
        explicit ShadowCasterScope(const bool) {}
        ~ShadowCasterScope() = default;
        ShadowCasterScope(const ShadowCasterScope&) = delete;
        ShadowCasterScope& operator=(const ShadowCasterScope&) = delete;
    };

    // Things drawn in scope stand on elevated ones below them, `lift` sprite px up.
    struct ShadowLiftScope
    {
        explicit ShadowLiftScope(const uint8_t) {}
        ~ShadowLiftScope() = default;
        ShadowLiftScope(const ShadowLiftScope&) = delete;
        ShadowLiftScope& operator=(const ShadowLiftScope&) = delete;
    };
}

void drawThing(const ThingPtr& thing, const Point& dest, const int flags, uint8_t& drawElevation, LightSink* lightView = nullptr,
               const bool castsShadow = true)
{
    (void)thing; (void)dest; (void)flags; (void)drawElevation; (void)lightView; (void)castsShadow;
}


void TWorldMapField::clean()
{
    // CIP tile refresh replaces the whole field stack. Do not run LocalPlayerSessionThing
    // onDisappear/onAppear here — that cancelled walk tweens and could leave
    // item appearances behind if removeThing short-circuited the erase loop.
    m_highlightThingStackPos = -1;
    m_stack.clear();
    rebuildAppearanceCache();

    m_thingTypeFlag = 0;

    m_firstCreatureIndex = -1;
    m_lastCreatureIndex = -1;

    notifyAppearanceInstancesChanged();
}

void TWorldMapField::addWalkingCreature(const uint32_t creatureId)
{
    if (!creatureId)
        return;
    // One walking slot per id — duplicate pushes drew the same creature twice
    // when covered-field updates raced a failed remove.
    if (std::ranges::find(m_walkingCreatureIds, creatureId) != m_walkingCreatureIds.end())
        return;
    m_walkingCreatureIds.push_back(creatureId);
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        if (const auto row = storage->creatureAt(creatureId)) {
            ThingType* outfitType = row->outfitAppearance() ? row->outfitAppearance()->thingType() : nullptr;
            applyCreatureStackFlags(*row, outfitType);
        }
    }
}

void TWorldMapField::removeWalkingCreature(const uint32_t creatureId)
{
    if (!creatureId)
        return;
    const auto erased = std::erase(m_walkingCreatureIds, creatureId);
    if (erased == 0)
        return;
    recalculateThingFlag();
}

void TWorldMapField::updateThingStackPos() {
    // Map Creature keep-alive retired — stack rows are appearance + creatureId only.
}

const std::vector<appearances::TAppearanceInstancePtr>& TWorldMapField::effectAppearances() const
{
    return effects::TEffectStorage::instance().effectsAt(getPosition());
}

bool TWorldMapField::hasEffect() const
{
    return effects::TEffectStorage::instance().hasEffectsAt(getPosition());
}

void TWorldMapField::effectsChanged()
{
    recalculateThingFlag();
    notifyAppearanceInstancesChanged();
}

void TWorldMapField::addAppearance(const appearances::TAppearanceInstancePtr& appearance, int stackPos)
{
    if (!appearance)
        return;

    const uint8_t size = static_cast<uint8_t>(m_stack.size());
    ThingType* type = appearance->thingType();
    const int priority = [&] {
        if (!type)
            return COMMON_ITEMS;
        if (tibia::game::clientRevision() <= 800 && type->isSplash())
            return GROUND;
        if (type->isGround())
            return GROUND;
        if (type->isGroundBorder())
            return GROUND_BORDER;
        if (type->isOnBottom())
            return ON_BOTTOM;
        if (type->isOnTop())
            return ON_TOP;
        return COMMON_ITEMS;
    }();

    if (stackPos < 0 || stackPos == 255) {
        bool append = (priority <= 3);
        if (tibia::game::clientRevision() >= 854 && priority == 4)
            append = !append;
        for (stackPos = 0; stackPos < size; ++stackPos) {
            const int otherPriority = stackPriorityAt(stackPos);
            if ((append && otherPriority > priority) || (!append && otherPriority >= priority))
                break;
        }
    } else if (stackPos > static_cast<int>(size)) {
        stackPos = size;
    }

    markHighlightedThing(Color::white);

    // CIP TPatternObjectAppearanceInstance : patterns from field coordinate.
    if (const auto patterned =
            std::dynamic_pointer_cast<appearances::TPatternObjectAppearanceInstance>(appearance))
        patterned->applyWorldMapCoordinate(getPosition());

    TFieldStackEntry entry;
    entry.appearance = appearance;
    m_stack.insert(m_stack.begin() + stackPos, std::move(entry));
    rebuildAppearanceCache();

    // Non-creature insert: shift creature range indexes like updateCreatureRangeForInsert.
    if (m_firstCreatureIndex != -1 && stackPos <= m_firstCreatureIndex)
        ++m_firstCreatureIndex;
    if (m_lastCreatureIndex != -1 && stackPos <= m_lastCreatureIndex)
        ++m_lastCreatureIndex;

    setStackEntryFlag(m_stack[stackPos]);

    if (static_cast<uint8_t>(m_stack.size()) > tibia::config::tileMaxThings())
        removeThingAtStackPos(tibia::config::tileMaxThings());

    updateThingStackPos();
    checkForDetachableThing();
    notifyAppearanceInstancesChanged();
}

void TWorldMapField::addCreatureId(const uint32_t creatureId, int stackPos)
{
    if (creatureId == 0)
        return;

    // CIP field stack holds at most one slot per creature id — re-place replaces
    // the existing entry (MoveCreature after a partial remove must not trail).
    const auto setStoredPosition = [this](const uint32_t id) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const auto row = storage->getOrCreate(id))
                row->setPosition(getPosition());
        }
    };
    if (const int existing = creatureStackPos(creatureId); existing >= 0) {
        if (stackPos < 0 || stackPos == 255 || stackPos == existing) {
            syncCreatureStackAppearance(creatureId);
            setStoredPosition(creatureId);
            return;
        }
        removeCreatureById(creatureId);
    }

    const uint8_t size = static_cast<uint8_t>(m_stack.size());
    if (stackPos < 0 || stackPos == 255) {
        const int priority = CREATURE;
        bool append = (priority <= 3);
        if (tibia::game::clientRevision() >= 854 && priority == 4)
            append = !append;
        for (stackPos = 0; stackPos < size; ++stackPos) {
            const int otherPriority = stackPriorityAt(stackPos);
            if ((append && otherPriority > priority) || (!append && otherPriority >= priority))
                break;
        }
    } else if (stackPos > static_cast<int>(size)) {
        stackPos = size;
    }

    markHighlightedThing(Color::white);

    TFieldStackEntry entry;
    entry.creatureId = creatureId;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        const auto row = storage->getOrCreate(creatureId);
        row->setPosition(getPosition());
        entry.appearance = appearances::TAppearanceInstanceFactory::fromStoredCreature(*row);
    }
    m_stack.insert(m_stack.begin() + stackPos, std::move(entry));
    rebuildAppearanceCache();

    // Range insert: treat as creature without ThingPtr.
    if (m_firstCreatureIndex != -1 && stackPos <= m_firstCreatureIndex)
        ++m_firstCreatureIndex;
    if (m_lastCreatureIndex != -1 && stackPos <= m_lastCreatureIndex)
        ++m_lastCreatureIndex;
    if (m_firstCreatureIndex == -1 || stackPos < m_firstCreatureIndex)
        m_firstCreatureIndex = static_cast<int16_t>(stackPos);
    if (stackPos > m_lastCreatureIndex)
        m_lastCreatureIndex = static_cast<int16_t>(stackPos);

    setStackEntryFlag(m_stack[stackPos]);

    if (static_cast<uint8_t>(m_stack.size()) > tibia::config::tileMaxThings()) {
        const int overflowPos = tibia::config::tileMaxThings();
        if (m_stack[overflowPos].creatureId != 0)
            removeCreatureById(m_stack[overflowPos].creatureId);
        else
            removeThingAtStackPos(overflowPos);
    }

    updateThingStackPos();
    checkForDetachableThing();
    notifyAppearanceInstancesChanged();
}

// TODO: Need refactoring
bool TWorldMapField::removeThing(const ThingPtr thing)
{
    if (!thing) return false;

    const auto it = std::ranges::find_if(m_stack, [&](const TFieldStackEntry& entry) {
        return stackEntryMatchesThing(entry, thing);
    });
    if (it == m_stack.end())
        return false;

    markHighlightedThing(Color::white);

    const bool wasCreature = it->creatureId != 0;
    if (wasCreature)
        thing->m_stackPos = -1;

    m_stack.erase(it);
    rebuildAppearanceCache();

    m_highlightThingStackPos = -1;

    recalculateThingFlag();

    updateThingStackPos();
    checkForDetachableThing();

    if (wasCreature) {
        thing->setMarked(Color::white);
        thing->onDisappear();
    }

    notifyAppearanceInstancesChanged();
    return true;
}

bool TWorldMapField::removeThingAtStackPos(const int stackPos)
{
    if (stackPos < 0 || stackPos >= static_cast<int>(m_stack.size()))
        return false;
    if (const uint32_t creatureId = m_stack[stackPos].creatureId) {
        // Prefer id path so Map keep-alive Creature is optional.
        if (removeCreatureById(creatureId))
            return true;
        return false;
    }
    markHighlightedThing(Color::white);
    m_stack.erase(m_stack.begin() + stackPos);
    rebuildAppearanceCache();
    m_highlightThingStackPos = -1;
    recalculateThingFlag();
    updateThingStackPos();
    checkForDetachableThing();
    notifyAppearanceInstancesChanged();
    return true;
}

bool TWorldMapField::removeCreatureById(const uint32_t creatureId)
{
    if (creatureId == 0)
        return false;
    const auto it = std::ranges::find_if(m_stack, [creatureId](const TFieldStackEntry& entry) {
        return entry.creatureId == creatureId;
    });
    if (it == m_stack.end())
        return false;

    markHighlightedThing(Color::white);
    m_stack.erase(it);
    rebuildAppearanceCache();
    m_highlightThingStackPos = -1;
    recalculateThingFlag();
    updateThingStackPos();
    checkForDetachableThing();
    notifyAppearanceInstancesChanged();
    return true;
}

ThingPtr TWorldMapField::getThing(const int stackPos) const
{
    (void)stackPos;
    return nullptr;
}

void TWorldMapField::updateCreatureRangeForInsert(const int16_t stackPos, const ThingPtr& thing)
{
    if (m_firstCreatureIndex != -1 && stackPos <= m_firstCreatureIndex) {
        ++m_firstCreatureIndex;
    }

    if (m_lastCreatureIndex != -1 && stackPos <= m_lastCreatureIndex) {
        ++m_lastCreatureIndex;
    }

    if (!thing->isCreature()) {
        return;
    }

    if (m_firstCreatureIndex == -1 || stackPos < m_firstCreatureIndex) {
        m_firstCreatureIndex = stackPos;
    }

    if (stackPos > m_lastCreatureIndex) {
        m_lastCreatureIndex = stackPos;
    }
}

void TWorldMapField::rebuildCreatureRange()
{
    m_firstCreatureIndex = -1;
    m_lastCreatureIndex = -1;

    const auto count = static_cast<int32_t>(m_stack.size());
    for (int32_t i = 0; i < count; ++i) {
        if (m_stack[i].creatureId == 0)
            continue;

        if (m_firstCreatureIndex == -1)
            m_firstCreatureIndex = static_cast<int16_t>(i);

        m_lastCreatureIndex = static_cast<int16_t>(i);
    }
}

void TWorldMapField::appendSpectators(std::vector<CreaturePtr>& out) const
{
    // CreaturePtr spectators are LocalPlayerSessionThing-only; prefer getStoredCreatures / stackCreatureEntries.
    if (!hasCreatures() || m_lastCreatureIndex == -1)
        return;

    const auto size = static_cast<int32_t>(m_stack.size());
    const auto beginOffset = size - 1 - static_cast<int32_t>(m_lastCreatureIndex);
    const auto endOffset = size - static_cast<int32_t>(m_firstCreatureIndex);

    auto it = m_stack.rbegin() + beginOffset;
    const auto end = m_stack.rbegin() + endOffset;

    for (; it != end; ++it) {
        if (it->creatureId == 0)
            continue;
        if (const CreaturePtr creature = tibia::game::creatureThingById(it->creatureId))
            out.emplace_back(creature);
    }
}

std::vector<CreaturePtr> TWorldMapField::getCreatures() const
{
    std::vector<CreaturePtr> creatures;
    appendSpectators(creatures);
    std::ranges::reverse(creatures);
    return creatures;
}

std::vector<std::shared_ptr<tibia::creatures::TCreature>> TWorldMapField::getStoredCreatures() const
{
    std::vector<std::shared_ptr<tibia::creatures::TCreature>> creatures;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        for (const auto& entry : m_stack) {
            if (entry.creatureId == 0)
                continue;
            if (std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(entry.creatureId))
                creatures.push_back(std::move(row));
        }
    }
    return creatures;
}

std::vector<TStackCreatureEntry> TWorldMapField::stackCreatureEntries() const
{
    std::vector<TStackCreatureEntry> entries;
    entries.reserve(static_cast<size_t>(m_lastCreatureIndex >= 0 ? m_lastCreatureIndex - m_firstCreatureIndex + 1 : 0));
    for (int i = 0; i < static_cast<int>(m_stack.size()); ++i) {
        if (m_stack[i].creatureId == 0)
            continue;
        entries.push_back({ i, m_stack[i].creatureId, m_stack[i].appearance });
    }
    return entries;
}

int TWorldMapField::getThingStackPos(const ThingPtr& thing)
{
    for (int stackpos = -1, s = static_cast<int>(m_stack.size()); ++stackpos < s;) {
        if (stackEntryMatchesThing(m_stack[stackpos], thing))
            return stackpos;
    }

    return -1;
}

int TWorldMapField::creatureStackPos(const uint32_t creatureId) const
{
    if (creatureId == 0)
        return -1;
    for (int stackpos = 0, s = static_cast<int>(m_stack.size()); stackpos < s; ++stackpos) {
        if (m_stack[stackpos].creatureId == creatureId)
            return stackpos;
    }
    return -1;
}

bool TWorldMapField::hasGround() const
{
    const ThingType* type = m_stack.empty() ? nullptr : entryThingType(m_stack[0].appearance);
    return (type && type->isSingleGround()) || (m_thingTypeFlag & HAS_GROUND_BORDER);
}

bool TWorldMapField::hasTopGround(const bool ignoreBorder) const
{
    const ThingType* type = m_stack.empty() ? nullptr : entryThingType(m_stack[0].appearance);
    return (type && type->isTopGround()) || (!ignoreBorder && (m_thingTypeFlag & HAS_TOP_GROUND_BORDER));
}
bool TWorldMapField::hasFloorChange() const
{
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        if (const ThingType* type = entryThingType(entry.appearance); type && type->hasFloorChange())
            return true;
    }
    return false;
}
TWorldMapField::TFieldItemHit TWorldMapField::topUseHit() const
{
    auto hitAt = [this](const int stackPos) -> TFieldItemHit {
        if (stackPos < 0 || stackPos >= static_cast<int>(m_stack.size()))
            return {};
        if (m_stack[stackPos].creatureId != 0 || !m_stack[stackPos].appearance)
            return {};
        return { m_stack[stackPos].appearance->id(), stackPos };
    };

    if (isEmpty())
        return {};

    for (int i = 0; i < static_cast<int>(m_stack.size()); ++i) {
        if (m_stack[i].creatureId != 0)
            continue;
        const ThingType* type = entryThingType(m_stack[i].appearance);
        if (!type)
            continue;
        if (type->isForceUse()
            || (!type->isGround() && !type->isGroundBorder() && !type->isOnBottom() && !type->isOnTop()
                && !type->isSplash()))
            return hitAt(i);
    }
    for (int i = static_cast<int>(m_stack.size()) - 1; i > 0; --i) {
        if (m_stack[i].creatureId != 0)
            continue;
        const ThingType* type = entryThingType(m_stack[i].appearance);
        if (type && !type->isSplash())
            return hitAt(i);
    }
    return hitAt(0);
}

std::vector<TWorldMapField::TFieldItemHit> TWorldMapField::itemHits() const
{
    std::vector<TFieldItemHit> out;
    out.reserve(m_stack.size());
    for (int stackPos = 0; stackPos < static_cast<int>(m_stack.size()); ++stackPos) {
        if (m_stack[stackPos].creatureId != 0)
            continue;
        const appearances::TAppearanceInstancePtr& appearance = m_stack[stackPos].appearance;
        if (!appearance || appearance->category() != ThingCategoryItem)
            continue;
        out.push_back({ appearance->id(), stackPos });
    }
    return out;
}

int TWorldMapField::getGroundSpeed()
{
    const ThingType* type = m_stack.empty() ? nullptr : entryThingType(m_stack[0].appearance);
    if (type && type->isGround())
        return type->getGroundSpeed();
    return 100;
}

int TWorldMapField::movementGroundSpeed() const
{
    for (const auto& entry : m_stack) {
        const ThingType* type = entryThingType(entry.appearance);
        if (type && type->isGround())
            return type->getGroundSpeed();
    }
    return 255;
}

uint8_t TWorldMapField::getMinimapColorByte()
{
    if (m_minimapColor != 0)
        return m_minimapColor;

    for (const auto& entry : std::ranges::reverse_view(m_stack)) {
        if (entry.creatureId != 0 || typeIsCommon(entryThingType(entry.appearance)))
            continue;
        if (const ThingType* type = entryThingType(entry.appearance)) {
            const uint8_t c = static_cast<uint8_t>(type->getMinimapColor());
            if (c != 0)
                return c;
        }
    }

    return 255;
}

CreaturePtr TWorldMapField::getTopCreature(const bool /*checkAround*/)
{
    return nullptr;
}

bool TWorldMapField::isWalkable(const bool ignoreCreatures) const
{
    if (m_thingTypeFlag & NOT_WALKABLE || !hasGround())
        return false;

    if (!ignoreCreatures && hasCreatures()) {
        tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
        for (const auto& entry : m_stack) {
            if (entry.creatureId == 0)
                continue;
            const std::shared_ptr<tibia::creatures::TCreature> row =
                storage ? storage->creatureAt(entry.creatureId) : nullptr;
            if (!row || row->isPassable() || !row->canBeSeen())
                continue;
            return false;
        }
    }

    return true;
}

int TWorldMapField::walkState() const
{
    // An unpassable item blocks and an item without a movement animation is
    // unsure. The first creature ends the scan: an unpassable one blocks, or is
    // unsure when it is a player. Otherwise the field is free once a ground
    // item was seen.
    bool ground = false;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0) {
            const std::shared_ptr<tibia::creatures::TCreature> row = storage ? storage->creatureAt(entry.creatureId) : nullptr;
            if (row && !row->isPassable())
                return row->creatureType() != 0 ? 1 : 2;
            return ground ? 0 : 2;
        }
        const ThingType* type = entryThingType(entry.appearance);
        if (!type)
            continue;
        if (type->isGround())
            ground = true;
        if (type->isNotWalkable())
            return 1;
        if (type->hasNoMovementAnimation())
            return 2;
    }
    return ground ? 0 : 2;
}

bool TWorldMapField::isCompletelyCovered(const uint8_t firstFloor, const bool resetCache)
{
    if (getPosition().z == 0 || getPosition().z == firstFloor) return false;

    if (resetCache)
        invalidateCoverCache();

    if (hasCreatures() || !m_walkingCreatureIds.empty() || hasLight())
        return false;

    const uint32_t idChecked = 1 << firstFloor;
    const uint32_t idState = 1 << (firstFloor + tibia::config::mapMaxZ());
    if ((m_isCompletelyCovered & idChecked) == 0) {
        m_isCompletelyCovered |= idChecked;
        bool isLoading = false;
        if (tibia::game::isCompletelyCovered(
                { getPosition().x, getPosition().y, getPosition().z }, isLoading, firstFloor)) {
            m_isCompletelyCovered |= idState;
            m_isCovered |= idChecked; // Set covered is Checked
            m_isCovered |= idState;
        }

        if (isLoading) {
            m_isCompletelyCovered &= ~idChecked;
            m_isCompletelyCovered &= ~idState;
            m_isCovered &= ~idChecked;
            m_isCovered &= ~idState;
            return false;
        }
    }

    return (m_isCompletelyCovered & idState) == idState;
}

bool TWorldMapField::isCovered(const int8_t firstFloor)
{
    if (getPosition().z == 0 || getPosition().z == firstFloor) return false;

    const uint32_t idChecked = 1 << firstFloor;
    const uint32_t idState = 1 << (firstFloor + tibia::config::mapMaxZ());

    if ((m_isCovered & idChecked) == 0) {
        m_isCovered |= idChecked;
        bool isLoading = false;
        if (tibia::game::isCovered({ getPosition().x, getPosition().y, getPosition().z }, isLoading, firstFloor))
            m_isCovered |= idState;

        if (isLoading) {
            m_isCovered &= ~idChecked;
            m_isCovered &= ~idState;
            return false;
        }
    }

    return (m_isCovered & idState) == idState;
}

bool TWorldMapField::isClickable()
{
    bool hasGround = false;
    bool hasOnBottom = false;
    bool hasIgnoreLook = false;
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        const ThingType* type = entryThingType(entry.appearance);
        if (!type)
            continue;
        if (type->isGround())
            hasGround = true;
        else if (type->isOnBottom())
            hasOnBottom = true;

        if (type->isIgnoreLook())
            hasIgnoreLook = true;

        if ((hasGround || hasOnBottom) && !hasIgnoreLook)
            return true;
    }

    return false;
}

void TWorldMapField::onAddInMapView()
{
    m_drawTopAndCreature = true;
    if (m_thingTypeFlag & CORRECT_CORPSE) {
        uint8_t redrawPreviousTopW = 0;
        uint8_t redrawPreviousTopH = 0;

        for (const auto& entry : m_stack) {
            if (entry.creatureId != 0)
                continue;
            const ThingType* type = entryThingType(entry.appearance);
            if (!type || !type->isLyingCorpse())
                continue;

            redrawPreviousTopW = std::max<int>(type->getWidth() - 1, redrawPreviousTopW);
            redrawPreviousTopH = std::max<int>(type->getHeight() - 1, redrawPreviousTopH);
        }

        for (int x = -redrawPreviousTopW; x <= 0; ++x) {
            for (int y = -redrawPreviousTopH; y <= 0; ++y) {
                if (x == 0 && y == 0)
                    continue;

                if (TWorldMapField* neighbour = fieldAt(getPosition().translated(x, y)))
                    neighbour->setDrawTopAndCreature(false);
            }
        }
    }
}

bool TWorldMapField::hasBlockingCreature() const
{
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    for (const auto& entry : m_stack) {
        if (entry.creatureId == 0)
            continue;
        const std::shared_ptr<tibia::creatures::TCreature> row =
            storage ? storage->creatureAt(entry.creatureId) : nullptr;
        if (row && !row->isPassable() && !row->isLocalPlayer())
            return true;
    }
    return false;
}

bool TWorldMapField::hasBlockingBottomItem() const
{
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        const ThingType* type = entryThingType(entry.appearance);
        if (type && !type->isGround() && type->isOnBottom() && type->isNotWalkable())
            return true;
    }
    return false;
}

bool TWorldMapField::limitsFloorsView(const bool isFreeView)
{
    // ground and walls limits the view
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            break;
        const ThingType* type = entryThingType(entry.appearance);
        if (!type)
            continue;
        // iterate until common item is encountered
        if (typeIsCommon(type))
            break;

        if (!type->isDontHide()
            && (type->isGround()
                || (isFreeView ? type->isOnBottom() : type->isOnBottom() && type->blockProjectile())))
            return true;
    }

    return false;
}

bool TWorldMapField::checkForDetachableThing(const TileSelectType selectType)
{
    const auto& markIfYouNeed = [&] {
        if (m_selectType == TileSelectType::NONE) return;
        markHighlightedThing(Color::yellow);
    };

    m_highlightThingStackPos = -1;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    int deferredLocalStackPos = -1;
    for (int i = 0; i < static_cast<int>(m_stack.size()); ++i) {
        const uint32_t id = m_stack[i].creatureId;
        if (id == 0)
            continue;
        if (storage) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(id)) {
                if (row->isLocalPlayer()) {
                    deferredLocalStackPos = i;
                    continue;
                }
                m_highlightThingStackPos = static_cast<int8_t>(i);
                markIfYouNeed();
                return true;
            }
        }
    }
    if (deferredLocalStackPos >= 0) {
        m_highlightThingStackPos = static_cast<int8_t>(deferredLocalStackPos);
        markIfYouNeed();
        return true;
    }
    // Walking creature on this field (ids only — no Map getTopCreature / CreaturePtr).
    for (auto it = m_walkingCreatureIds.rbegin(); it != m_walkingCreatureIds.rend(); ++it) {
        const uint32_t id = *it;
        if (!id || !storage || !storage->creatureAt(id))
            continue;
        for (int i = 0; i < static_cast<int>(m_stack.size()); ++i) {
            if (m_stack[i].creatureId != id)
                continue;
            m_highlightThingStackPos = static_cast<int8_t>(i);
            markIfYouNeed();
            return true;
        }
        markIfYouNeed();
        return true;
    }

    const bool isFiltered = selectType != TileSelectType::NO_FILTERED;

    if (hasCommonItem()) {
        for (int i = 0; i < static_cast<int>(m_stack.size()); ++i) {
            if (m_stack[i].creatureId != 0)
                continue;
            const ThingType* type = entryThingType(m_stack[i].appearance);
            if (!typeIsCommon(type) || !entryDrawable(m_stack[i].appearance))
                continue;

            if (isFiltered && type->isIgnoreLook() && !type->isUsable() && !type->hasLight())
                continue;

            m_highlightThingStackPos = static_cast<int8_t>(i);
            markIfYouNeed();
            return true;
        }
    }

    if (hasBottomItem()) {
        for (int i = static_cast<int>(m_stack.size()) - 1; i >= 0; --i) {
            if (m_stack[i].creatureId != 0)
                continue;
            const ThingType* type = entryThingType(m_stack[i].appearance);
            if (!type || !type->isOnBottom() || !entryDrawable(m_stack[i].appearance))
                continue;

            if (isFiltered && (type->isIgnoreLook() || type->isFluidContainer()))
                continue;

            m_highlightThingStackPos = static_cast<int8_t>(i);
            markIfYouNeed();
            return true;
        }
    }

    if (hasTopItem()) {
        for (int i = static_cast<int>(m_stack.size()) - 1; i >= 0; --i) {
            if (m_stack[i].creatureId != 0)
                break;
            const ThingType* type = entryThingType(m_stack[i].appearance);
            if (!type || !type->isOnTop())
                break;
            if (!entryDrawable(m_stack[i].appearance))
                continue;

            if (isFiltered && (type->isIgnoreLook() || type->getLensHelp() == 0))
                continue;

            m_highlightThingStackPos = static_cast<int8_t>(i);
            markIfYouNeed();
            return true;
        }
    }

    if (!isFiltered) {
        m_highlightThingStackPos = static_cast<int8_t>(m_stack.size()) - 1;
        markIfYouNeed();
        return true;
    }

    return false;
}

void TWorldMapField::applyStackFlagsFromThingType(ThingType* type)
{
    if (!type)
        return;

    if (type->hasLight())
        m_thingTypeFlag |= HAS_LIGHT;
    if (type->hasDisplacement())
        m_thingTypeFlag |= HAS_DISPLACEMENT;

    if (typeIsCommon(type))
        m_thingTypeFlag |= HAS_COMMON_ITEM;
    if (type->isOnTop())
        m_thingTypeFlag |= HAS_TOP_ITEM;
    if (type->isSingleGroundBorder())
        m_thingTypeFlag |= HAS_GROUND_BORDER;
    if (type->isTopGroundBorder())
        m_thingTypeFlag |= HAS_TOP_GROUND_BORDER;
    if (type->isLyingCorpse() && !tibia::game::cipFeature(tibia::client::GameMapDontCorrectCorpse))
        m_thingTypeFlag |= CORRECT_CORPSE;

    if (type->isOnBottom()) {
        m_thingTypeFlag |= HAS_BOTTOM_ITEM;
        if (type->isHookSouth())
            m_thingTypeFlag |= HAS_HOOK_SOUTH;
        if (type->isHookEast())
            m_thingTypeFlag |= HAS_HOOK_EAST;
    }

    if (hasElevation())
        m_thingTypeFlag |= HAS_THING_WITH_ELEVATION;
    if (type->isIgnoreLook())
        m_thingTypeFlag |= IGNORE_LOOK;

    if (!type->isSingleDimension() || type->hasElevation() || type->hasDisplacement())
        m_thingTypeFlag |= NOT_SINGLE_DIMENSION;

    if (type->getHeight() > 1) {
        m_thingTypeFlag |= HAS_TALL_THINGS;
        if (type->getHeight() > 2)
            m_thingTypeFlag |= HAS_TALL_THINGS_2;
    }
    if (type->getWidth() > 1) {
        m_thingTypeFlag |= HAS_WIDE_THINGS;
        if (type->getWidth() > 2)
            m_thingTypeFlag |= HAS_WIDE_THINGS_2;
    }

    if (type->getWidth() > 1 && type->getHeight() > 1)
        m_thingTypeFlag |= HAS_WALL;
    if (type->isNotWalkable())
        m_thingTypeFlag |= NOT_WALKABLE;
    if (type->isNotPathable())
        m_thingTypeFlag |= NOT_PATHABLE;
    if (type->blockProjectile())
        m_thingTypeFlag |= BLOCK_PROJECTTILE;
    if (type->isFullGround())
        m_thingTypeFlag |= FULL_GROUND;
    if (type->hasElevation())
        ++m_elevation;
}

void TWorldMapField::applyCreatureStackFlags(const tibia::creatures::TCreature& stored, ThingType* outfitType)
{
    if (stored.light().intensity > 0 || (outfitType && outfitType->hasLight()))
        m_thingTypeFlag |= HAS_LIGHT;
    if (!stored.displacement().isNull() || (outfitType && outfitType->hasDisplacement()))
        m_thingTypeFlag |= HAS_DISPLACEMENT;

    m_thingTypeFlag |= HAS_CREATURE;

    if (hasElevation())
        m_thingTypeFlag |= HAS_THING_WITH_ELEVATION;

    if (!outfitType)
        return;

    if (!outfitType->isSingleDimension() || outfitType->hasElevation() || outfitType->hasDisplacement())
        m_thingTypeFlag |= NOT_SINGLE_DIMENSION;

    if (outfitType->getHeight() > 1) {
        m_thingTypeFlag |= HAS_TALL_THINGS;
        if (outfitType->getHeight() > 2)
            m_thingTypeFlag |= HAS_TALL_THINGS_2;
    }
    if (outfitType->getWidth() > 1) {
        m_thingTypeFlag |= HAS_WIDE_THINGS;
        if (outfitType->getWidth() > 2)
            m_thingTypeFlag |= HAS_WIDE_THINGS_2;
    }
}

void TWorldMapField::setStackEntryFlag(const TFieldStackEntry& entry)
{
    if (entry.creatureId != 0) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const auto row = storage->creatureAt(entry.creatureId)) {
                ThingType* outfitType = entry.appearance ? entry.appearance->thingType() : row->outfitAppearance()
                                                                                                   ? row->outfitAppearance()->thingType()
                                                                                                   : nullptr;
                applyCreatureStackFlags(*row, outfitType);
                return;
            }
        }
        // Storage miss: still stamp HAS_CREATURE from the stack id (protocol keep-alive
        // may fill the Map Creature later). Outfit geometry flags wait for sync.
        m_thingTypeFlag |= HAS_CREATURE;
        return;
    }
    applyStackFlagsFromThingType(entry.appearance ? entry.appearance->thingType() : nullptr);
}

void TWorldMapField::setMarkedOnStackEntry(const int stackPos, const Color& color)
{
    if (stackPos < 0 || stackPos >= static_cast<int>(m_stack.size()))
        return;
    if (auto* object = dynamic_cast<appearances::TObjectAppearanceInstance*>(m_stack[stackPos].appearance.get()))
        object->setMarkedColor(color);
}

void TWorldMapField::setThingFlag(const ThingPtr& thing)
{
    if (thing->hasLight())
        m_thingTypeFlag |= HAS_LIGHT;

    if (thing->hasDisplacement())
        m_thingTypeFlag |= HAS_DISPLACEMENT;

    if (thing->isCommon())
        m_thingTypeFlag |= HAS_COMMON_ITEM;

    if (thing->isOnTop())
        m_thingTypeFlag |= HAS_TOP_ITEM;

    if (thing->isCreature())
        m_thingTypeFlag |= HAS_CREATURE;

    if (thing->isSingleGroundBorder())
        m_thingTypeFlag |= HAS_GROUND_BORDER;

    if (thing->isTopGroundBorder())
        m_thingTypeFlag |= HAS_TOP_GROUND_BORDER;

    if (thing->isLyingCorpse() && !tibia::game::cipFeature(tibia::client::GameMapDontCorrectCorpse))
        m_thingTypeFlag |= CORRECT_CORPSE;

    // Creatures and items
    if (thing->isOnBottom()) {
        m_thingTypeFlag |= HAS_BOTTOM_ITEM;

        if (thing->isHookSouth())
            m_thingTypeFlag |= HAS_HOOK_SOUTH;

        if (thing->isHookEast())
            m_thingTypeFlag |= HAS_HOOK_EAST;
    }

    if (hasElevation())
        m_thingTypeFlag |= HAS_THING_WITH_ELEVATION;

    if (thing->isIgnoreLook())
        m_thingTypeFlag |= IGNORE_LOOK;

    // best option to have something more real, but in some cases as a custom project,
    // the developers are not defining crop size
    //if(thing->getRealSize > tibia::config::spriteSize)
    if (!thing->isSingleDimension() || thing->hasElevation() || thing->hasDisplacement())
        m_thingTypeFlag |= NOT_SINGLE_DIMENSION;

    if (thing->getHeight() > 1) {
        m_thingTypeFlag |= HAS_TALL_THINGS;

        if (thing->getHeight() > 2)
            m_thingTypeFlag |= HAS_TALL_THINGS_2;
    }

    if (thing->getWidth() > 1) {
        m_thingTypeFlag |= HAS_WIDE_THINGS;

        if (thing->getWidth() > 2)
            m_thingTypeFlag |= HAS_WIDE_THINGS_2;
    }

    if (!thing->isItem()) return;

    if (thing->getWidth() > 1 && thing->getHeight() > 1)
        m_thingTypeFlag |= HAS_WALL;

    if (thing->isNotWalkable())
        m_thingTypeFlag |= NOT_WALKABLE;

    if (thing->isNotPathable())
        m_thingTypeFlag |= NOT_PATHABLE;

    if (thing->blockProjectile())
        m_thingTypeFlag |= BLOCK_PROJECTTILE;

    if (thing->isFullGround())
        m_thingTypeFlag |= FULL_GROUND;

    if (thing->hasElevation())
        ++m_elevation;
}

void TWorldMapField::select(const TileSelectType selectType)
{
    m_selectType = selectType;

    if (selectType == TileSelectType::NO_FILTERED && !isEmpty()) {
        checkForDetachableThing(selectType);
    }

    markHighlightedThing(Color::yellow);
}

void TWorldMapField::unselect()
{
    m_selectType = TileSelectType::NONE;

    markHighlightedThing(Color::white);

    if (m_selectType == TileSelectType::NO_FILTERED)
        checkForDetachableThing();
}

bool TWorldMapField::canRender(uint32_t& flags, const Position& cameraPosition, const AwareRange viewPort)
{
    const int8_t dz = getPosition().z - cameraPosition.z;
    const auto& checkPos = getPosition().translated(dz, dz);

    bool draw = true;

    // Check for non-visible tiles on the screen and ignore them
    {
        if ((cameraPosition.x - checkPos.x >= viewPort.left) || (checkPos.x - cameraPosition.x == viewPort.right && !hasWideThings() && !hasDisplacement() && !hasThingWithElevation() && m_walkingCreatureIds.empty()))
            draw = false;
        else if ((cameraPosition.y - checkPos.y >= viewPort.top) || (checkPos.y - cameraPosition.y == viewPort.bottom && !hasTallThings() && !hasWideThings2() && !hasDisplacement() && !hasThingWithElevation() && m_walkingCreatureIds.empty()))
            draw = false;
        else if (((checkPos.x - cameraPosition.x > viewPort.right && (!hasWideThings() || !hasDisplacement() || !hasThingWithElevation())) || (checkPos.y - cameraPosition.y > viewPort.bottom)) && !hasTallThings2())
            draw = false;
    }

    if (!draw) {
        flags &= ~tibia::client::DrawThings;
        if (!hasLight())
            flags &= ~tibia::client::DrawLights;
        if (!hasCreatures())
            flags &= ~(tibia::client::DrawManaBar | tibia::client::DrawNames | tibia::client::DrawBars);
    }

    return flags > 0;
}

void TWorldMapField::drawTexts(Point dest)
{
    (void)dest;
}


void TWorldMapField::markHighlightedThing(const Color& color) {
    if (m_highlightThingStackPos > -1 && m_highlightThingStackPos < static_cast<int8_t>(m_stack.size()))
        setMarkedOnStackEntry(m_highlightThingStackPos, color);
}

void TWorldMapField::setText(const std::string& text, Color color)
{
    if (!m_text) {
        m_text = std::make_unique<StaticText>();
    }

    m_text->setText(text);
    m_text->setColor(color);
}

std::string TWorldMapField::getText()
{
    return m_text ? m_text->getText() : "";
}

void TWorldMapField::setTimer(int time, Color color)
{
    if (time > 60000) {
        g_logger.warning("Max tile timer value is 300000 (300s)!");
        return;
    }
    m_timer = time + g_clock.millis();
    if (!m_timerText) {
        m_timerText = std::make_unique<StaticText>();
    }

    m_timerText->setColor(color);
}

int TWorldMapField::getTimer()
{
    return m_timerText ? std::max<int>(0, m_timer - g_clock.millis()) : 0;
}

void TWorldMapField::setFill(Color color)
{
    m_fill = color;
}

bool TWorldMapField::canShoot(int distance)
{
    const Position playerPos = tibia::game::localPlayerPosition();
    if (!playerPos.isValid())
        return false;
    if (distance > 0 && std::max<int>(std::abs(getPosition().x - playerPos.x), std::abs(getPosition().y - playerPos.y)) > distance)
        return false;
    const Position fieldPos = getPosition();
    return tibia::game::isSightClear({ playerPos.x, playerPos.y, playerPos.z },
                                    { fieldPos.x, fieldPos.y, fieldPos.z });
}

bool TWorldMapField::isFullyOpaque() const {
    if (isFullGround())
        return true;

    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        if (const ThingType* type = entryThingType(entry.appearance); type && type->isOpaque())
            return true;
    }

    return false;
}

bool TWorldMapField::isLoading() const {
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        if (const ThingType* type = entryThingType(entry.appearance); type && type->isLoading())
            return true;
    }

    return false;
}

} // namespace tibia::worldmap
