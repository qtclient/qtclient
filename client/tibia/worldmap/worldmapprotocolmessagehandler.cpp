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

TWorldmapProtocolMessageHandler* g_liveWorldmapProtocolHandler = nullptr;

using namespace ::tibia::protobuf::protocol;

Position positionOf(const ProtocolCoordinate& c)
{
    return Position(static_cast<int32_t>(c.x()), static_cast<int32_t>(c.y()), static_cast<uint8_t>(c.z()));
}

template<typename Message>
const Message* extension(const GameserverMessage& shadow, const auto& id)
{
    return shadow.HasExtension(id) ? &shadow.GetExtension(id) : nullptr;
}

} // namespace

TWorldmapProtocolMessageHandler* TWorldmapProtocolMessageHandler::liveInstance()
{
    return g_liveWorldmapProtocolHandler;
}

void TWorldmapProtocolMessageHandler::setLiveInstance(TWorldmapProtocolMessageHandler* handler)
{
    g_liveWorldmapProtocolHandler = handler;
}

void TWorldmapProtocolMessageHandler::clearField(const Position& position)
{
    if (m_storage)
        m_storage->clearField(position);
    else
        TWorldMapStorage::ensureStorage()->clearField(position);
}

void TWorldmapProtocolMessageHandler::applyAppearanceToField(const appearances::TAppearanceInstancePtr& appearance,
                                                             const Position& position, const int stackPos)
{
    if (m_storage)
        m_storage->placeAppearance(appearance, position, stackPos);
    else
        TWorldMapStorage::ensureStorage()->placeAppearance(appearance, position, stackPos);
}

void TWorldmapProtocolMessageHandler::applyCreatureIdToField(const uint32_t creatureId, const Position& position,
                                                             const int stackPos)
{
    if (m_storage)
        m_storage->placeCreatureId(creatureId, position, stackPos);
    else
        TWorldMapStorage::ensureStorage()->placeCreatureId(creatureId, position, stackPos);
}

bool TWorldmapProtocolMessageHandler::applyObject(const ProtocolMapObject& object, const Position& position, const int stackPos)
{
    if (object.has_creature()) {
        // The creature's data is applied first; its field then holds it.
        if (const uint32_t id = creatures::applyProtocolCreature(object.creature()); id != 0) {
            applyCreatureIdToField(id, position, stackPos);
            return true;
        }
        return false;
    }
    if (object.has_item()) {
        if (const auto appearance = container::TInventorySlot::fromProtocolInventoryItem(object.item()).appearance) {
            applyAppearanceToField(appearance, position, stackPos);
            return true;
        }
    }
    return false;
}

void TWorldmapProtocolMessageHandler::applyField(const ProtocolMapField& field)
{
    const Position position = positionOf(field.coordinate());
    // The storage holds fields of its known area only.
    if (!game::isAwareOf({ position.x, position.y, position.z }))
        return;
    // The objects replace the field's content in their order; objects that
    // yield nothing are left out, and a field left without objects is dropped.
    clearField(position);
    int stackPos = 0;
    for (const ProtocolMapObject& object : field.objects()) {
        if (applyObject(object, position, stackPos))
            ++stackPos;
    }
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
    const Position at = positionOf(message.coordinate());
    if (applyObject(message.object(), at, static_cast<int>(message.stackpos())))
        updateMinimapField(at);
}

void TWorldmapProtocolMessageHandler::handleChangeOnMapMessage(const GameserverMessageChangeOnMap& message)
{
    if (game::isSceneFrozen())
        return;
    TWorldMapStorage* map = m_storage ? m_storage : TWorldMapStorage::ensureStorage();
    // A creature replacement updates the creature storage even when its field
    // slot is not found (a turn of a creature already moved on).
    const bool creatureReplacement = message.object().has_creature();
    const auto replace = [&](const Position& at, const int stackPos) { applyObject(message.object(), at, stackPos); };
    if (message.has_creature_id()) {
        // CIP: the id form only updates the creature; its field entry stays.
        if (creatureReplacement)
            (void)creatures::applyProtocolCreature(message.object().creature());
        return;
    }
    const Position at = positionOf(message.coordinate());
    const int stackPos = static_cast<int>(message.stackpos());
    const auto replaceAt = [&] {
        if (TWorldMapField* tile = fieldAt(at)) {
            if (const uint32_t stackCreatureId = tile->stackCreatureId(stackPos)) {
                if (map->removeCreatureFromFields(stackCreatureId, at)) {
                    replace(at, stackPos);
                    return;
                }
            }
            if (tile->removeThingAtStackPos(stackPos)) {
                replace(at, stackPos);
                return;
            }
        }
        // A turn still carries the creature's update when its field slot is not found.
        if (creatureReplacement)
            (void)creatures::applyProtocolCreature(message.object().creature());
    };
    replaceAt();
    // The minimap records the field whether or not the slot was found.
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
    // The field drops the object at the stack position (a creature leaves the
    // field), then the minimap records the field.
    TWorldMapStorage* map = m_storage ? m_storage : TWorldMapStorage::ensureStorage();
    const Position at = positionOf(message.coordinate());
    const int stackPos = static_cast<int>(message.stackpos());
    if (TWorldMapField* tile = fieldAt(at)) {
        const uint32_t stackCreatureId = tile->stackCreatureId(stackPos);
        if (!stackCreatureId || !map->removeCreatureFromFields(stackCreatureId, at))
            (void)tile->removeThingAtStackPos(stackPos);
    }
    updateMinimapField(at);
}

