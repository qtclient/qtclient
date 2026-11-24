#include "input/keyboardinputtogameactionconverter.h"

#include "input/playermovementinputinterpreter.h"

#include <client/const.h>

#include <utility>

namespace tibia::input {

TKeyboardInputToGameActionConverter::TKeyboardInputToGameActionConverter(
    std::shared_ptr<const TPlayerMovementInputInterpreter> pPlayerMovementInputInterpreter)
    : cip::tibia::input::TKeyboardInputToGameActionConverter(nullptr)
    , m_pPlayerMovementInputInterpreter(std::move(pPlayerMovementInputInterpreter))
    , m_pMovementIntent(std::make_shared<TMovementIntentStop>())
{
}

TKeyboardInputToGameActionConverter::~TKeyboardInputToGameActionConverter() = default;

TMovementIntentPtr TKeyboardInputToGameActionConverter::updateMovementIntent()
{
    using namespace tibia::client;
    const TPlayerMovementInputInterpreter& input = *m_pPlayerMovementInputInterpreter;
    // qtclient's direction intents carry tibia::client::Direction, rotations the look direction (N, E, S, W).
    int walk = -1;
    if (input.walksNorth())
        walk = North;
    else if (input.walksSouth())
        walk = South;
    else if (input.walksWest())
        walk = West;
    else if (input.walksEast())
        walk = East;
    else if (input.walksNorthWest())
        walk = NorthWest;
    else if (input.walksNorthEast())
        walk = NorthEast;
    else if (input.walksSouthWest())
        walk = SouthWest;
    else if (input.walksSouthEast())
        walk = SouthEast;
    if (walk >= 0) {
        m_pMovementIntent = std::make_shared<TMovementIntentDirection>(walk);
        return m_pMovementIntent;
    }
    if (input.rotatesNorth())
        m_pMovementIntent = std::make_shared<TMovementIntentRotate>(North);
    else if (input.rotatesEast())
        m_pMovementIntent = std::make_shared<TMovementIntentRotate>(East);
    else if (input.rotatesWest())
        m_pMovementIntent = std::make_shared<TMovementIntentRotate>(West);
    else if (input.rotatesSouth())
        m_pMovementIntent = std::make_shared<TMovementIntentRotate>(South);
    else
        m_pMovementIntent = std::make_shared<TMovementIntentStop>();
    return m_pMovementIntent;
}

TMovementIntentPtr TKeyboardInputToGameActionConverter::changedMovementIntent()
{
    const TMovementIntentPtr previous = movementIntent();
    TMovementIntentPtr intent = updateMovementIntent();
    if (*previous == *intent)
        return nullptr;
    return intent;
}

}
