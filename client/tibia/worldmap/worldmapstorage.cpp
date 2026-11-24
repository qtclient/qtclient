#include "worldmap/worldmapstorage.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

#include "worldmap/worldmapfield.h"
#include "minimap/minimaptilestorage.h"
#include "minimap/waypointsectors.h"
#include "input/gameactioncontext.h"

#include "creatures/creaturestorage.h"
#include "creatures/movementdirection.h"
#include "appearances/appearancetype.h"
#include "appearances/creatureplaceholderappearanceinstance.h"
#include "clockservice.h"
#include "effects/effectstorage.h"
#include "iclockservice.h"

#include "client/const.h"
#include <tibia/config/runtimegameconfig.h>
#include <tibia/game/gamenotify.h>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "input/playermovementintenthandler.h"
#include <tibia/game/tgamenotify.h>
#include <any>
#include <cstdlib>
#include <string_view>
#include <memory>

namespace tibia::worldmap {

namespace {

/// The stored field at a map position (null off the map).
TWorldMapField* mapFieldAt(TWorldMapStorage& storage, const Position& position)
{
    return position.isMapPosition() ? storage.fieldAt(position) : nullptr;
}

/// The field at a map position, created inside the stored volume (null off the map).
TWorldMapField* createMapField(TWorldMapStorage& storage, const Position& position)
{
    return position.isMapPosition() ? storage.getOrCreateField(position) : nullptr;
}

bool sceneFrozen()
{
    const game::TGameSessionBase* session = game::currentGameSession();
    return session && session->isSceneFrozen();
}

/// The own creature's id while the session has a character (0 otherwise).
uint32_t sessionLocalPlayerId()
{
    const game::TGameSessionBase* session = game::currentGameSession();
    const creatures::TCreatureStorage* creatureRows = session && session->hasLocalPlayer() ? session->creatureStorage().get() : nullptr;
    return creatureRows ? creatureRows->playerID() : 0;
}

Position sessionLocalPlayerPosition()
{
    const game::TGameSessionBase* session = game::currentGameSession();
    return session ? session->localPlayerPosition() : Position();
}

/// The own creature while the session has a character.
std::shared_ptr<creatures::TCreature> sessionLocalPlayer()
{
    const game::TGameSessionBase* session = game::currentGameSession();
    const creatures::TCreatureStorage* creatureRows = session && session->hasLocalPlayer() ? session->creatureStorage().get() : nullptr;
    return creatureRows ? creatureRows->player() : nullptr;
}

/// Same floor, at most one field away.
bool isOneStep(const Position& from, const Position& to)
{
    return from.z == to.z && std::abs(from.x - to.x) <= 1 && std::abs(from.y - to.y) <= 1;
}

/// A floor change or a jump: the own creature's motion and the walk goal end.
void endLocalPlayerMotion()
{
    if (const std::shared_ptr<creatures::TCreature> player = sessionLocalPlayer())
        player->resetMovement();
    if (const game::TGameSessionBase* session = game::currentGameSession())
        if (input::TPlayerMovementIntentHandler* intent = session->movementIntent())
            intent->stopWalking();
}

/// Pack ABI fields onAddThing / onRemoveThing: (field position). Published only
/// while a pack listens: the map messages change many fields at once.
void publishThingChange(const std::string_view field, const Position& position)
{
    game::TGameNotify* notifier = game::TGameNotify::instance();
    if (notifier->wants(field))
        (void)notifier->publish(field, game::TGameNotify::Args{ std::any(position) });
}

/// probeCoverTile flags.
constexpr uint8_t ProbeSeen = 1;
constexpr uint8_t ProbeEmpty = 2;
constexpr uint8_t ProbeOpaque = 4;
constexpr uint8_t ProbeTopGround = 8;
constexpr uint8_t ProbeLoading = 16;

}

TWorldMapStorage::TWorldMapStorage(creatures::TCreatureMovementSpeedCalculator* speedCalculator, QObject* parent)
    : cip::tibia::worldmap::TWorldMapStorage(parent)
    , m_speedCalculator(speedCalculator ? speedCalculator : &m_ownSpeedCalculator)
{
    // Host: the aware range is reset by the owner once the storage is installed
    // (TGameSessionBase::initializeWorldMapSessionState, ensureStorage).
}

TWorldMapStorage::TWorldMapStorage(std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> speedCalculator,
                                   std::shared_ptr<shared::IClockService> clock, QObject* parent)
    : cip::tibia::worldmap::TWorldMapStorage(parent)
    , m_speedCalculator(speedCalculator ? speedCalculator.get() : &m_ownSpeedCalculator)
    , m_sharedSpeedCalculator(std::move(speedCalculator))
    , m_clock(std::move(clock))
{
    qRegisterMetaType<TWorldMapCoordinate>("TWorldMapCoordinate");
}

TWorldMapStorage::~TWorldMapStorage()
{
    for (const auto& [coordinate, field] : m_fields)
        field->disconnect(this);
}

void TWorldMapStorage::removeField(const Position& position)
{
    const auto it = m_fields.find(TWorldMapCoordinate(position));
    if (it == m_fields.end())
        return;
    it->second->disconnect(this);
    m_fields.erase(it);
}

void TWorldMapStorage::clearAllFields()
{
    for (const auto& [coordinate, field] : m_fields)
        field->disconnect(this);
    m_fields.clear();
}

WorldMapFieldList TWorldMapStorage::fieldList(const int8_t floor) const
{
    WorldMapFieldList result;
    result.reserve(m_fields.size());
    for (const auto& [coordinate, field] : m_fields) {
        if (floor < 0 || coordinate.z == floor)
            result.push_back(field.get());
    }
    return result;
}

std::shared_ptr<TWorldMapField> TWorldMapStorage::field(const TWorldMapCoordinate& coordinate) const
{
    const auto it = m_fields.find(coordinate);
    return it != m_fields.end() ? it->second : nullptr;
}

void TWorldMapStorage::removeFieldsOutsideStoredVolume(const std::function<bool(const Position&)>& isStored,
                                                       const std::function<void(const Position&)>& onRemoved)
{
    for (auto it = m_fields.begin(); it != m_fields.end();) {
        const Position position = it->first.toPosition();
        if (isStored(position)) {
            ++it;
            continue;
        }
        const std::shared_ptr<TWorldMapField> field = it->second;
        field->disconnect(this);
        it = m_fields.erase(it);
        if (!field->isEmpty() || field->hasEffect())
            field->setObjects({});
        onRemoved(position);
        Q_EMIT informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate(position));
    }
}

