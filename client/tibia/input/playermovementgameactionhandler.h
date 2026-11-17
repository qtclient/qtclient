#pragma once

#include "gameaction.h"

#include <cip_gen.h>

#include <vector>

namespace tibia {
namespace input {

/// CIP TPlayerMovementGameActionHandler: SET_PLAYER_MOVEMENT_INTENT applies the
/// auto-chase setting, stops following and walks to the action's coordinate
/// through the movement intent handler; attacking or following a creature
/// drops the walk goal.
class TPlayerMovementGameActionHandler final : public cip::tibia::input::TPlayerMovementGameActionHandler
{
    Q_OBJECT
public:
    explicit TPlayerMovementGameActionHandler(QObject* parent = nullptr);
    ~TPlayerMovementGameActionHandler() override;
    TPlayerMovementGameActionHandler(const TPlayerMovementGameActionHandler&) = delete;
    TPlayerMovementGameActionHandler& operator=(const TPlayerMovementGameActionHandler&) = delete;

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);

public Q_SLOTS:
    /// SET_PLAYER_MOVEMENT_INTENT walks; attacking, following, using or opening
    /// an object on the map drops the walk goal. Connected to the router's
    /// publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
};

}
}
