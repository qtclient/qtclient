#pragma once

#include "input/playermovementintent.h"

#include <cip_gen.h>

#include <memory>

namespace tibia {
namespace input {

class TPlayerMovementInputInterpreter;

/// Turns the walk keys the movement input interpreter reports into the
/// player's movement intent: the first walk direction held in the order
/// N, S, W, E, NW, NE, SW, SE; else the first turn in the order N, E, W, S;
/// else a stop. The intent starts as a stop.
class TKeyboardInputToGameActionConverter final : public cip::tibia::input::TKeyboardInputToGameActionConverter
{
    Q_OBJECT
public:
    explicit TKeyboardInputToGameActionConverter(std::shared_ptr<const TPlayerMovementInputInterpreter> pPlayerMovementInputInterpreter);
    ~TKeyboardInputToGameActionConverter() override;

    /// CIP v12: reads the interpreter and keeps the intent it gives.
    TMovementIntentPtr updateMovementIntent();
    /// CIP v13: the intent kept last.
    TMovementIntentPtr movementIntent() const { return m_pMovementIntent; }
    /// CIP v14: the new intent when it differs from the one kept before,
    /// otherwise null.
    TMovementIntentPtr changedMovementIntent();

private:
    /// +0x10
    std::shared_ptr<const TPlayerMovementInputInterpreter> m_pPlayerMovementInputInterpreter;
    /// +0x20: null from the constructor; no reader in the unit.
    std::shared_ptr<void> m_pUnused;
    /// +0x30
    TMovementIntentPtr m_pMovementIntent;
};

}
}
