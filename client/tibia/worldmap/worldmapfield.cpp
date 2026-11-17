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
#include <tibia/appearances/appearancetype.h>
#include "tibia/assets/things.h"
#include "sprites/spritesystem.h"
#include <tibia/config/runtimegameconfig.h>
#include <tibia/game/mapsession.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/appearances/appearancetype.h>
#include "clockservice.h"
#include <tibia/schedule/schedule.h>
#include <tibia/schedule/schedule.h>
#include <tibia/game/gamesession.h>

namespace tibia::worldmap {

int TWorldMapField::stackOrder(const TFieldStackEntry& entry)
{
    // Bank, clip, bottom and top objects lead the stack in that order, then
    // the creatures, then every other object.
    const TAppearanceType* type = entry.appearance ? entry.appearance->thingType() : nullptr;
    if (type && type->isGround())
        return 0;
    if (type && type->isGroundBorder())
        return 1;
    if (type && type->isOnBottom())
        return 2;
    if (type && type->isOnTop())
        return 3;
    return entry.creatureId != 0 ? 4 : 5;
}

int TWorldMapField::insertionIndex(const TFieldStackEntry& entry) const
{
    // An object goes after the objects that sort before or with it; a common
    // object goes before the other common objects (the newest lies on top).
    const int order = stackOrder(entry);
    int index = 0;
    for (; index < static_cast<int>(m_stack.size()); ++index) {
        const int other = stackOrder(m_stack[index]);
        if (order < other)
            break;
        if (order == 5 && other == 5)
            break;
    }
    return index;
}

uint16_t TWorldMapField::itemClientIdAt(const int stackPos) const
{
    if (const appearances::TAppearanceInstancePtr appearance = appearanceAt(stackPos))
        return appearance->id();
    return 0;
}

void TWorldMapField::rebuildAppearanceCache()
{
    m_appearanceCache.clear();
    m_appearanceCache.reserve(m_stack.size());
    for (const auto& entry : m_stack)
        m_appearanceCache.push_back(entry.appearance);
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
                TAppearanceType* outfitType = row->outfitAppearance() ? row->outfitAppearance()->thingType() : nullptr;
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

namespace
{
    // A field taking a creature places it on a field; every creature leaving a
    // field is no longer placed (the battle list shows placed creatures only).
    void placeCreatureOnField(const uint32_t creatureId)
    {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
            storage->placeOnField(creatureId);
    }

    void removeCreatureFromField(const uint32_t creatureId)
    {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
            storage->removeFromField(creatureId);
    }
}

TWorldMapField::TWorldMapField(QObject* parent)
    : cip::tibia::worldmap::TWorldMapField(parent)
{
}

TWorldMapField::~TWorldMapField()
{
    for (const TFieldStackEntry& entry : m_stack) {
        if (entry.creatureId != 0)
            removeCreatureFromField(entry.creatureId);
    }
}

namespace
{
    TAppearanceType* entryThingType(const appearances::TAppearanceInstancePtr& appearance)
    {
        return appearance ? appearance->thingType() : nullptr;
    }

    const appearances::TObjectAppearanceInstance* entryObject(const appearances::TAppearanceInstancePtr& appearance)
    {
        return dynamic_cast<const appearances::TObjectAppearanceInstance*>(appearance.get());
    }

    bool typeIsCommon(const TAppearanceType* type)
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



void TWorldMapField::clean()
{
    // CIP tile refresh replaces the whole field stack; the local player's
    // appear/disappear does not run here (it would cancel its step).
    // The creatures it held leave the field.
    for (const TFieldStackEntry& entry : m_stack) {
        if (entry.creatureId != 0)
            removeCreatureFromField(entry.creatureId);
    }
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
            TAppearanceType* outfitType = row->outfitAppearance() ? row->outfitAppearance()->thingType() : nullptr;
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
    if (stackPos < 0 || stackPos == 255)
        stackPos = insertionIndex(TFieldStackEntry{ appearance, 0 });
    else if (stackPos > static_cast<int>(size))
        stackPos = size;
    // An object sorted past the last stack position does not fit on the field.
    if (stackPos >= static_cast<int>(tibia::config::tileMaxThings()))
        return;

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

    // The object pushed past the last stack position drops off the field.
    if (static_cast<uint8_t>(m_stack.size()) > tibia::config::tileMaxThings())
        eraseStackEntry(tibia::config::tileMaxThings());

    updateThingStackPos();
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
            placeCreatureOnField(creatureId);
            return;
        }
        removeCreatureById(creatureId);
    }

    const uint8_t size = static_cast<uint8_t>(m_stack.size());
    if (stackPos < 0 || stackPos == 255)
        stackPos = insertionIndex(TFieldStackEntry{ nullptr, creatureId });
    else if (stackPos > static_cast<int>(size))
        stackPos = size;
    if (stackPos >= static_cast<int>(tibia::config::tileMaxThings()))
        return;

    TFieldStackEntry entry;
    entry.creatureId = creatureId;
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
        const auto row = storage->getOrCreate(creatureId);
        row->setPosition(getPosition());
        entry.appearance = appearances::TAppearanceInstanceFactory::fromStoredCreature(*row);
    }
    m_stack.insert(m_stack.begin() + stackPos, std::move(entry));
    rebuildAppearanceCache();
    placeCreatureOnField(creatureId);

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

