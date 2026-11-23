#include "worldmap/worldmapgameactionhandler.h"

#include "creatures/creaturestorage.h"
#include "input/gameactioncontext.h"
#include "input/gameactionhandlers.h"
#include "protocol/protocolcodes.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <cstdint>

namespace tibia::worldmap {

namespace {

using A = input::EGameAction;
using C = input::EGameActionContextType;

/// The protocol coordinate of a classic position.
void setCoordinate(::tibia::protobuf::protocol::ProtocolCoordinate* coordinate, const Position& position)
{
    coordinate->set_x(position.x);
    coordinate->set_y(position.y);
    coordinate->set_z(position.z);
}

/// The classic message names a moved creature by its field and stack position
/// (CipSoft's message carries the action's own location instead).
bool creatureSource(const uint32_t creatureId, Position* from, int* stackPosition)
{
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
    const std::shared_ptr<creatures::TCreature> creature = storage ? storage->creatureAt(creatureId) : nullptr;
    if (!creature || !creature->position().isValid())
        return false;
    *from = creature->position();
    *stackPosition = -1;
    if (TWorldMapStorage* map = TWorldMapStorage::instance()) {
        if (TWorldMapField* field = map->fieldAt(*from))
            *stackPosition = field->creatureStackPos(creatureId);
    }
    return *stackPosition >= 0;
}

}

TWorldMapGameActionHandler::TWorldMapGameActionHandler(QObject* parent)
    : cip::tibia::worldmap::TWorldMapGameActionHandler(parent)
{
}

void TWorldMapGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    if (pGameAction->gameActionType() == A::MOVE_OBJECT)
        handleMoveObjectGameAction(std::dynamic_pointer_cast<const input::TGameActionMoveObject>(pGameAction));
}

void TWorldMapGameActionHandler::handleMoveObjectGameAction(
    const std::shared_ptr<const input::TGameActionMoveObject>& pGameActionMoveObject)
{
    if (!pGameActionMoveObject || !pGameActionMoveObject->destination())
        return;
    const input::TGameActionMoveObject& move = *pGameActionMoveObject;
    const Position& to = *move.destination();
    const input::TGameActionContextPtr destination = input::locationContextFromPosition(to, -1);
    if (!destination || destination->gameActionContextType() != C::LOCATION_WORLD_MAP_COORDINATE)
        return;

    // Host concern (classic message): the source field, stack position and
    // object id; a creature is object 99 on its own field.
    Position from = move.position();
    int stackPosition = move.stackPosition();
    uint16_t objectId = move.objectClientId();
    if (const uint32_t creatureId = input::creatureOfObject(move.context())) {
        if (!creatureSource(creatureId, &from, &stackPosition))
            return;
        objectId = static_cast<uint16_t>(Proto::Creature);
    }
    if (!from.isValid() || objectId == 0 || stackPosition < 0)
        return;

    ::tibia::protobuf::protocol::GameclientMessageMoveObject message;
    message.set_count(static_cast<uint32_t>(input::moveObjectCount(move)));
    setCoordinate(message.mutable_to_position(), to);
    setCoordinate(message.mutable_from_position(), from);
    message.set_from_stack_position(static_cast<uint32_t>(stackPosition));
    message.set_object_id(objectId);
    // An object moved onto the field it lies on is not sent.
    if (from == to)
        return;
    Q_EMIT sendMoveObject(message);
}

}