void TWorldMapStorage::fieldAppearanceInstancesChanged(const TWorldMapField* field)
{
    Q_EMIT informationAtWorldMapCoordinateHasChanged(field->coordinate());
}

TWorldMapField* TWorldMapStorage::fieldAt(const TWorldMapCoordinate& coordinate)
{
    return fieldAt(coordinate.toPosition());
}

TWorldMapField* TWorldMapStorage::fieldAt(const Position& position)
{
    const auto it = m_fields.find(TWorldMapCoordinate(position));
    return it != m_fields.end() ? it->second.get() : nullptr;
}

TWorldMapField* TWorldMapStorage::getOrCreateField(const Position& position)
{
    const TWorldMapCoordinate coordinate(position);
    if (const auto it = m_fields.find(coordinate); it != m_fields.end())
        return it->second.get();
    if (!isInStoredVolume(position))
        return nullptr;
    // Deleted from the event loop: callers may still hold the raw field in the
    // update that drops it.
    // The storage's fields hold at most 10 objects.
    std::shared_ptr<TWorldMapField> field(new TWorldMapField(coordinate, 10),
                                          [](TWorldMapField* dropped) { dropped->deleteLater(); });
    m_fields.emplace(coordinate, field);
    connect(field.get(), &TWorldMapField::appearanceInstancesChanged, this,
            &TWorldMapStorage::fieldAppearanceInstancesChanged);
    Q_EMIT informationAtWorldMapCoordinateHasChanged(coordinate);
    return field.get();
}

std::vector<std::pair<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>>> TWorldMapStorage::fields() const
{
    return { m_fields.begin(), m_fields.end() };
}

std::vector<std::pair<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>>>
TWorldMapStorage::fieldsIn(const TShearedWorldMapCuboid& cuboid) const
{
    std::vector<std::pair<TWorldMapCoordinate, std::shared_ptr<TWorldMapField>>> inside;
    for (const auto& entry : m_fields) {
        if (cuboid.contains(entry.first))
            inside.push_back(entry);
    }
    return inside;
}

std::vector<std::shared_ptr<creatures::TCreature>> TWorldMapStorage::creaturesIn(const TShearedWorldMapCuboid& cuboid) const
{
    std::vector<std::shared_ptr<creatures::TCreature>> creatures;
    for (const auto& [coordinate, field] : m_fields) {
        if (!cuboid.contains(coordinate))
            continue;
        const auto onField = field->creatures();
        creatures.insert(creatures.end(), onField.begin(), onField.end());
    }
    return creatures;
}

void TWorldMapStorage::setField(const TWorldMapCoordinate& coordinate, std::shared_ptr<TWorldMapField> field)
{
    if (!isInStoredVolume(coordinate.toPosition()))
        return;
    if (const auto stored = m_fields.find(coordinate); stored != m_fields.end())
        disconnect(stored->second.get(), &TWorldMapField::appearanceInstancesChanged, this,
                   &TWorldMapStorage::fieldAppearanceInstancesChanged);
    if (!field) {
        m_fields.erase(coordinate);
    } else {
        m_fields.insert_or_assign(coordinate, field);
        connect(field.get(), &TWorldMapField::appearanceInstancesChanged, this,
                &TWorldMapStorage::fieldAppearanceInstancesChanged);
    }
    Q_EMIT informationAtWorldMapCoordinateHasChanged(coordinate);
}

void TWorldMapStorage::notifyCoordinateChanged(const Position& position)
{
    // A stored field announces its coordinate itself.
    if (TWorldMapField* field = fieldAt(position))
        field->notifyAppearanceInstancesChanged();
    else
        Q_EMIT informationAtWorldMapCoordinateHasChanged(TWorldMapCoordinate(position));
}

TShearedWorldMapCuboid TWorldMapStorage::storedVolume() const
{
    if (m_storedVolume)
        return *m_storedVolume;
    const Position center = m_centralPosition;
    const AwareRange range = m_awareRange;
    // The session places the volume (CipSoft's setStoredVolume always widens
    // it by the margins).
    return TShearedWorldMapCuboid(
        TWorldMapCoordinate(center.x - range.left - m_storedVolumeMarginLeft,
                            center.y - range.top - m_storedVolumeMarginTop, center.z),
        TWorldMapExtent(range.horizontal() + m_storedVolumeMarginLeft + m_storedVolumeMarginRight,
                        range.vertical() + m_storedVolumeMarginTop + m_storedVolumeMarginBottom, 1),
        center.z);
}