    // The object pushed past the last stack position drops off the field; a
    // creature among them keeps its placed-on-field flag.
    if (static_cast<uint8_t>(m_stack.size()) > tibia::config::tileMaxThings())
        eraseStackEntry(tibia::config::tileMaxThings());

    updateThingStackPos();
    notifyAppearanceInstancesChanged();
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
    eraseStackEntry(stackPos);
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

    eraseStackEntry(static_cast<int>(it - m_stack.begin()));
    removeCreatureFromField(creatureId);
    return true;
}

void TWorldMapField::eraseStackEntry(const int stackIndex)
{
    m_stack.erase(m_stack.begin() + stackIndex);
    rebuildAppearanceCache();
    recalculateThingFlag();
    updateThingStackPos();
    notifyAppearanceInstancesChanged();
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
    const TAppearanceType* type = m_stack.empty() ? nullptr : entryThingType(m_stack[0].appearance);
    return (type && type->isGround() && type->isSingleDimension()) || (m_thingTypeFlag & HAS_GROUND_BORDER);
}

bool TWorldMapField::hasTopGround(const bool ignoreBorder) const
{
    const TAppearanceType* type = m_stack.empty() ? nullptr : entryThingType(m_stack[0].appearance);
    return (type && type->isGround() && type->getWidth() + type->getHeight() == 4) || (!ignoreBorder && (m_thingTypeFlag & HAS_TOP_GROUND_BORDER));
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
        const TAppearanceType* type = entryThingType(m_stack[i].appearance);
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
        const TAppearanceType* type = entryThingType(m_stack[i].appearance);
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
    const TAppearanceType* type = m_stack.empty() ? nullptr : entryThingType(m_stack[0].appearance);
    if (type && type->isGround())
        return type->getGroundSpeed();
    return 100;
}

int TWorldMapField::movementGroundSpeed() const
{
    for (const auto& entry : m_stack) {
        const TAppearanceType* type = entryThingType(entry.appearance);
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
        if (const TAppearanceType* type = entryThingType(entry.appearance)) {
            const uint8_t c = static_cast<uint8_t>(type->getMinimapColor());
            if (c != 0)
                return c;
        }
    }

    return 255;
}

bool TWorldMapField::hasBank() const
{
    for (const auto& entry : m_stack) {
        if (const TAppearanceType* type = entryThingType(entry.appearance); type && type->isGround())
            return true;
    }
    return false;
}

namespace {

bool anyObject(const std::vector<TWorldMapField::TStackView>& stack, bool (*test)(const TAppearanceType&))
{
    for (const auto& view : stack) {
        if (view.type && test(*view.type))
            return true;
    }
    return false;
}

}

