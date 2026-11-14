#include "worldmap/worldmapprotocolmessagehandler.h"

#include "protocol/protocolciptypes.h"
#include "worldmap/worldmapstorage.h"

#include "container/containerciptypes.h"
#include "creatures/creatureprotocolapply.h"
#include "creatures/creaturestorage.h"
#include "game/localplayersessionthing.h"
#include "worldmap/worldmapfield.h"

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

void TWorldmapProtocolMessageHandler::applyObject(const ProtocolMapObject& object, const Position& position, const int stackPos)
{
    if (object.has_creature()) {
        if (const uint32_t id = creatures::applyProtocolCreature(object.creature()); id != 0)
            applyCreatureIdToField(id, position, stackPos);
        return;
    }
    if (object.has_item()) {
        if (const auto appearance = container::TInventorySlot::fromProtocolInventoryItem(object.item()).appearance)
            applyAppearanceToField(appearance, position, stackPos);
    }
}

void TWorldmapProtocolMessageHandler::applyField(const ProtocolMapField& field)
{
    const Position position = positionOf(field.coordinate());
    clearField(position);
    int stackPos = 0;
    for (const ProtocolMapObject& object : field.objects())
        applyObject(object, position, stackPos++);
}

void TWorldmapProtocolMessageHandler::centerOn(const Position& position)
{
    if (!m_mapKnown) {
        if (const LocalPlayerSessionThingPtr player = game::localPlayer())
            player->setPosition(position);
    }
    game::setCentralPosition({ position.x, position.y, position.z });
}

void TWorldmapProtocolMessageHandler::handleFullMapMessage(const GameserverMessageFullMap& message)
{
    if (game::isSceneFrozen())
        return;
    const Position position = positionOf(message.coordinate());
    const LocalPlayerSessionThingPtr player = game::localPlayer();
    const Position oldPosition = player ? player->getPosition() : Position();
    centerOn(position);
    // A jump re-places the view: the step in flight ends before the new map arrives.
    if (player && oldPosition.isValid() && !game::isOneStep(oldPosition, position))
        player->handleTeleport();
    for (const ProtocolMapField& field : message.fields())
        applyField(field);
    game::updateMapLatency();
    notifyWindowChanged(true);
    if (!m_mapKnown) {
        schedule::post([] { game::notify("onMapKnown"); });
        m_mapKnown = true;
    }
    schedule::post([] { game::notify("onMapDescription"); });
    game::notify("onTeleport", player, position, oldPosition);
}

void TWorldmapProtocolMessageHandler::handleFloorDescription(const google::protobuf::RepeatedPtrField<ProtocolMapField>& fields,
                                                             const Position& center, const bool viewFloor)
{
    if (game::isSceneFrozen())
        return;
    if (viewFloor) {
        const LocalPlayerSessionThingPtr player = game::localPlayer();
        const Position oldPosition = player ? player->getPosition() : Position();
        centerOn(center);
        if (!m_mapKnown) {
            schedule::post([] { game::notify("onMapKnown"); });
            m_mapKnown = true;
        }
        schedule::post([] { game::notify("onMapDescription"); });
        if (player && oldPosition.isValid() && !game::isOneStep(oldPosition, center))
            player->handleTeleport();
        game::notify("onTeleport", player, center, oldPosition);
    }
    for (const ProtocolMapField& field : fields)
        applyField(field);
    notifyWindowChanged(viewFloor);
    game::updateMapLatency();
}

