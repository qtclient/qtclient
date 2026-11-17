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
}

TPlayerMovementGameActionHandler::~TPlayerMovementGameActionHandler() = default;

void TPlayerMovementGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction)
        return;
    // Using or opening an object on the map, or using one with another when
    // either lies on the map (not in a container or the inventory).
    const auto onMap = [](const IGameAction& action) {
        const std::optional<TObjectGameActionContext> object = objectContextOf(action);
        return creatureIdOf(action) == 0 && object && object->position.isValid() && object->position.x != 0xffff;
    };
    switch (pGameAction->gameActionType()) {
    case A::SET_PLAYER_MOVEMENT_INTENT:
        if (const auto* walk = gameActionAs<TGameActionPlayerMovementIntent>(pGameAction)) {
            publish(makeGameAction(A::APPLY_AUTOCHASE_SETTING));
            publish(std::make_shared<TGameActionCreature>(A::STOP_FOLLOWING_CREATURE, 0));
            tibia::game::localPlayerAutoWalk(walk->target(), false,
                                             static_cast<tibia::client::AutoWalkPathMode>(walk->autoWalkPathMode()),
                                             walk->enterUnwalkableGoal());
        }
        break;
    case A::ATTACK_CREATURE:
    case A::FOLLOW_CREATURE:
        tibia::game::localPlayerClearMovementGoal();
        break;
    case A::USE:
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
        if (onMap(*pGameAction))
            tibia::game::localPlayerClearMovementGoal();
        break;
    case A::USE_WITH:
        if (const TGameActionPtr action = unportedGameAction(pGameAction);
            action && action->target && (onMap(*action) || onMap(*action->target)))
            tibia::game::localPlayerClearMovementGoal();
        break;
    default:
        break;
    }
}
}