std::vector<TWorldMapField::TStackView> TWorldMapField::stackView() const
{
    std::vector<TStackView> view;
    view.reserve(m_stack.size());
    for (const auto& entry : m_stack)
        view.push_back({ entryThingType(entry.appearance), entry.creatureId });
    return view;
}

std::vector<std::shared_ptr<tibia::creatures::TCreature>> TWorldMapField::creatures() const
{
    std::vector<std::shared_ptr<tibia::creatures::TCreature>> result;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    for (const auto& entry : m_stack) {
        if (entry.creatureId == 0 || !storage)
            continue;
        if (auto creature = storage->creatureAt(entry.creatureId))
            result.push_back(std::move(creature));
    }
    return result;
}

bool TWorldMapField::hasBottom() const
{
    return anyObject(stackView(), [](const TAppearanceType& type) { return type.isOnBottom(); });
}

bool TWorldMapField::hasUnsight() const
{
    return anyObject(stackView(), [](const TAppearanceType& type) { return type.blockProjectile(); });
}

bool TWorldMapField::hasDontHide() const
{
    return anyObject(stackView(), [](const TAppearanceType& type) { return type.isDontHide(); });
}

bool TWorldMapField::hasTranslucent() const
{
    return anyObject(stackView(), [](const TAppearanceType& type) { return type.isTranslucent(); });
}

bool TWorldMapField::blocksMovement() const
{
    // An unpassable object blocks, and so does an unpassable creature other
    // than the local player.
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    for (const auto& entry : m_stack) {
        if (const TAppearanceType* type = entryThingType(entry.appearance); type && type->isNotWalkable())
            return true;
        if (entry.creatureId == 0 || !storage)
            continue;
        if (const auto creature = storage->creatureAt(entry.creatureId);
            creature && !creature->isLocalPlayer() && creature->isUnpassable())
            return true;
    }
    return false;
}

bool TWorldMapField::hasUnpassableObject() const
{
    for (const auto& view : stackView()) {
        if (view.type && view.type->isNotWalkable() && view.creatureId == 0)
            return true;
    }
    return false;
}

Light TWorldMapField::light() const
{
    // The light of the highest object that shines.
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
        if (const TAppearanceType* type = entryThingType(it->appearance); type && type->getLight().intensity != 0)
            return type->getLight();
    }
    return {};
}

void TWorldMapField::updateElevation()
{
    // In draw order every object but a top object stands on the elevation of
    // the objects drawn before it; the stack rises by each object's height up
    // to 24 pixels.
    const std::vector<int> order = drawList();
    uint8_t elevation = 0;
    for (const int index : order) {
        const appearances::TAppearanceInstancePtr& appearance = m_stack[index].appearance;
        const TAppearanceType* type = entryThingType(appearance);
        if (!type)
            continue;
        if (!type->isOnTop())
            appearance->setElevation(elevation);
        elevation = static_cast<uint8_t>(std::min(24, elevation + type->getElevation()));
    }
    m_objectElevation = elevation;
    // A hangable object hangs on the hook of the last hook object drawn before it.
    uint8_t hook = 0;
    for (const int index : order) {
        const appearances::TAppearanceInstancePtr& appearance = m_stack[index].appearance;
        if (const TAppearanceType* type = entryThingType(appearance)) {
            if (type->isHookSouth())
                hook = 1;
            else if (type->isHookEast())
                hook = 2;
        }
        if (const auto hangable = std::dynamic_pointer_cast<appearances::THangableObjectAppearanceInstance>(appearance))
            hangable->setHangSide(hook);
    }
}

void TWorldMapField::updateViewFlags()
{
    // Sight is blocked by an unsight object, or by a bottom object that can
    // neither be moved nor passed.
    m_blocksSight = anyObject(stackView(), [](const TAppearanceType& type) {
        return type.blockProjectile() || (type.isOnBottom() && type.isNotMoveable() && type.isNotWalkable());
    });
    // A bank or an unsight bottom object hides the floor below unless
    // something on the field is translucent.
    const bool covers = anyObject(stackView(), [](const TAppearanceType& type) {
        return type.isGround() || (type.isOnBottom() && type.blockProjectile());
    });
    m_opaque = covers && !hasTranslucent();
}