void TWorldmapProtocolMessageHandler::handleSlice(const google::protobuf::RepeatedPtrField<ProtocolMapField>& fields,
                                                  const Position& center)
{
    if (game::isSceneFrozen())
        return;
    for (const ProtocolMapField& field : fields)
        applyField(field);
    game::updateMapLatency();
    game::setCentralPosition({ center.x, center.y, center.z });
    notifyWindowChanged(false);
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

namespace {

/// A floor change: the step in flight ends, the new floors land, the view re-centres.
void changeFloor(TWorldmapProtocolMessageHandler& handler, const google::protobuf::RepeatedPtrField<ProtocolMapField>& fields,
                 const Position& center, const Position& from)
{
    const LocalPlayerSessionThingPtr player = game::localPlayer();
    if (player)
        player->handleTeleport();
    for (const ProtocolMapField& field : fields)
        handler.applyField(field);
    game::setCentralPosition({ center.x, center.y, center.z });
    notifyWindowChanged(true);
    game::notify("onTeleport", player, center, from);
}

}

void TWorldmapProtocolMessageHandler::handleTopFloorMessage(const GameserverMessageTopFloor& message)
{
    const Position center = positionOf(message.coordinate());
    changeFloor(*this, message.fields(), center, Position(center.x - 1, center.y - 1, center.z));
}

void TWorldmapProtocolMessageHandler::handleBottomFloorMessage(const GameserverMessageBottomFloor& message)
{
    const Position center = positionOf(message.coordinate());
    changeFloor(*this, message.fields(), center, Position(center.x + 1, center.y + 1, center.z));
}

void TWorldmapProtocolMessageHandler::handleFieldDataMessage(const GameserverMessageFieldData& message)
{
    if (!game::isSceneFrozen())
        applyField(message.field());
}

void TWorldmapProtocolMessageHandler::handleCreateOnMapMessage(const GameserverMessageCreateOnMap& message)
{
    if (!game::isSceneFrozen())
        applyObject(message.object(), positionOf(message.coordinate()), static_cast<int>(message.stackpos()));
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
        // The id form is always a creature, found on its stored field.
        const uint32_t id = message.creature_id();
        Position at;
        if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance()) {
            if (const auto row = storage->creatureAt(id))
                at = row->position();
        }
        if (TWorldMapField* tile = at.isValid() ? fieldAt(at) : nullptr) {
            const int stackPos = tile->creatureStackPos(id);
            if (stackPos >= 0) {
                map->removeCreatureFromFields(id, at);
                replace(tile->getPosition(), stackPos);
                return;
            }
        }
        if (creatureReplacement)
            (void)creatures::applyProtocolCreature(message.object().creature());
        return;
    }
    const Position at = positionOf(message.coordinate());
    const int stackPos = static_cast<int>(message.stackpos());
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
}

void TWorldmapProtocolMessageHandler::handleDeleteOnMapMessage(const GameserverMessageDeleteOnMap& message)
{
    if (game::isSceneFrozen())
        return;
    TWorldMapStorage* map = m_storage ? m_storage : TWorldMapStorage::ensureStorage();
    // A deleted creature keeps its identity (name, outfit); only its field slot goes.
    if (message.has_creature_id()) {
        const uint32_t id = message.creature_id();
        if (map->removeCreatureFromFields(id)) {
            if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance())
                storage->leaveMap(id);
        }
        return;
    }
    const Position at = positionOf(message.coordinate());
    const int stackPos = static_cast<int>(message.stackpos());
    if (TWorldMapField* tile = fieldAt(at)) {
        if (const uint32_t stackCreatureId = tile->stackCreatureId(stackPos)) {
            if (map->removeCreatureFromFields(stackCreatureId, at)) {
                if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance())
                    storage->leaveMap(stackCreatureId);
                return;
            }
        }
        (void)tile->removeThingAtStackPos(stackPos);
    }
}

void TWorldmapProtocolMessageHandler::handleAmbientLightMessage(const GameserverMessageAmbientLight& AmbientLightMessage)
{
    const auto intensity = static_cast<uint8_t>(AmbientLightMessage.intensity());
    const auto color = static_cast<uint8_t>(AmbientLightMessage.color());
    tibia::game::applyWorldLight(intensity, color);
}

void TWorldmapProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                              bool applyClassic)
{
    // Peeled opcodes: native H applies when the classic bridge will not
    // (applyClassic=false on the structured TR→Q path). Measure-fail still
    // classic-applies first then enqueues with applyClassic=false — re-applying
    // the same AmbientLight values here is idempotent (no spurious notify).
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

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

void TWorldmapProtocolMessageHandler::onWindowChanged(const bool fullDescription)
{
    if (m_storage && fullDescription)
        m_storage->notifyStoredVolumeChanged();
}

void TWorldmapProtocolMessageHandler::onTileChanged(const Position& position)
{
    if (m_storage)
        m_storage->notifyCoordinateChanged(position);
}

}