void TWorldmapProtocolMessageHandler::handleAmbientLightMessage(const GameserverMessageAmbientLight& AmbientLightMessage)
{
    const auto intensity = static_cast<uint8_t>(AmbientLightMessage.intensity());
    const auto color = static_cast<uint8_t>(AmbientLightMessage.color());
    tibia::game::applyWorldLight(intensity, color);
    if (TAmbientLightStorage* ambient = TAmbientLightStorage::instance()) {
        // The 6x6x6 palette; indices past 215 are black.
        QColor rgb = QColor::fromRgb(0, 0, 0, 255);
        if (color <= 215)
            rgb = QColor::fromRgb(color / 36 * 51, color / 6 % 6 * 51, color % 6 * 51, 255);
        ambient->setLight({ intensity, rgb });
    }
}

void TWorldmapProtocolMessageHandler::onServerMessageEnqueued(int opcode)
{
    // A new session (LoginSuccess) waits for its first full map again.
    if (opcode == 23)
        m_mapKnown = false;
    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;
    switch (opcode) {
    case 100:
        if (const auto* m = extension<GameserverMessageFullMap>(*shadow, GameserverMessageFullMap::gameserver_message_full_map))
            handleFullMapMessage(*m);
        break;
    case 101:
        if (const auto* m = extension<GameserverMessageTopRow>(*shadow, GameserverMessageTopRow::gameserver_message_top_row))
            handleTopRowMessage(*m);
        break;
    case 102:
        if (const auto* m = extension<GameserverMessageRightColumn>(*shadow, GameserverMessageRightColumn::gameserver_message_right_column))
            handleRightColumnMessage(*m);
        break;
    case 103:
        if (const auto* m = extension<GameserverMessageBottomRow>(*shadow, GameserverMessageBottomRow::gameserver_message_bottom_row))
            handleBottomRowMessage(*m);
        break;
    case 104:
        if (const auto* m = extension<GameserverMessageLeftColumn>(*shadow, GameserverMessageLeftColumn::gameserver_message_left_column))
            handleLeftColumnMessage(*m);
        break;
    case 105:
        if (const auto* m = extension<GameserverMessageFieldData>(*shadow, GameserverMessageFieldData::gameserver_message_field_data))
            handleFieldDataMessage(*m);
        break;
    case 106:
        if (const auto* m = extension<GameserverMessageCreateOnMap>(*shadow, GameserverMessageCreateOnMap::gameserver_message_create_on_map))
            handleCreateOnMapMessage(*m);
        break;
    case 107:
        if (const auto* m = extension<GameserverMessageChangeOnMap>(*shadow, GameserverMessageChangeOnMap::gameserver_message_change_on_map))
            handleChangeOnMapMessage(*m);
        break;
    case 108:
        if (const auto* m = extension<GameserverMessageDeleteOnMap>(*shadow, GameserverMessageDeleteOnMap::gameserver_message_delete_on_map))
            handleDeleteOnMapMessage(*m);
        break;
    case 130:
        if (const auto* m = extension<GameserverMessageAmbientLight>(*shadow, GameserverMessageAmbientLight::gameserver_message_ambient_light))
            handleAmbientLightMessage(*m);
        break;
    case 190:
        if (const auto* m = extension<GameserverMessageTopFloor>(*shadow, GameserverMessageTopFloor::gameserver_message_top_floor))
            handleTopFloorMessage(*m);
        break;
    case 191:
        if (const auto* m = extension<GameserverMessageBottomFloor>(*shadow, GameserverMessageBottomFloor::gameserver_message_bottom_floor))
            handleBottomFloorMessage(*m);
        break;
    case 239:
        if (const auto* m = extension<GameserverMessageTibiaTime>(*shadow, GameserverMessageTibiaTime::gameserver_message_tibia_time)) {
            const auto hour = static_cast<uint8_t>(m->hour());
            const auto minute = static_cast<uint8_t>(m->minute());
            tibia::world::setWorldTime(hour, minute);
            tibia::game::notify("onChangeWorldTime", hour, minute);
        }
        break;
    default:
        break;
    }
}

TWorldmapProtocolMessageHandler::TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent)
    : cip::tibia::worldmap::TWorldmapProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

}