namespace {

bool isBank(const TAppearanceType* type)
{
    return type && type->isGround();
}
bool isClip(const TAppearanceType* type)
{
    return type && type->isGroundBorder();
}
bool isBottom(const TAppearanceType* type)
{
    return type && type->isOnBottom();
}
bool isTop(const TAppearanceType* type)
{
    return type && type->isOnTop();
}
bool isBankClipOrBottom(const TAppearanceType* type)
{
    return isBank(type) || isClip(type) || isBottom(type);
}
bool isForceUse(const TAppearanceType* type)
{
    return type && type->isForceUse();
}
bool isLiquidPool(const TAppearanceType* type)
{
    return type && type->isSplash();
}
bool isUnmove(const TAppearanceType* type)
{
    return type && type->isNotMoveable();
}
bool isIgnoreLook(const TAppearanceType* type)
{
    return type && type->isIgnoreLook();
}
bool isCorpse(const TAppearanceType* type)
{
    return type && type->isCorpse();
}
bool isContainer(const TAppearanceType* type)
{
    return type && type->isContainer();
}

}

uint16_t TFieldObject::clientId() const
{
    if (appearance)
        return appearance->id();
    if (creatureId) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(creatureId))
                if (row->outfitAppearance())
                    return row->outfitAppearance()->id();
        }
    }
    return 0;
}

TFieldObject TWorldMapField::fieldObject(const int stackIndex) const
{
    const TFieldStackEntry& entry = m_stack[stackIndex];
    TFieldObject object;
    object.stackPosition = stackPositionOf(entry.appearance);
    object.creatureId = entry.creatureId;
    object.appearance = entry.appearance;
    return object;
}

std::vector<int> TWorldMapField::drawList() const
{
    const int count = thingCount();
    int prefix = 0;
    while (prefix < count && isBankClipOrBottom(entryThingType(m_stack[prefix].appearance)))
        ++prefix;
    std::vector<int> list;
    list.reserve(count);
    for (int i = 0; i < prefix; ++i)
        list.push_back(i);
    for (int i = count - 1; i >= prefix; --i)
        list.push_back(i);
    return list;
}

int TWorldMapField::stackPositionOf(const appearances::TAppearanceInstancePtr& appearance) const
{
    for (int i = 0; i < thingCount() && i < 0xff; ++i) {
        if (m_stack[i].appearance == appearance)
            return i;
    }
    return 0xff;
}

TFieldObject TWorldMapField::moveObject() const
{
    const std::vector<int> list = drawList();
    int item = -1;
    int creature = -1;
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        const TFieldStackEntry& entry = m_stack[*it];
        const TAppearanceType* type = entryThingType(entry.appearance);
        if (isTop(type))
            continue;
        if (isBankClipOrBottom(type))
            break;
        if (entry.creatureId != 0) {
            if (creature < 0)
                creature = *it;
            continue;
        }
        if (!isUnmove(type))
            item = *it;
        break;
    }
    if (item >= 0)
        return fieldObject(item);
    if (creature >= 0)
        return fieldObject(creature);
    return {};
}

TFieldObject TWorldMapField::useObject() const
{
    const int count = thingCount();
    // Only pickable fields are asked, which are never empty.
    if (count == 0)
        return {};
    int index = 0;
    if (count != 1) {
        for (;;) {
            const TFieldStackEntry& entry = m_stack[index];
            const TAppearanceType* type = entryThingType(entry.appearance);
            if (isForceUse(type))
                return fieldObject(index);
            if (!isBankClipOrBottom(type) && !isTop(type) && entry.creatureId == 0)
                break;
            if (++index >= count - 1)
                break;
        }
        while (index != 0) {
            const TFieldStackEntry& entry = m_stack[index];
            if (entry.creatureId == 0 && !isLiquidPool(entryThingType(entry.appearance)))
                break;
            --index;
        }
    }
    return fieldObject(index);
}

