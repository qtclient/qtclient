#include "playermovementgameactionhandler.h"

#include "gameaction.h"
#include "gameactionhandlers.h"
#include "inputciptypes.h"

#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;

}

TPlayerMovementGameActionHandler::TPlayerMovementGameActionHandler(QObject* parent)
    : cip::tibia::input::TPlayerMovementGameActionHandler(parent)
{
    setActionHandler(A::SET_PLAYER_MOVEMENT_INTENT, [](const TGameAction& action) {
        publish(makeGameAction(A::APPLY_AUTOCHASE_SETTING));
        publish(std::make_shared<TGameActionCreature>(A::STOP_FOLLOWING_CREATURE, 0));
        (void)TPlayerMovementIntent::target(action.position.x, action.position.y, action.position.z);
        tibia::game::localPlayerAutoWalk(action.position, false,
                                         static_cast<tibia::client::AutoWalkPathMode>(action.autoWalkPathMode),
                                         action.enterUnwalkableGoal);
        return true;
    });
    // Attacking or following a creature drops the walk goal.
    for (const A type : { A::ATTACK_CREATURE, A::FOLLOW_CREATURE }) {
        m_listeners.push_back(addActionListener(type, [](const IGameAction&) {
            tibia::game::localPlayerClearMovementGoal();
            return false;
        }));
    }
    // So does using or opening an object on the map, or using one with another
    // when either lies on the map (not in a container or the inventory).
    const auto onMap = [](const IGameAction& action) {
        const std::optional<TObjectGameActionContext> object = objectContextOf(action);
        return creatureIdOf(action) == 0 && object && object->position.isValid() && object->position.x != 0xffff;
    };
    for (const A type : { A::USE, A::OPEN, A::OPEN_IN_NEW_WINDOW }) {
        m_listeners.push_back(addActionListener(type, [onMap](const IGameAction& action) {
            if (onMap(action))
                tibia::game::localPlayerClearMovementGoal();
            return false;
        }));
    }
    m_listeners.push_back(addActionListener(A::USE_WITH, [onMap](const TGameAction& action) {
        if (action.target && (onMap(action) || onMap(*action.target)))
            tibia::game::localPlayerClearMovementGoal();
        return false;
    }));
}

TPlayerMovementGameActionHandler::~TPlayerMovementGameActionHandler()
{
    setActionHandler(A::SET_PLAYER_MOVEMENT_INTENT, nullptr);
    for (const int id : m_listeners)
        removeActionListener(id);
}

}
