#include "playermovementgameactionhandler.h"

#include "gameaction.h"
#include "gameactionhandlers.h"

#include <client/const.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

namespace tibia::input {

namespace {

using A = EGameAction;

}

TPlayerMovementGameActionHandler::TPlayerMovementGameActionHandler(QObject* parent)
    : cip::tibia::input::TPlayerMovementGameActionHandler(parent)
{
    setActionHandler(A::SET_PLAYER_MOVEMENT_INTENT, [](const TGameAction& action) {
        // CIP TPlayerMovementGameActionHandler (0x100e79d60) also stops
        // following, which Game::autoWalk does itself.
        publish(makeAction(A::APPLY_AUTOCHASE_SETTING));
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer()) {
            const auto mode = static_cast<Otc::AutoWalkPathMode>(action.autoWalkPathMode);
            player->autoWalk(action.position, false, mode);
        }
        return true;
    });
}

TPlayerMovementGameActionHandler::~TPlayerMovementGameActionHandler()
{
    setActionHandler(A::SET_PLAYER_MOVEMENT_INTENT, nullptr);
}

}