TFieldObject TWorldMapField::quickLootObject() const
{
    const std::vector<int> list = drawList();
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        const TAppearanceType* type = entryThingType(m_stack[*it].appearance);
        if (isCorpse(type) && isContainer(type))
            return fieldObject(*it);
    }
    return {};
}

TFieldObject TWorldMapField::lookObject() const
{
    const int count = thingCount();
    if (count == 0)
        return {};
    int candidate = 0;
    for (int i = 0; i < count; ++i) {
        const TAppearanceType* type = entryThingType(m_stack[i].appearance);
        if (isIgnoreLook(type))
            continue;
        candidate = i;
        if (isBankClipOrBottom(type))
            continue;
        if (!isTop(type) || i + 1 == count)
            break;
    }
    return fieldObject(candidate);
}

TFieldObject TWorldMapField::multiUseObject() const
{
    const int count = thingCount();
    // Only pickable fields are asked, which are never empty.
    if (count == 0)
        return {};
    int index = 0;
    while (index != count - 1) {
        const TAppearanceType* type = entryThingType(m_stack[index].appearance);
        if (isForceUse(type) || !(isBankClipOrBottom(type) || isTop(type)))
            break;
        ++index;
    }
    if (index != 0 && isLiquidPool(entryThingType(m_stack[index].appearance)))
        --index;
    return fieldObject(index);
}

TFieldObject TWorldMapField::topCreature() const
{
    int creature = -1;
    for (const int index : drawList()) {
        if (m_stack[index].creatureId != 0)
            creature = index;
    }
    return creature >= 0 ? fieldObject(creature) : TFieldObject{};
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
        const TAppearanceType* type = entryThingType(entry.appearance);
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

bool TWorldMapField::isPickable() const
{
    for (const auto& entry : m_stack) {
        const TAppearanceType* type = entryThingType(entry.appearance);
        if (type && (type->isGround() || type->isOnBottom()) && !type->isIgnoreLook())
            return true;
    }
    return false;
}

bool TWorldMapField::hasBlockingBottomItem() const
{
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        const TAppearanceType* type = entryThingType(entry.appearance);
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
        const TAppearanceType* type = entryThingType(entry.appearance);
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

void TWorldMapField::applyStackFlagsFromThingType(TAppearanceType* type)
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
    if (type->isGroundBorder() && type->isSingleDimension())
        m_thingTypeFlag |= HAS_GROUND_BORDER;
    if (type->isGroundBorder() && type->getWidth() + type->getHeight() == 4)
        m_thingTypeFlag |= HAS_TOP_GROUND_BORDER;
    if (type->isLyingCorpse())
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

void TWorldMapField::applyCreatureStackFlags(const tibia::creatures::TCreature& stored, TAppearanceType* outfitType)
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
                TAppearanceType* outfitType = entry.appearance ? entry.appearance->thingType() : row->outfitAppearance()
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

bool TWorldMapField::isFullyOpaque() const {
    if (isFullGround())
        return true;

    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        if (const TAppearanceType* type = entryThingType(entry.appearance); type && type->isOpaque())
            return true;
    }

    return false;
}

bool TWorldMapField::isLoading() const {
    // A sprite of an object on the field is still being decoded.
    const auto typeIsLoading = [](const TAppearanceType& type) {
        for (const auto& [group, info] : type.frameGroups()) {
            for (const uint32_t id : info.spriteIds()) {
                if (!tibia::sprites::TSpriteSystem::instance().isSpriteReady(id))
                    return true;
            }
        }
        return false;
    };
    for (const auto& entry : m_stack) {
        if (entry.creatureId != 0)
            continue;
        if (const TAppearanceType* type = entryThingType(entry.appearance); type && typeIsLoading(*type))
            return true;
    }

    return false;
}

} // namespace tibia::worldmap