void TWorldMapStorage::setStoredVolume(const Position& center)
{
    const TShearedWorldMapCuboid previous = storedVolume();
    // The session's centre defines the known area; moving it drops the fields
    // that leave it.
    setCentralPosition(center);
    if (!(storedVolume() == previous))
        Q_EMIT storedVolumeHasChanged(storedVolume());
}

void TWorldMapStorage::setStoredVolume(const TShearedWorldMapCuboid& volume, const bool withMargins)
{
    TShearedWorldMapCuboid stored = volume;
    if (withMargins) {
        const TWorldMapCoordinate origin(volume.origin.x - m_storedVolumeMarginLeft,
                                         volume.origin.y - m_storedVolumeMarginTop, volume.origin.z);
        const TWorldMapExtent extent(volume.extent.width + m_storedVolumeMarginLeft + m_storedVolumeMarginRight,
                                     volume.extent.height + m_storedVolumeMarginTop + m_storedVolumeMarginBottom,
                                     volume.extent.depth);
        stored = TShearedWorldMapCuboid(origin, extent, volume.shearFloor);
    }
    const bool changed = !(storedVolume() == stored);
    m_storedVolume = stored;
    if (changed)
        Q_EMIT storedVolumeHasChanged(stored);
    removeFieldsOutsideStoredVolume([this](const Position& position) { return isInStoredVolume(position); },
                                    [](const Position&) {});
}

bool TWorldMapStorage::isInStoredVolume(const Position& position) const
{
    if (m_storedVolume)
        return m_storedVolume->contains(TWorldMapCoordinate(position));
    return isAwareOfPosition(position);
}

namespace {

/// The minimap data of a field the storage does not hold: no colour, no
/// ground, impassable and avoided.
minimap::TMinimapDataOfOneField unknownFieldMinimapData()
{
    minimap::TMinimapDataOfOneField data;
    data.color = 0;
    data.cost = 0xfe;
    data.blocked = true;
    data.avoid = true;
    return data;
}

}

minimap::TMinimapDataOfOneField TWorldMapStorage::minimapDataOfField(const TWorldMapCoordinate& coordinate) const
{
    minimap::TMinimapDataOfOneField data = unknownFieldMinimapData();
    if (const std::shared_ptr<TWorldMapField> stored = field(coordinate))
        fillMinimapData(*stored, data);
    return data;
}

std::map<TWorldMapCoordinate, minimap::TMinimapDataOfOneField> TWorldMapStorage::minimapDataOfFloor(const int z) const
{
    std::map<TWorldMapCoordinate, minimap::TMinimapDataOfOneField> floor;
    for (const auto& [coordinate, stored] : m_fields) {
        if (coordinate.z != z)
            continue;
        const auto entry = floor.try_emplace(coordinate, unknownFieldMinimapData()).first;
        fillMinimapData(*stored, entry->second);
    }
    return floor;
}

std::shared_ptr<minimap::TWaypointSectors> TWorldMapStorage::waypointCostMap(const TWorldMapCoordinate& start,
                                                                             const TWorldMapCoordinate& goal) const
{
    auto costs = std::make_shared<minimap::TWaypointSectors>();
    for (const auto& [coordinate, stored] : m_fields) {
        if (coordinate.z != start.z)
            continue;
        const auto cost = static_cast<uint8_t>(stored->movementGroundSpeed());
        if (cost == 0 || stored->blocksMovement())
            continue;
        if (!stored->hasAvoidedObjectOrCreature() || coordinate == start || coordinate == goal)
            costs->setCost(coordinate.toPosition(), cost);
    }
    return costs;
}

void TWorldMapStorage::fillMinimapData(const TWorldMapField& field, minimap::TMinimapDataOfOneField& data) const
{
    data.color = field.minimapColor().value_or(0);
    data.cost = static_cast<uint8_t>(std::min(field.movementGroundSpeed(), 250));
    data.blocked = field.hasUnpassableObject();
    data.avoid = field.walkState() != 0 || field.hasAvoidedObjectOrCreature();
}

WorldMapFieldList fieldsOnFloor(const int8_t floor)
{
    TWorldMapStorage* storage = game::currentWorldMapStorage();
    return storage ? storage->fieldList(floor) : WorldMapFieldList{};
}

const std::vector<tibia::appearances::TAppearanceInstancePtr>& floorMissiles(const uint8_t z)
{
    static const std::vector<tibia::appearances::TAppearanceInstancePtr> none;
    const effects::TEffectStorage* effectStorage = game::currentEffectStorage();
    return effectStorage ? effectStorage->missilesOn(z) : none;
}

int TWorldMapStorage::groundSpeedAt(const Position& position)
{
    TWorldMapField* field = fieldAt(position);
    return field ? field->movementGroundSpeed() : 255;
}

int TWorldMapStorage::elevationAt(const Position& position)
{
    TWorldMapField* field = fieldAt(position);
    return field ? field->objectElevation() : 0;
}

void TWorldMapStorage::updateAnimations(const qint64 nowMs)
{
    for (const auto& [coordinate, field] : m_fields)
        field->updateAnimations(nowMs);
}

appearances::TAppearanceInstancePtr TWorldMapStorage::appearanceInstanceAt(const TWorldMapCoordinate& coordinate,
                                                                          const int stackPos)
{
    TWorldMapField* field = fieldAt(coordinate);
    return field ? field->appearanceAt(stackPos) : nullptr;
}

