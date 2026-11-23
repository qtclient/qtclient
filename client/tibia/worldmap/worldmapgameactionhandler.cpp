#include "worldmap/worldmapgameactionhandler.h"

#include "input/gameactioncontext.h"
#include "input/gameactionhandlers.h"

#include <cstdint>

namespace tibia::worldmap {

namespace {

using A = input::EGameAction;
using C = input::EGameActionContextType;

}

TWorldMapGameActionHandler::TWorldMapGameActionHandler(
    std::shared_ptr<::tibia::protocol::TGameActionContextHelper> gameActionContextHelper, QObject* parent)
    : cip::tibia::worldmap::TWorldMapGameActionHandler(parent)
    , m_gameActionContextHelper(std::move(gameActionContextHelper))
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
    if (!pGameActionMoveObject || !pGameActionMoveObject->destination() || !m_gameActionContextHelper)
        return;
    const input::TGameActionMoveObject& move = *pGameActionMoveObject;
    const input::TGameActionContextPtr destination = input::locationContextFromPosition(*move.destination(), -1);
    if (!destination || destination->gameActionContextType() != C::LOCATION_WORLD_MAP_COORDINATE)
        return;

    ::tibia::protobuf::protocol::GameclientMessageMoveObject message;
    message.set_count(static_cast<uint32_t>(input::moveObjectCount(move)));
    m_gameActionContextHelper->fillObjectPosition(destination, message.mutable_destination_object_position());
    if (move.location())
        m_gameActionContextHelper->fillObjectPosition(move.location(), message.mutable_source_object_position());
    if (move.object())
        m_gameActionContextHelper->fillObjectIdentifier(move.object(), message.mutable_object_identifier());
    // An object moved onto the map field it lies on is not sent.
    const auto& source = message.source_object_position();
    const auto& target = message.destination_object_position();
    if (source.has_worldmap_position() && target.has_worldmap_position()
        && source.worldmap_position().coordinate().SerializeAsString()
            == target.worldmap_position().coordinate().SerializeAsString())
        return;
    Q_EMIT sendMoveObject(message);
}

}
