#pragma once

#include <cip_gen.h>

namespace tibia::input {

/// CIP TPlayerMovementGameActionHandler (0x100e79d60): SET_PLAYER_MOVEMENT_INTENT
/// stops following (via Game::autoWalk) and walks to the action's coordinate.
class TPlayerMovementGameActionHandler final : public cip::tibia::input::TPlayerMovementGameActionHandler
{
    Q_OBJECT
public:
    explicit TPlayerMovementGameActionHandler(QObject* parent = nullptr);
    ~TPlayerMovementGameActionHandler() override;
    TPlayerMovementGameActionHandler(const TPlayerMovementGameActionHandler&) = delete;
    TPlayerMovementGameActionHandler& operator=(const TPlayerMovementGameActionHandler&) = delete;
};

}