appearances::TAppearanceInstance* TWorldMapStorage::appearanceInstanceAt(
    const input::TLocationWorldMapCoordinateWithStackPositionContext& context)
{
    const std::shared_ptr<TWorldMapField> stored = field(TWorldMapCoordinate(context.coordinate()));
    return stored->appearances().at(context.stackPosition()).get();
}

appearances::TAppearanceInstancePtr TWorldMapStorage::creatureInstanceAt(const TWorldMapCoordinate& coordinate,
                                                                        const int stackPos)
{
    appearances::TAppearanceInstancePtr found = appearanceInstanceAt(coordinate, stackPos);
    if (found && found->creatureId() != 0)
        return found;
    std::ostringstream error;
    error << "Coordinate( " << coordinate.x << ", " << coordinate.y << ", " << coordinate.z << " ) Index( " << stackPos
          << " ) ";
    if (!found)
        error << "null";
    else {
        const appearances::TAppearanceInstance& appearance = *found;
        error << "FoundAppearanceInstance::AppearanceType( " << appearance.id()
              << " )  FoundAppearanceInstanceTypeId( " << typeid(appearance).name() << " )";
    }
    error << " Appearances in Network Order: ";
    if (TWorldMapField* field = fieldAt(coordinate)) {
        int index = 0;
        for (const appearances::TAppearanceInstancePtr& instance : field->appearances()) {
            if (instance)
                error << "Appearance: (" << instance->id() << ", "
                      << static_cast<int>(instance->thingType() ? instance->thingType()->category()
                                                                : appearances::EAppearanceCategory::Object)
                      << ") Index in Stack: " << index << "; ";
            ++index;
        }
    }
    throw std::runtime_error(error.str());
}

void TWorldMapStorage::placeCreatureInstance(const appearances::TAppearanceInstancePtr& instance,
                                             const TWorldMapCoordinate& target)
{
    TWorldMapField* field = fieldAt(target);
    if (!field)
        throw std::runtime_error(QStringLiteral("target field not existing").toStdString());
    const auto* placeholder = dynamic_cast<const appearances::TCreaturePlaceholderAppearanceInstance*>(instance.get());
    const std::shared_ptr<creatures::TCreature> creature = placeholder ? placeholder->creature() : nullptr;
    if (!creature)
        throw std::runtime_error(
            QStringLiteral("pCreatureInstance is not linked to a creature which should be placed at [%1,%2,%3]")
                .arg(target.x)
                .arg(target.y)
                .arg(target.z)
                .toStdString());
    // The field takes the instance where a creature sorts (a full field does
    // not), and the creature stands at the target.
    (void)field->addObject(instance);
    creature->setPosition(target.toPosition());
}

bool TWorldMapStorage::moveCreatureInstance(const TWorldMapCoordinate& from, const int stackPos,
                                            const TWorldMapCoordinate& target)
{
    TWorldMapField* source = fieldAt(from);
    if (!source)
        return false;
    const int elevationFrom = source->objectElevation();
    const appearances::TAppearanceInstancePtr instance = source->takeCreatureInstance(stackPos);
    if (!instance)
        return false;
    creatures::TCreatureStorage* storage = game::currentCreatureStorage();
    const auto creature = storage ? storage->creatureAt(instance->creatureId()) : nullptr;
    if (!creature)
        return false;
    placeCreatureInstance(instance, target);
    const auto look = creatures::lookDirectionAfter(static_cast<creatures::ECreatureLookDirection>(creature->patternX()),
                                                    creatures::directionBetween(from, target));
    creature->setDirection(static_cast<uint8_t>(look));
    moveCreature(*creature, from.toPosition(), target.toPosition(), elevationFrom, elevationAt(target.toPosition()));
    return true;
}

void TWorldMapStorage::moveCreature(creatures::TCreature& creature, const Position& from, const Position& to,
                                    const int elevationFrom, const int elevationTo)
{
    const TWorldMapCoordinate origin(from);
    const TWorldMapCoordinate target(to);
    const creatures::EMovementDirection direction = creatures::directionBetween(origin, target);
    if (static_cast<unsigned>(direction) >= 8) {
        creature.resetMovement();
        return;
    }
    const TWorldMapExtent step(target.x - origin.x, target.y - origin.y, target.z - origin.z);
    const qint64 duration = m_speedCalculator->stepDuration(creature.speed(), step, groundSpeedAt(to));
    const qint64 now = m_clock ? m_clock->nowMillis() : tibia::shared::TClockService::frameMillis();
    creature.move(now, now + duration, origin, direction, elevationFrom, elevationTo);
}

bool TWorldMapStorage::hasField(const TWorldMapCoordinate& coordinate)
{
    return fieldAt(coordinate) != nullptr;
}

bool TWorldMapStorage::hasUnpassableObjectAt(const TWorldMapCoordinate& coordinate)
{
    const TWorldMapField* field = fieldAt(coordinate);
    return field && field->hasUnpassableObject();
}

bool TWorldMapStorage::isUnderOpenSkyAt(const TWorldMapCoordinate& coordinate)
{
    if (coordinate.z > 7)
        return false;
    int x = coordinate.x;
    int y = coordinate.y;
    if (const TWorldMapField* field = fieldAt(coordinate); field && field->blocksSight()) {
        ++x;
        ++y;
    }
    return coordinate.z <= highestOpaqueFloorAt(x, y);
}

