#include "crashhandler.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/ambientlightstorage.h"

#include "protocol/protocolciptypes.h"
#include "worldmap/worldmapstorage.h"

#include "container/containerciptypes.h"
#include "creatures/creatureprotocolapply.h"
#include "creatures/creaturestorage.h"
#include "effects/effectstorage.h"
#include "minimap/minimaptilestorage.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapviewport.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/mapsession.h>
#include <tibia/game/sessionhooks.h>
#include <tibia/schedule/schedule.h>
#include <protocol.pb.h>

namespace tibia::worldmap {

namespace {

using namespace ::tibia::protobuf::protocol;

Position positionOf(const ProtocolCoordinate& c)
{
    return Position(static_cast<int32_t>(c.x()), static_cast<int32_t>(c.y()), static_cast<uint8_t>(c.z()));
}

} // namespace

appearances::TAppearanceInstancePtr TWorldmapProtocolMessageHandler::applyObject(const ProtocolMapObject& object,
                                                                                   const Position& position)
{
    if (object.has_creature()) {
        // The creature's data is applied first; the creature then stands at
        // the field and its placeholder is what the field holds.
        const uint32_t id = creatures::applyProtocolCreature(object.creature());
        creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
        const std::shared_ptr<creatures::TCreature> creature = id != 0 && storage ? storage->creatureAt(id) : nullptr;
        if (!creature)
            return nullptr;
        creature->setPosition(position);
        return creature->placeholderAppearance();
    }
    if (object.has_item()) {
        appearances::TAppearanceInstancePtr appearance =
            container::TInventorySlot::fromProtocolInventoryItem(object.item()).appearance;
        // An object drawn by its field's pattern takes it from the coordinate.
        if (const auto patterned = std::dynamic_pointer_cast<appearances::TPatternObjectAppearanceInstance>(appearance))
            patterned->applyWorldMapCoordinate(position);
        return appearance;
    }
    return nullptr;
}

void TWorldmapProtocolMessageHandler::applyField(const ProtocolMapField& field)
{
    const Position position = positionOf(field.coordinate());
    // The storage holds fields of its known area only.
    if (!game::isAwareOf({ position.x, position.y, position.z }))
        return;
    // The objects replace the field's content in their order; objects that
    // yield nothing are left out, and a field left without objects is dropped.
    std::vector<appearances::TAppearanceInstancePtr> objects;
    objects.reserve(static_cast<size_t>(field.objects_size()));
    for (const ProtocolMapObject& object : field.objects()) {
        if (appearances::TAppearanceInstancePtr instance = applyObject(object, position))
            objects.push_back(std::move(instance));
    }
    map().setFieldObjects(position, objects);
    updateMinimapField(position);
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
    return m_storage ? *m_storage : *TWorldMapStorage::ensureStorage();
}

void TWorldmapProtocolMessageHandler::moveKnownArea(const Position& center)
{
    map().setStoredVolume(center);
    effects::TEffectStorage::instance().removeOutside(
        [](const Position& field) { return game::isAwareOf({ field.x, field.y, field.z }); });
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

void TWorldmapProtocolMessageHandler::applyArea(const google::protobuf::RepeatedPtrField<ProtocolMapField>& fields)
{
    // Fields the server skipped keep what they hold.
    for (const ProtocolMapField& field : fields)
        applyField(field);
}

void TWorldmapProtocolMessageHandler::handleFullMapMessage(const GameserverMessageFullMap& message)
{
    if (game::isSceneFrozen())
        return;
    const Position position = positionOf(message.coordinate());
    const bool hasPlayer = game::hasLocalPlayer();
    const Position oldPosition = hasPlayer ? game::localPlayerPosition() : Position();
    // The session's first map also places the local player.
    if (!m_mapKnown && hasPlayer)
        map().sessionState().setLocalPlayerPosition(position);
    // The view centres on the map's field, its top-left subfield.
    if (m_viewport)
        m_viewport->setCenter(TWorldMapSubfieldCoordinate::fromField(TWorldMapCoordinate(position), 0, 0));
    moveKnownArea(position);
    // Every creature counts as off the map until the new area places it on a field.
    if (creatures::TCreatureStorage* creatures = creatures::TCreatureStorage::instance())
        creatures->clearPlacedOnFieldFlags();
    // A jump re-places the view: the step in flight ends before the new map arrives.
    if (hasPlayer && oldPosition.isValid() && !game::isOneStep(oldPosition, position))
        game::localPlayerHandleTeleport();
    applyArea(message.fields());
    game::updateMapLatency();
    if (!m_mapKnown) {
        schedule::post([] { game::notify("onMapKnown"); });
        m_mapKnown = true;
    }
    schedule::post([] { game::notify("onMapDescription"); });
    game::notify("onTeleport", game::localPlayerId(), position, oldPosition);
}

void TWorldmapProtocolMessageHandler::handleSlice(const google::protobuf::RepeatedPtrField<ProtocolMapField>& fields,
                                                  const Position& center)
{
    if (game::isSceneFrozen())
        return;
    moveKnownArea(center);
    applyArea(fields);
    game::updateMapLatency();
}

void TWorldmapProtocolMessageHandler::handleTopRowMessage(const GameserverMessageTopRow& message)
{
    handleSlice(message.fields(), positionOf(message.coordinate()));
}

void TWorldmapProtocolMessageHandler::handleRightColumnMessage(const GameserverMessageRightColumn& message)
{
    handleSlice(message.fields(), positionOf(message.coordinate()));
}

void TWorldmapProtocolMessageHandler::handleBottomRowMessage(const GameserverMessageBottomRow& message)
{
    handleSlice(message.fields(), positionOf(message.coordinate()));
}

void TWorldmapProtocolMessageHandler::handleLeftColumnMessage(const GameserverMessageLeftColumn& message)
{
    handleSlice(message.fields(), positionOf(message.coordinate()));
}

void TWorldmapProtocolMessageHandler::changeFloor(const google::protobuf::RepeatedPtrField<ProtocolMapField>& fields,
                                                  const Position& center, const Position& from)
{
    // The step in flight ends; the new floors land around the moved view.
    if (game::hasLocalPlayer())
        game::localPlayerHandleTeleport();
    moveKnownArea(center);
    applyArea(fields);
    game::notify("onTeleport", game::localPlayerId(), center, from);
}

void TWorldmapProtocolMessageHandler::handleTopFloorMessage(const GameserverMessageTopFloor& message)
{
    const Position center = positionOf(message.coordinate());
    changeFloor(message.fields(), center, Position(center.x - 1, center.y - 1, center.z));
}

void TWorldmapProtocolMessageHandler::handleBottomFloorMessage(const GameserverMessageBottomFloor& message)
{
    const Position center = positionOf(message.coordinate());
    changeFloor(message.fields(), center, Position(center.x + 1, center.y + 1, center.z));
}

void TWorldmapProtocolMessageHandler::handleFieldDataMessage(const GameserverMessageFieldData& message)
{
    if (!game::isSceneFrozen())
        applyField(message.field());
}

void TWorldmapProtocolMessageHandler::handleCreateOnMapMessage(const GameserverMessageCreateOnMap& message)
{
    if (game::isSceneFrozen())
        return;
    // Stack position 0xff puts the object where its kind sorts on the stack.
    const Position at = positionOf(message.coordinate());
    if (const appearances::TAppearanceInstancePtr instance = applyObject(message.object(), at)) {
        map().insertObjectAt(instance, at, static_cast<int>(message.stackpos() & 0xff));
        updateMinimapField(at);
    }
}

void TWorldmapProtocolMessageHandler::handleChangeOnMapMessage(const GameserverMessageChangeOnMap& message)
{
    if (game::isSceneFrozen())
        return;
    if (message.has_creature_id()) {
        // CIP: the id form only updates the creature; its field entry stays.
        if (message.object().has_creature())
            (void)creatures::applyProtocolCreature(message.object().creature());
        return;
    }
    // The object at the stack position leaves the field and the new one takes
    // its place (a creature's data is applied either way).
    const Position at = positionOf(message.coordinate());
    if (const appearances::TAppearanceInstancePtr instance = applyObject(message.object(), at))
        map().replaceObjectAt(instance, at, static_cast<int>(message.stackpos() & 0xff));
    // The minimap records the field whether or not an object was placed.
    updateMinimapField(at);
}

void TWorldmapProtocolMessageHandler::handleDeleteOnMapMessage(const GameserverMessageDeleteOnMap& message)
{
    if (game::isSceneFrozen())
        return;
    // The creature-id form leaves the creature where it is drawn: it only
    // stops counting as placed on a field.
    if (message.has_creature_id()) {
        if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance())
            storage->removeFromField(message.creature_id());
        return;
    }
    // The field drops the object at the stack position, then the minimap
    // records the field.
    const Position at = positionOf(message.coordinate());
    (void)map().removeObjectAt(at, static_cast<int>(message.stackpos() & 0xff));
    updateMinimapField(at);
}

void TWorldmapProtocolMessageHandler::handleAmbientLightMessage(const GameserverMessageAmbientLight& AmbientLightMessage)
{
    const auto intensity = static_cast<uint8_t>(AmbientLightMessage.intensity());
    const auto color = static_cast<uint8_t>(AmbientLightMessage.color());
    tibia::game::applyWorldLight(intensity, color);
    if (TAmbientLightStorage* ambient = TAmbientLightStorage::instance())
        ambient->setLight(TAmbientLight(intensity, color));
}

void TWorldmapProtocolMessageHandler::handleTibiaTimeMessage(const GameserverMessageTibiaTime& TibiaTimeMessage)
{
    const auto hour = static_cast<uint8_t>(TibiaTimeMessage.hour());
    const auto minute = static_cast<uint8_t>(TibiaTimeMessage.minute());
    if (TAmbientLightStorage* ambient = TAmbientLightStorage::instance())
        ambient->setNormalizedTibiaTimeOfDay(static_cast<float>(TibiaTimeMessage.hour()) / 24.0f
                                             + static_cast<float>(TibiaTimeMessage.minute()) / 1440.0f);
    tibia::world::setWorldTime(hour, minute);
    tibia::game::notify("onChangeWorldTime", hour, minute);
}

TWorldmapProtocolMessageHandler::TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent)
    : cip::tibia::worldmap::TWorldmapProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

}
