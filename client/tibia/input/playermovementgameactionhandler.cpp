#include "playermovementgameactionhandler.h"

#include "gameaction.h"
#include "gameactionhandlers.h"
#include "inputciptypes.h"

#include "playermovementintenthandler.h"

#include <tibia/game/gamenotify.h>

namespace tibia::input {

namespace {

using A = EGameAction;

}

TPlayerMovementGameActionHandler::TPlayerMovementGameActionHandler(std::shared_ptr<TPlayerMovementIntentHandler> pPlayerMovementIntentHandler)
    : cip::tibia::input::TPlayerMovementGameActionHandler(nullptr)
    , m_pPlayerMovementIntentHandler(std::move(pPlayerMovementIntentHandler))
{
}

TPlayerMovementGameActionHandler::~TPlayerMovementGameActionHandler() = default;

void TPlayerMovementGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case A::USE:
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
        handleObjectGameAction(*pGameAction);
        break;
    case A::USE_WITH:
        if (const auto* use = gameActionAs<TGameActionUseWith>(pGameAction))
            handleUseWithGameAction(*use);
        break;
    case A::ATTACK_CREATURE:
    case A::FOLLOW_CREATURE:
        stopMovement();
        break;
    case A::SET_PLAYER_MOVEMENT_INTENT: {
        const auto* action = gameActionAs<TGameActionPlayerMovementIntent>(pGameAction);
        if (!action || !action->intent())
            break;
        const TMovementIntentKind kind = action->intentKind();
        if (kind == TMovementIntentKind::Direction || kind == TMovementIntentKind::Target) {
            Q_EMIT publishGameAction(makeGameAction(A::APPLY_AUTOCHASE_SETTING));
            Q_EMIT publishGameAction(std::make_shared<TGameActionCreature>(A::STOP_FOLLOWING_CREATURE, 0));
        }
        // qtclient pack notify: the player walks by hand (a click), e.g. to stop a pack's travel.
        if (kind == TMovementIntentKind::Target)
            tibia::game::notify("onWalk", tibia::client::InvalidDirection);
        m_pPlayerMovementIntentHandler->handleMovementIntent(action->intent());
        break;
    }
    case A::TRADE_ITEM:
        if (const auto* trade = gameActionAs<TGameActionPlayerTradeItem>(pGameAction); trade && trade->targetCreatureId() != 0)
            stopMovement();
        break;
    default:
        // qtclient: STOP_ATTACKING_FOLLOWING_WALKING (stop the walk, sendCancel) is
        // handled by TUnportedGameActionHandler through tibia::game::stop.
        break;
    }
}

void TPlayerMovementGameActionHandler::handleObjectGameAction(const IGameAction& action)
{
    const std::optional<TObjectGameActionContext> object = objectContextOf(action);
    if (object && object->locationType() == EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION)
        stopMovement();
}

void TPlayerMovementGameActionHandler::handleUseWithGameAction(const TGameActionUseWith& action)
{
    if (!action.hasTarget())
        return;
    if (action.context().locationType() == EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION
        || action.target()->locationType() == EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION)
        stopMovement();
}

void TPlayerMovementGameActionHandler::stopMovement()
{
    m_pPlayerMovementIntentHandler->clearPath();
}
}