int TWorldMapStorage::highestOpaqueFloorAt(const int x, const int y)
{
    const TWorldMapField* top = fieldAt(TWorldMapCoordinate(x, y, 0));
    const bool topIsOpen = !top || !top->isOpaque();
    for (int z = 1; z < 7; ++z) {
        const TWorldMapField* field = fieldAt(TWorldMapCoordinate(x, y, z));
        if (field && field->isOpaque())
            return topIsOpen ? z : 0;
    }
    return 7;
}

bool TWorldMapStorage::hasBankAt(const TWorldMapCoordinate& coordinate)
{
    const TWorldMapField* field = fieldAt(coordinate);
    return field && field->hasBank();
}

bool TWorldMapStorage::hasCreatureAt(const TWorldMapCoordinate& coordinate)
{
    const TWorldMapField* field = fieldAt(coordinate);
    return field && !field->creatures().empty();
}

bool TWorldMapStorage::hasTranslucentAt(const TWorldMapCoordinate& coordinate)
{
    const TWorldMapField* field = fieldAt(coordinate);
    return field && field->hasTranslucent();
}

// Host: the field updates, the known area, the local player's field, the
// world light and the renderer's cover caches.

void TWorldMapStorage::fieldUpdated(const Position& position, const bool creature,
                                    const tibia::client::Operation operation)
{
    if (!position.isMapPosition())
        return;

    // A creature's OPERATION_CLEAN is only a walking-field hint: it must not bump
    // the cover or item revisions, or every step forces a full static remap.
    // Real field cleans come without a creature. Creatures are never opaque, so
    // only field cleans churn the cover caches.
    const bool fieldClean = operation == tibia::client::OPERATION_CLEAN && !creature;
    // Per-cell cover cache clear (a global wipe would cold-start every field on
    // each map-stream packet). While the static dirty list is saturated, one
    // wipe on the next cover-probe batch replaces the column walks.
    if (fieldClean) {
        m_coverRevision.fetch_add(1, std::memory_order_relaxed);
        if (m_staticDirty.size() >= MaxStaticDirty)
            m_coverInvalidateFlood = true;
        else
            invalidateCoverCachesFrom(position);
        m_itemRevision.fetch_add(1, std::memory_order_relaxed);
        // Static dirty for the world renderer; unaware eviction floods are skipped.
        if (!m_removingUnawareThings && m_staticDirty.size() < MaxStaticDirty)
            m_staticDirty.push_back(position);
    }

    // Walking-field hints (CLEAN with a creature) are not field changes.
    if (!m_removingUnawareThings && (creature ? operation != tibia::client::OPERATION_CLEAN : true))
        notifyCoordinateChanged(position);
}

void TWorldMapStorage::invalidateCoverCachesFrom(const Position& position)
{
    // isCompletelyCovered walks coveredUp from a field and tests a 2x2 at each
    // level. An opaque change only affects the fields that look up through that
    // cell: those columns lose their bits, not every field in the world.
    Position cursor = position;
    for (;;) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (TWorldMapField* field = mapFieldAt(*this, cursor.translated(dx, dy)))
                    field->invalidateCoverCache();
            }
        }
        if (!cursor.coveredDown())
            break;
    }
}

void TWorldMapStorage::invalidateAllCoverCaches()
{
    for (const auto& [coordinate, field] : m_fields)
        field->invalidateCoverCache();
    m_coverInvalidateFlood = false;
}

void TWorldMapStorage::beginCoverProbeBatch()
{
    if (m_coverInvalidateFlood)
        invalidateAllCoverCaches();
    m_coverProbe.clear();
    m_coverProbeActive = true;
}

void TWorldMapStorage::endCoverProbeBatch()
{
    m_coverProbeActive = false;
    m_coverProbe.clear();
}

uint8_t TWorldMapStorage::probeCoverTile(const Position& position)
{
    if (m_coverProbeActive) {
        const auto it = m_coverProbe.find(position);
        if (it != m_coverProbe.end())
            return it->second;
    }
    uint8_t flags = ProbeSeen;
    TWorldMapField* field = mapFieldAt(*this, position);
    if (!field) {
        flags |= ProbeEmpty;
    } else {
        if (field->isLoading())
            flags |= ProbeLoading;
        if (field->isFullyOpaque())
            flags |= ProbeOpaque;
        if (field->hasTopGround())
            flags |= ProbeTopGround;
    }
    if (m_coverProbeActive)
        m_coverProbe.emplace(position, flags);
    return flags;
}

size_t TWorldMapStorage::clearStaticDirtyTiles()
{
    if (m_coverInvalidateFlood)
        invalidateAllCoverCaches();
    const size_t n = m_staticDirty.size();
    m_staticDirty.clear();
    return n;
}

bool TWorldMapStorage::isCovered(const Position& position, bool& isLoading, const uint8_t firstFloor)
{
    // A field above the position, or the top ground one floor up and down-right.
    Position above = position;
    while (above.coveredUp() && above.z >= firstFloor) {
        const uint8_t here = probeCoverTile(above);
        if (here & ProbeLoading) {
            isLoading = true;
            return false;
        }
        if (here & ProbeOpaque)
            return true;
        if (probeCoverTile(above.translated(1, 1)) & ProbeTopGround)
            return true;
    }
    return false;
}

