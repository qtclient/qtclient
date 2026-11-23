#pragma once

#include "input/gameaction.h"
#include "protocol/gameactioncontexthelper.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <QObject>

#include <memory>

namespace tibia {
namespace worldmap {

/// CIP worldmap::TWorldMapGameActionHandler: the moves of an object onto a
/// world map field. Each becomes a MoveObject client message whose positions
/// and object the session's TGameActionContextHelper writes, handed to the
/// session through sendMoveObject; a move onto the field the object lies on
/// sends nothing. Moves into the inventory or a container are the container
/// handler's.
class TWorldMapGameActionHandler final : public cip::tibia::worldmap::TWorldMapGameActionHandler
{
    Q_OBJECT
public:
    explicit TWorldMapGameActionHandler(std::shared_ptr<::tibia::protocol::TGameActionContextHelper> gameActionContextHelper,
                                        QObject* parent = nullptr);

Q_SIGNALS:
    void sendMoveObject(const ::tibia::protobuf::protocol::GameclientMessageMoveObject& MoveObjectMessage);

public Q_SLOTS:
    /// Hands a move to handleMoveObjectGameAction; connected to the router's
    /// publishGameActionInternal.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    /// A move whose destination is a world map field.
    void handleMoveObjectGameAction(const std::shared_ptr<const ::tibia::input::TGameActionMoveObject>& pGameActionMoveObject);

private:
    std::shared_ptr<::tibia::protocol::TGameActionContextHelper> m_gameActionContextHelper;
};

}
}
