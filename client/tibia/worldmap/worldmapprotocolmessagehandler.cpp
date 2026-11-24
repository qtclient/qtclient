#include "crashhandler.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/ambientlightstorage.h"

#include "protocol/protocolciptypes.h"
#include "worldmap/worldmapstorage.h"

#include "appearances/appearanceinstance.h"
#include "clockservice.h"
#include "creatures/creatureprotocolapply.h"
#include "creatures/creaturestorage.h"
#include "creatures/creaturesciptypes.h"
#include "effects/effectstorage.h"
#include "minimap/minimaptilestorage.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapviewport.h"

#include <tibia/game/gamenotify.h>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "input/playermovementintenthandler.h"
#include <tibia/game/sessionhooks.h>
#include <tibia/schedule/schedule.h>
#include <protocol.pb.h>

#include <cstdlib>
#include <stdexcept>

namespace tibia::worldmap {

namespace {

using namespace ::tibia::protobuf::protocol;

TWorldMapCoordinate coordinateOf(const ProtocolCoordinate& c)
{
    return { static_cast<int>(c.x()), static_cast<int>(c.y()), static_cast<int>(c.z()) };
}

bool handlerSceneFrozen()
{
    const game::TGameSessionBase* session = game::currentGameSession();
    return session && session->isSceneFrozen();
}

/// Same floor, at most one field away.
bool isOneFieldStep(const Position& from, const Position& to)
{
    return from.z == to.z && std::abs(from.x - to.x) <= 1 && std::abs(from.y - to.y) <= 1;
}

/// A floor change or a jump: the own creature's motion and the walk goal end.
void endLocalPlayerMotion(game::TGameSessionBase* session, creatures::TCreatureStorage* creatureStorage)
{
    if (!session || !session->hasLocalPlayer())
        return;
    if (creatureStorage)
        if (const std::shared_ptr<creatures::TCreature> player = creatureStorage->player())
            player->resetMovement();
    if (input::TPlayerMovementIntentHandler* intent = session->movementIntent())
        intent->stopWalking();
}

} // namespace

appearances::TAppearanceInstancePtr TWorldmapProtocolMessageHandler::applyObject(const AppearanceInstance& object,
                                                                                   const TWorldMapCoordinate& coordinate)
{
    if (object.HasExtension(CreatureData::creature_data)) {
        // The creature's data is applied first; the creature then stands at
        // the field and its placeholder is what the field holds.
        const CreatureData& data = object.GetExtension(CreatureData::creature_data);
        (void)creatures::applyProtocolCreature(data);
        const std::shared_ptr<creatures::TCreature> creature =
            m_creatureStorage ? m_creatureStorage->creatureAt(data.id()) : nullptr;
        if (!creature)
            return nullptr;
        creature->setPosition(coordinate.toPosition());
        return creature->placeholderAppearance();
    }
    // Any other object is made by the instance factory at the current time.
    // Host: animations start on the frame clock the renderer advances them by.
    appearances::TAppearanceInstancePtr appearance =
        m_appearanceInstanceFactory->createFromProtobuf(object, static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
    // Host: qtclient's factory takes no coordinate; an object drawn by its
    // field's pattern takes it from the coordinate here.
    if (const auto patterned = std::dynamic_pointer_cast<appearances::TPatternObjectAppearanceInstance>(appearance))
        patterned->applyWorldMapCoordinate(coordinate.toPosition());
    return appearance;
}

void TWorldmapProtocolMessageHandler::applyArea(const MapArea& area, const TShearedWorldMapCuboid& volume,
                                                const TWorldMapCentralPosition& central)
{
    // Field data in the other form describes no field.
    const MapFields& fields = area.field_data().fields();
    std::size_t index = 0;
    for (const MapField& field : fields.fields()) {
        const Position position = central.coordinateOfAreaIndex(volume, index).toPosition();
        // The storage holds fields of its known area only.
        if (map().isAwareOfPosition(position)) {
            // The objects replace the field's content in their order; objects
            // that yield nothing are left out, and a field left without objects
            // is dropped.
            std::vector<appearances::TAppearanceInstancePtr> objects;
            objects.reserve(static_cast<size_t>(field.objects_size()));
            for (const AppearanceInstance& object : field.objects()) {
                if (appearances::TAppearanceInstancePtr instance = applyObject(object, TWorldMapCoordinate(position)))
                    objects.push_back(std::move(instance));
            }
            map().setFieldObjects(position, objects);
            updateMinimapField(position);
        }
        index += static_cast<std::size_t>(field.skip()) + 1;
    }
    const std::size_t count = volume.getExtent().volume();
    if (index != count)
        throw std::runtime_error(
            QStringLiteral("Number of processed fields (%1) doesn't fit the number of fields in the area to read (%2)")
                .arg(index)
                .arg(count)
                .toStdString());
}

void TWorldmapProtocolMessageHandler::setMinimapTileStorage(minimap::TMinimapTileStorage* minimap)
{
    m_minimap = minimap;
}

void TWorldmapProtocolMessageHandler::updateMinimapField(const Position& position)
{
    if (m_minimap)
        m_minimap->onFieldChanged(position);
}

TWorldMapStorage& TWorldmapProtocolMessageHandler::map() const
{
    if (!m_worldMapStorage)
        throw std::runtime_error("the world map protocol message handler has no world map storage");
    return *m_worldMapStorage;
}

TWorldMapCentralPosition TWorldmapProtocolMessageHandler::centralPosition() const
{
    const AwareRange range = map().awareRange();
    return TWorldMapCentralPosition(m_center, TWorldMapExtent(range.horizontal(), range.vertical(), 1),
                                    TWorldMapExtent(range.left, range.top, 0));
}

void TWorldmapProtocolMessageHandler::moveKnownArea()
{
    const Position center = m_center.toPosition();
    map().setStoredVolume(center);
    if (m_effectStorage)
        m_effectStorage->removeOutside(
            [this](const Position& field) { return map().isAwareOfPosition(field); });
    // The minimap follows the view's floor and marks the fields of the area
    // the server left out as unseen, on every move (also onto the same spot).
    if (m_minimap) {
        m_minimap->setPlayerZ(center.z);
        m_minimap->markUnseen();
    }
    setCrashReportPosition(center);
}

void TWorldmapProtocolMessageHandler::setCrashReportPosition(const Position& center)
{
    // A crash report tells where the player stood.
    tibia::shared::TCrashHandler::instance().setAnnotation(
        QStringLiteral("Position"), QStringLiteral("[%1,%2,%3]").arg(center.x).arg(center.y).arg(center.z));
}

void TWorldmapProtocolMessageHandler::handleFullMapMessage(const GameserverMessageFullMap& message)
{
    if (handlerSceneFrozen())
        return;
    m_center = coordinateOf(message.coordinate());
    const Position position = m_center.toPosition();
    game::TGameSessionBase* session = game::currentGameSession();
    const bool hasPlayer = session && session->hasLocalPlayer();
    const Position oldPosition = hasPlayer ? session->localPlayerPosition() : Position();
    // The session's first map also places the local player.
    if (!m_mapKnown && hasPlayer)
        map().setLocalPlayerPosition(position);
    // The view centres on the map's field, its top-left subfield.
    if (m_viewport)
        m_viewport->setCenter(TWorldMapSubfieldCoordinate::fromField(m_center, 0, 0));
    moveKnownArea();
    // Every creature counts as off the map until the new area places it on a field.
    if (m_creatureStorage)
        m_creatureStorage->clearPlacedOnFieldFlags();
    // A jump re-places the view: the step in flight ends before the new map arrives.
    if (hasPlayer && oldPosition.isValid() && !isOneFieldStep(oldPosition, position))
        endLocalPlayerMotion(session, m_creatureStorage.get());
    const TWorldMapCentralPosition central = centralPosition();
    applyArea(message.area(), central.visibleArea(), central);
    m_mapKnown = true;
    schedule::post([] { game::notify("onMapDescription"); });
}

void TWorldmapProtocolMessageHandler::handleSlice(const MapArea& area, const TShearedWorldMapCuboid& volume)
{
    moveKnownArea();
    applyArea(area, volume, centralPosition());
}

void TWorldmapProtocolMessageHandler::handleTopRowMessage(const GameserverMessageTopRow& message)
{
    if (handlerSceneFrozen())
        return;
    --m_center.y;
    handleSlice(message.area(), centralPosition().topRow());
}

void TWorldmapProtocolMessageHandler::handleRightColumnMessage(const GameserverMessageRightColumn& message)
{
    if (handlerSceneFrozen())
        return;
    ++m_center.x;
    handleSlice(message.area(), centralPosition().rightColumn());
}

void TWorldmapProtocolMessageHandler::handleBottomRowMessage(const GameserverMessageBottomRow& message)
{
    if (handlerSceneFrozen())
        return;
    ++m_center.y;
    handleSlice(message.area(), centralPosition().bottomRow());
}

void TWorldmapProtocolMessageHandler::handleLeftColumnMessage(const GameserverMessageLeftColumn& message)
{
    if (handlerSceneFrozen())
        return;
    --m_center.x;
    handleSlice(message.area(), centralPosition().leftColumn());
}

void TWorldmapProtocolMessageHandler::handleTopFloorMessage(const GameserverMessageTopFloor& message)
{
    // The step in flight ends; the new floors land around the moved view.
    TWorldMapCentralPosition central = centralPosition();
    central.moveUpFloor();
    m_center = central.center;
    endLocalPlayerMotion(game::currentGameSession(), m_creatureStorage.get());
    moveKnownArea();
    applyArea(message.area(), central.topFloorArea(), central);
}

void TWorldmapProtocolMessageHandler::handleBottomFloorMessage(const GameserverMessageBottomFloor& message)
{
    TWorldMapCentralPosition central = centralPosition();
    central.moveDownFloor();
    m_center = central.center;
    endLocalPlayerMotion(game::currentGameSession(), m_creatureStorage.get());
    moveKnownArea();
    applyArea(message.area(), central.bottomFloorArea(), central);
}

void TWorldmapProtocolMessageHandler::handleFieldDataMessage(const GameserverMessageFieldData& message)
{
    if (handlerSceneFrozen())
        return;
    // An area of the one field at the coordinate.
    const TWorldMapCoordinate at = coordinateOf(message.coordinate());
    applyArea(message.area(), TShearedWorldMapCuboid(TWorldMapCuboid::ofField(at), at.z), centralPosition());
}

void TWorldmapProtocolMessageHandler::handleCreateOnMapMessage(const GameserverMessageCreateOnMap& message)
{
    if (handlerSceneFrozen())
        return;
    // Stack position 0xff puts the object where its kind sorts on the stack.
    const TWorldMapCoordinate at = coordinateOf(message.coordinate());
    if (const appearances::TAppearanceInstancePtr instance = applyObject(message.object(), at)) {
        map().insertObjectAt(instance, at.toPosition(), static_cast<int>(message.stack_position() & 0xff));
        updateMinimapField(at.toPosition());
    }
}

void TWorldmapProtocolMessageHandler::handleChangeOnMapMessage(const GameserverMessageChangeOnMap& message)
{
    if (handlerSceneFrozen())
        return;
    if (!message.has_coordinate()) {
        // The creature form only updates the creature; its field entry stays.
        if (message.object().HasExtension(CreatureData::creature_data))
            (void)creatures::applyProtocolCreature(message.object().GetExtension(CreatureData::creature_data));
        return;
    }
    // The object at the stack position leaves the field and the new one takes
    // its place (a creature's data is applied either way).
    const TWorldMapCoordinate at = coordinateOf(message.coordinate());
    if (const appearances::TAppearanceInstancePtr instance = applyObject(message.object(), at))
        map().replaceObjectAt(instance, at.toPosition(), static_cast<int>(message.stack_position() & 0xff));
    // The minimap records the field whether or not an object was placed.
    updateMinimapField(at.toPosition());
}

void TWorldmapProtocolMessageHandler::handleDeleteOnMapMessage(const GameserverMessageDeleteOnMap& message)
{
    if (handlerSceneFrozen())
        return;
    // The creature-id form leaves the creature where it is drawn: it only
    // stops counting as placed on a field.
    if (message.has_creature_id()) {
        if (m_creatureStorage)
            m_creatureStorage->removeFromField(message.creature_id());
        return;
    }
    // The field drops the object at the stack position, then the minimap
    // records the field.
    const Position at = coordinateOf(message.object_position().coordinate()).toPosition();
    (void)map().removeObjectAt(at, static_cast<int>(message.object_position().stack_position() & 0xff));
    updateMinimapField(at);
}

void TWorldmapProtocolMessageHandler::handleAmbientLightMessage(const GameserverMessageAmbientLight& AmbientLightMessage)
{
    const auto intensity = static_cast<uint8_t>(AmbientLightMessage.intensity());
    const auto color = static_cast<uint8_t>(AmbientLightMessage.color());
    map().setWorldLight(intensity, color);
    if (m_ambientLightStorage)
        m_ambientLightStorage->setLight(TAmbientLight(intensity, color));
}

void TWorldmapProtocolMessageHandler::handleTibiaTimeMessage(const GameserverMessageTibiaTime& TibiaTimeMessage)
{
    const auto hour = static_cast<uint8_t>(TibiaTimeMessage.hour());
    const auto minute = static_cast<uint8_t>(TibiaTimeMessage.minute());
    if (m_ambientLightStorage)
        m_ambientLightStorage->setNormalizedTibiaTimeOfDay(static_cast<float>(TibiaTimeMessage.hour()) / 24.0f
                                             + static_cast<float>(TibiaTimeMessage.minute()) / 1440.0f);
    tibia::world::setWorldTime(hour, minute);
    tibia::game::notify("onChangeWorldTime", hour, minute);
}

TWorldmapProtocolMessageHandler::TWorldmapProtocolMessageHandler(
    std::shared_ptr<TWorldMapStorage> worldMapStorage, std::shared_ptr<TAmbientLightStorage> ambientLightStorage,
    std::shared_ptr<creatures::TCreatureStorage> creatureStorage, std::shared_ptr<effects::TEffectStorage> effectStorage,
    std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory,
    std::shared_ptr<shared::IClockService> clockService, std::shared_ptr<TWorldMapViewport> viewport,
    std::shared_ptr<creatures::TCreatureProtocolMessageHandler> creatureProtocolMessageHandler, QObject* parent)
    : cip::tibia::worldmap::TWorldmapProtocolMessageHandler(parent)
    , m_worldMapStorage(std::move(worldMapStorage))
    , m_ambientLightStorage(std::move(ambientLightStorage))
    , m_creatureStorage(std::move(creatureStorage))
    , m_effectStorage(std::move(effectStorage))
    , m_appearanceInstanceFactory(std::move(appearanceInstanceFactory))
    , m_clockService(std::move(clockService))
    , m_viewport(std::move(viewport))
    , m_creatureProtocolMessageHandler(std::move(creatureProtocolMessageHandler))
{
}

}
