#pragma once

#include "gameaction.h"

#include <cip_gen.h>

#include <vector>

namespace tibia {
namespace input {

/// CIP TPlayerMovementGameActionHandler: SET_PLAYER_MOVEMENT_INTENT hands the
/// intent to the movement intent handler (a walk or a held direction first
/// applies the auto-chase setting and stops following); attacking or following
/// a creature, trading an object with a player and using or opening an object
/// on a map field drop the walk path.
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
    /// CIP v15. Connected to the router's publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    /// CIP v12: a use or open of an object on a map field stops the walk.
    void handleObjectGameAction(const IGameAction& action);
    /// CIP v13: a use with whose object or target lies on a map field stops the walk.
    void handleUseWithGameAction(const TGameActionUseWith& action);
    /// CIP v14: the movement intent handler drops its path.
    void stopMovement();
};

}
}