bool TWorldMapStorage::isCompletelyCovered(const Position& position, bool& isLoading, const uint8_t firstFloor)
{
    TWorldMapField* checked = mapFieldAt(*this, position);
    const bool single = !checked || checked->isSingleDimension();
    Position above = position;
    while (above.coveredUp() && above.z >= firstFloor) {
        bool covered = true;
        bool done = false;

        // Top ground on the 2x2 above.
        for (auto x = -1; ++x < 2 && !done;) {
            for (auto y = -1; ++y < 2 && !done;) {
                const uint8_t flags = probeCoverTile(above.translated(x, y));
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

        // Fully opaque fields on the 2x2 above.
        for (auto x = -1; ++x < 2 && !done;) {
            for (auto y = -1; ++y < 2 && !done;) {
                const uint8_t flags = probeCoverTile(above.translated(-x, -y));
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

void TWorldMapStorage::cleanDynamicThings()
{
    // The local player's creature is stored, too.
    if (creatures::TCreatureStorage* storage = game::currentCreatureStorage()) {
        const QList<creatures::TCreatureID> ids = storage->creatureIds();
        for (const creatures::TCreatureID id : ids)
            removeCreatureFromFields(id);
        for (const creatures::TCreatureID id : ids)
            storage->remove(id);
    }
}

void TWorldMapStorage::clean()
{
    cleanDynamicThings();
    clearAllFields();
}

void TWorldMapStorage::cleanField(const Position& position)
{
    if (sceneFrozen() || !position.isMapPosition())
        return;
    if (TWorldMapField* field = fieldAt(position)) {
        field->setObjects({});
        if (field->canErase())
            removeField(position);
        fieldUpdated(position, false, tibia::client::OPERATION_CLEAN);
    } else {
        notifyCoordinateChanged(position);
    }
}

void TWorldMapStorage::setFieldObjects(const Position& position,
                                       const std::vector<appearances::TAppearanceInstancePtr>& objects)
{
    // The field's content goes first; a field left without objects is dropped.
    cleanField(position);
    if (objects.empty() || sceneFrozen())
        return;
    TWorldMapField* field = createMapField(*this, position);
    if (!field)
        return;
    field->setObjects(objects);
    m_itemRevision.fetch_add(1, std::memory_order_relaxed);
    fieldUpdated(position, false, tibia::client::OPERATION_ADD);
    publishThingChange("onAddThing", position);
    for (const appearances::TAppearanceInstancePtr& object : objects) {
        if (object->creatureId() != 0 && object->creatureId() == sessionLocalPlayerId())
            objectPlaced(object, position);
    }
}

bool TWorldMapStorage::insertObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position,
                                      const int stackPos)
{
    // Host: an item without a type is never placed.
    if (!object || (!object->isCreature() && object->id() == 0) || !position.isValid())
        return false;
    TWorldMapField* field = createMapField(*this, position);
    if (!field)
        return false;
    if (stackPos < 0 || stackPos >= 0xff)
        (void)field->addObject(object);
    else
        (void)field->insertObject(object, static_cast<uint8_t>(stackPos));
    objectPlaced(object, position);
    return true;
}

bool TWorldMapStorage::replaceObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position,
                                       const int stackPos)
{
    if (!object || stackPos < 0 || !position.isValid())
        return false;
    TWorldMapField* field = createMapField(*this, position);
    if (!field)
        return false;
    const appearances::TAppearanceInstancePtr removed =
        field->replaceObject(object, static_cast<uint8_t>(std::min(stackPos, 0xff)));
    // The local player leaving its field (a turn puts its placeholder back).
    if (removed && removed->creatureId() != 0 && removed->creatureId() == sessionLocalPlayerId()) {
        fieldUpdated(position, true, tibia::client::OPERATION_REMOVE);
        localPlayerDisappeared();
    }
    if (removed)
        publishThingChange("onRemoveThing", position);
    objectPlaced(object, position);
    return true;
}

appearances::TAppearanceInstancePtr TWorldMapStorage::removeObjectAt(const Position& position, const int stackPos)
{
    TWorldMapField* field = stackPos >= 0 && position.isValid() ? mapFieldAt(*this, position) : nullptr;
    if (!field)
        return nullptr;
    appearances::TAppearanceInstancePtr removed = field->removeObject(static_cast<uint8_t>(std::min(stackPos, 0xff)));
    if (!removed)
        return nullptr;
    publishThingChange("onRemoveThing", position);
    // A creature leaving is announced; an object's field announces itself.
    if (!removed->isCreature())
        return removed;
    const bool isLocal = removed->creatureId() != 0 && removed->creatureId() == sessionLocalPlayerId();
    fieldUpdated(position, isLocal, tibia::client::OPERATION_REMOVE);
    if (isLocal)
        localPlayerDisappeared();
    return removed;
}

void TWorldMapStorage::objectPlaced(const appearances::TAppearanceInstancePtr& object, const Position& position)
{
    publishThingChange("onAddThing", position);
    if (object->creatureId() != 0 && object->creatureId() == sessionLocalPlayerId()) {
        setLocalPlayerPosition(position);
        localPlayerAppeared();
        fieldUpdated(position, true, tibia::client::OPERATION_ADD);
        return;
    }
    if (!object->isCreature())
        m_itemRevision.fetch_add(1, std::memory_order_relaxed);
    fieldUpdated(position, false, tibia::client::OPERATION_ADD);
}

void TWorldMapStorage::addMissileAppearance(const appearances::TAppearanceInstancePtr& appearance)
{
    if (dynamic_cast<appearances::TMissileEffectInstance*>(appearance.get()))
        if (effects::TEffectStorage* effectStorage = game::currentEffectStorage())
            (void)effectStorage->add(appearance);
}

void TWorldMapStorage::addEffectAppearance(const appearances::TAppearanceInstancePtr& appearance,
                                           const Position& position)
{
    if (!appearance || appearance->category() != ThingCategoryEffect)
        return;
    if (!dynamic_cast<appearances::TEffectInstance*>(appearance.get()))
        return;
    // The effect shows on its field even before the field has content.
    (void)createMapField(*this, position);
    if (effects::TEffectStorage* effectStorage = game::currentEffectStorage(); effectStorage && effectStorage->add(appearance))
        fieldUpdated(position, false, tibia::client::OPERATION_ADD);
}

bool TWorldMapStorage::removeCreatureFromFields(const uint32_t creatureId, const Position& fieldPos,
                                                const bool fireLocalDisappear)
{
    if (creatureId == 0)
        return false;

    const bool isLocal = creatureId == sessionLocalPlayerId();
    bool removed = false;
    const auto removeAt = [&](const Position& at) {
        TWorldMapField* field = at.isValid() ? mapFieldAt(*this, at) : nullptr;
        if (!field)
            return;
        // The stack entry the creature's placeholder holds.
        const auto& stack = field->appearances();
        const auto it = std::ranges::find_if(stack, [creatureId](const appearances::TAppearanceInstancePtr& entry) {
            return entry->creatureId() == creatureId;
        });
        if (it == stack.end())
            return;
        (void)field->removeObject(static_cast<uint8_t>(it - stack.begin()));
        removed = true;
        publishThingChange("onRemoveThing", at);
        fieldUpdated(at, isLocal, tibia::client::OPERATION_REMOVE);
    };

    // The field the message names, else the field the creature stands on.
    removeAt(fieldPos);
    if (!removed) {
        if (creatures::TCreatureStorage* storage = game::currentCreatureStorage()) {
            if (const auto row = storage->creatureAt(creatureId); row && row->position() != fieldPos)
                removeAt(row->position());
        }
    }

    if (removed && fireLocalDisappear && isLocal)
        localPlayerDisappeared();
    return removed;
}

bool TWorldMapStorage::applyCreatureMove(const uint32_t creatureId, const Position& oldPos, const int oldStack,
                                         const Position& newPos, const bool byId)
{
    creatures::TCreatureStorage* storage = game::currentCreatureStorage();
    if (!creatureId || !newPos.isValid() || !storage)
        return false;
    const auto row = storage->getOrCreate(creatureId);
    if (!row)
        return false;

    // The position form names the field and slot the creature leaves; the id
    // form moves it from where it stands.
    const bool isLocal = creatureId == sessionLocalPlayerId();
    if (byId) {
        // The creature leaves the field it stands on for the target field.
        const Position from = row->position();
        if (from.isValid() && from != newPos)
            removeCreatureFromFields(creatureId, from, false);
        if (row->placeholderAppearance() && createMapField(*this, newPos)) {
            row->setPosition(newPos);
            insertObjectAt(row->placeholderAppearance(), newPos, -1);
        }
        moveCreature(*row, from, newPos, elevationAt(from), elevationAt(newPos));
        return true;
    }
    TWorldMapField* fromField = mapFieldAt(*this, oldPos);
    if (!fromField || fromField->stackCreatureId(oldStack) != creatureId)
        return false;
    if (!createMapField(*this, newPos)) {
        // A step out of the known area only leaves the field.
        (void)fromField->removeObject(static_cast<uint8_t>(oldStack));
        fieldUpdated(oldPos, isLocal, tibia::client::OPERATION_REMOVE);
        return true;
    }
    if (!moveCreatureInstance(TWorldMapCoordinate(oldPos), oldStack, TWorldMapCoordinate(newPos)))
        return false;
    fieldUpdated(oldPos, isLocal, tibia::client::OPERATION_REMOVE);
    if (isLocal) {
        setLocalPlayerPosition(newPos);
        localPlayerAppeared();
    }
    fieldUpdated(newPos, isLocal, tibia::client::OPERATION_ADD);
    return true;
}

void TWorldMapStorage::removeUnawareThings()
{
    // The creatures standing outside the known area leave their fields.
    if (creatures::TCreatureStorage* storage = game::currentCreatureStorage()) {
        for (const creatures::TCreatureID id : storage->creatureIds()) {
            const auto row = storage->creatureAt(id);
            if (!row || !row->position().isValid())
                continue;
            if (!isAwareOfPosition(row->position()))
                removeCreatureFromFields(id);
        }
    }
    if (const uint32_t localId = sessionLocalPlayerId();
        localId != 0 && !isAwareOfPosition(sessionLocalPlayerPosition()))
        removeCreatureFromFields(localId);

    // The storage keeps the fields of the known area only: every field outside
    // it goes, and its coordinate is announced as changed.
    m_removingUnawareThings = true;
    removeFieldsOutsideStoredVolume(
        [this](const Position& position) { return isAwareOfPosition(position); },
        [this](const Position& position) { fieldUpdated(position, false, tibia::client::OPERATION_CLEAN); });
    m_removingUnawareThings = false;
}

void TWorldMapStorage::resetLocalPlayer()
{
    if (m_localPlayerLeave) {
        m_localPlayerLeave->cancel();
        m_localPlayerLeave = nullptr;
    }
    m_localPlayerPosition = {};
    m_localPlayerLastField = {};
    m_localPlayerOffMap = true;
}

void TWorldMapStorage::setLocalPlayerPosition(const Position& position)
{
    if (m_localPlayerPosition == position)
        return;
    const Position oldPosition = m_localPlayerPosition;
    m_localPlayerPosition = position;
    tibia::game::notify("onPositionChange", position, oldPosition);
}

void TWorldMapStorage::localPlayerAppeared()
{
    if (m_localPlayerLeave) {
        m_localPlayerLeave->cancel();
        m_localPlayerLeave = nullptr;
    }
    const Position position = sessionLocalPlayerPosition();
    if (m_localPlayerOffMap) {
        m_localPlayerOffMap = false;
    } else if (m_localPlayerLastField != position) {
        // The first field after the login is no jump.
        if (m_localPlayerLastField.isValid() && !isOneStep(m_localPlayerLastField, position))
            endLocalPlayerMotion();
        m_localPlayerLastField = position;
    }
    tibia::game::notify("onCreatureAppear", sessionLocalPlayerId());
}

void TWorldMapStorage::localPlayerDisappeared()
{
    if (m_localPlayerLeave)
        m_localPlayerLeave->cancel();
    m_localPlayerLastField = sessionLocalPlayerPosition();
    tibia::game::notify("onCreatureDisappear", sessionLocalPlayerId());
    m_localPlayerLeave = tibia::schedule::post([this] {
        m_localPlayerOffMap = true;
        if (const std::shared_ptr<creatures::TCreature> player = sessionLocalPlayer(); player && player->isWalking())
            player->stopMovement();
        m_localPlayerLastField = {};
        m_localPlayerLeave = nullptr;
    });
}

void TWorldMapStorage::setCentralPosition(const Position& center)
{
    if (sceneFrozen() || m_centralPosition == center)
        return;

    m_centralPosition = center;
    removeUnawareThings();

    // The server leaves the local player out of the map when its field holds
    // too many objects: the player then stands at the central position.
    tibia::schedule::post([this] {
        if (const game::TGameSessionBase* session = game::currentGameSession(); !session || !session->hasLocalPlayer())
            return;
        const Position oldPosition = sessionLocalPlayerPosition();
        if (oldPosition == m_centralPosition)
            return;
        if (const TWorldMapField* field = fieldAt(oldPosition)) {
            const uint32_t localId = sessionLocalPlayerId();
            if (std::ranges::any_of(field->appearances(), [localId](const appearances::TAppearanceInstancePtr& entry) {
                    return entry->creatureId() == localId;
                }))
                return;
        }
        if (!m_localPlayerOffMap)
            localPlayerDisappeared();
        setLocalPlayerPosition(m_centralPosition);
        localPlayerAppeared();
    });
}

void TWorldMapStorage::setWorldLight(const uint8_t intensity, const uint8_t color)
{
    const Light oldLight = m_worldLight;
    const Light newLight{ intensity, color };
    m_worldLight = newLight;
    if (oldLight.color != color || oldLight.intensity != intensity)
        tibia::game::notify("onWorldLightChange", newLight, oldLight);
}

bool TWorldMapStorage::isAwareOfPosition(const Position& position, const AwareRange& range) const
{
    if ((position.z < firstAwareFloor() || position.z > lastAwareFloor()) && range == m_awareRange)
        return false;

    Position grounded = position;
    while (grounded.z != m_centralPosition.z) {
        if (grounded.z > m_centralPosition.z) {
            // 65535,65535,15 cannot go up to 65536,65536,14.
            if (grounded.x == UINT16_MAX || grounded.y == UINT16_MAX)
                break;
            grounded.coveredUp();
        } else {
            // 0,0,0 cannot go down to -1,-1,1.
            if (grounded.x == 0 || grounded.y == 0)
                break;
            grounded.coveredDown();
        }
    }

    return m_centralPosition.isInRange(grounded, range.left, range.right, range.top, range.bottom);
}

void TWorldMapStorage::setAwareRange(const AwareRange& range)
{
    m_awareRange = range;
    removeUnawareThings();
}

void TWorldMapStorage::resetAwareRange()
{
    setAwareRange({ .left = static_cast<uint8_t>(tibia::config::mapViewPortWidth()),
                    .top = static_cast<uint8_t>(tibia::config::mapViewPortHeight()),
                    .right = static_cast<uint8_t>(tibia::config::mapViewPortWidth() + 1),
                    .bottom = static_cast<uint8_t>(tibia::config::mapViewPortHeight() + 1) });
}

uint8_t TWorldMapStorage::firstAwareFloor() const
{
    if (m_centralPosition.z <= tibia::config::mapSeaFloor())
        return 0;
    return m_centralPosition.z - tibia::config::mapAwareUndergroundFloorRange();
}

uint8_t TWorldMapStorage::lastAwareFloor() const
{
    if (m_centralPosition.z <= tibia::config::mapSeaFloor())
        return tibia::config::mapSeaFloor();
    return std::min<uint8_t>(m_centralPosition.z + tibia::config::mapAwareUndergroundFloorRange(),
                             tibia::config::mapMaxZ());
}

void TWorldMapStorage::setPathAvoid(const Position& position, const bool avoid)
{
    if (!position.isValid())
        return;
    if (avoid)
        m_pathAvoids.insert(position);
    else
        m_pathAvoids.erase(position);
}

void TWorldMapStorage::clearPathAvoids()
{
    m_pathAvoids.clear();
}

}
