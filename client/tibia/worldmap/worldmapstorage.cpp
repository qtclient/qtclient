#include "worldmap/worldmapstorage.h"

#include <sstream>
#include <stdexcept>
#include <typeinfo>

#include "worldmap/worldmapfield.h"
#include "input/gameactioncontext.h"

#include "creatures/creaturestorage.h"
#include "creatures/movementdirection.h"
#include "appearances/appearancetype.h"
#include "appearances/creatureplaceholderappearanceinstance.h"
#include "clockservice.h"
#include "iclockservice.h"

#include "client/const.h"
#include <tibia/config/runtimegameconfig.h>
#include <tibia/game/mapsession.h>
#include <memory>

namespace tibia::worldmap {

namespace {

TWorldMapStorage* g_worldMapStorage = nullptr;
std::unique_ptr<TWorldMapStorage> g_fallbackWorldMapStorage;

}

TWorldMapStorage::TWorldMapStorage(creatures::TCreatureMovementSpeedCalculator* speedCalculator, QObject* parent)
    : cip::tibia::worldmap::TWorldMapStorage(parent)
    , m_speedCalculator(speedCalculator ? speedCalculator : &m_ownSpeedCalculator)
{
    // Size missile floors only — full sessionState().init() calls resetAwareRange →
    // removeUnawareThings → ensureStorage(), which re-enters while g_worldMapStorage
    // is still null (stack overflow). Callers init after setInstance / ensureStorage.
    m_sessionState.ensureMissileFloorsSized();
}

TWorldMapStorage::TWorldMapStorage(std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> speedCalculator,
                                   std::shared_ptr<shared::IClockService> clock, QObject* parent)
    : cip::tibia::worldmap::TWorldMapStorage(parent)
    , m_speedCalculator(speedCalculator ? speedCalculator.get() : &m_ownSpeedCalculator)
    , m_sharedSpeedCalculator(std::move(speedCalculator))
    , m_clock(std::move(clock))
{
    m_sessionState.ensureMissileFloorsSized();
    qRegisterMetaType<TWorldMapCoordinate>("TWorldMapCoordinate");
}

TWorldMapStorage::~TWorldMapStorage()
{
    if (g_worldMapStorage == this)
        g_worldMapStorage = nullptr;
    for (const auto& [coordinate, field] : m_fields)
        field->disconnect(this);
}

TWorldMapStorage* TWorldMapStorage::instance()
{
    return g_worldMapStorage;
}

void TWorldMapStorage::setInstance(TWorldMapStorage* storage)
{
    g_worldMapStorage = storage;
}

TWorldMapStorage* TWorldMapStorage::ensureStorage()
{
    if (g_worldMapStorage)
        return g_worldMapStorage;
    if (!g_fallbackWorldMapStorage) {
        g_fallbackWorldMapStorage = std::make_unique<TWorldMapStorage>();
        g_worldMapStorage = g_fallbackWorldMapStorage.get();
        // After ctor: instance is live, so session init (aware range) is safe.
        g_worldMapStorage->sessionState().init();
        return g_worldMapStorage;
    }
    g_worldMapStorage = g_fallbackWorldMapStorage.get();
    return g_worldMapStorage;
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
    if (!m_sessionState.isAwareOfPosition(position))
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
    if (!m_sessionState.isAwareOfPosition(coordinate.toPosition()))
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

void TWorldMapStorage::setFieldObjects(const Position& position,
                                       const std::vector<appearances::TAppearanceInstancePtr>& objects)
{
    sessionState().setFieldObjects(position, objects);
}

bool TWorldMapStorage::insertObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position,
                                      const int stackPos)
{
    return sessionState().insertObjectAt(object, position, stackPos);
}

bool TWorldMapStorage::replaceObjectAt(const appearances::TAppearanceInstancePtr& object, const Position& position,
                                       const int stackPos)
{
    return sessionState().replaceObjectAt(object, position, stackPos);
}

appearances::TAppearanceInstancePtr TWorldMapStorage::removeObjectAt(const Position& position, const int stackPos)
{
    return sessionState().removeObjectAt(position, stackPos);
}

bool TWorldMapStorage::removeCreatureFromFields(const uint32_t creatureId, const Position& fieldPos,
                                                const bool fireLocalDisappear)
{
    return sessionState().removeCreatureFromFields(creatureId, fieldPos, fireLocalDisappear);
}

bool TWorldMapStorage::applyCreatureMove(const uint32_t creatureId, const Position& oldPos, const int oldStack,
                                         const Position& newPos, const bool byId)
{
    return sessionState().applyCreatureMove(creatureId, oldPos, oldStack, newPos, byId);
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
    const Position center = m_sessionState.getCentralPosition();
    const AwareRange range = m_sessionState.getAwareRange();
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
    m_sessionState.setCentralPosition(center);
    if (!(storedVolume() == previous))
        Q_EMIT storedVolumeHasChanged(storedVolume());
}

void TWorldMapStorage::addMissileAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance)
{
    sessionState().addMissileAppearance(appearance);
}

void TWorldMapStorage::addEffectAppearance(const tibia::appearances::TAppearanceInstancePtr& appearance,
                                           const Position& position)
{
    sessionState().addEffectAppearance(appearance, position);
}

void TWorldMapStorage::addStaticText(const StaticTextPtr& text, const Position& position)
{
    sessionState().addStaticText(text, position);
}

bool TWorldMapStorage::removeStaticText(const StaticTextPtr& text)
{
    return sessionState().removeStaticText(text);
}

void TWorldMapStorage::addAnimatedText(const AnimatedTextPtr& text, const Position& position)
{
    sessionState().addAnimatedText(text, position);
}

bool TWorldMapStorage::removeAnimatedText(const AnimatedTextPtr& text)
{
    return sessionState().removeAnimatedText(text);
}

WorldMapFieldList fieldsOnFloor(const int8_t floor)
{
    return TWorldMapStorage::ensureStorage()->fieldList(floor);
}

const std::vector<tibia::appearances::TAppearanceInstancePtr>& floorMissiles(const uint8_t z)
{
    return TWorldMapStorage::ensureStorage()->sessionState().getFloorMissiles(z);
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
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
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

}
