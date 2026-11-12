#include "playermovementgameactionhandler.h"

#include "gameaction.h"
#include "gameactionhandlers.h"
#include "inputciptypes.h"

#include <client/const.h>

#include <QTimer>
#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;

/// CIP IntentHandler tick grain (same order as direction-hold RepeatDelayMs).
constexpr int TargetReplanTickMs = 50;
/// Floor between stalled Target replans so findPathAsync / GoPath are not
/// thrashed every tick. Matches qtclient's first autoWalk retry delay.
constexpr int TargetReplanIntervalMs = 250;

}

TPlayerMovementGameActionHandler* TPlayerMovementGameActionHandler::s_instance = nullptr;

TPlayerMovementGameActionHandler* TPlayerMovementGameActionHandler::instance()
{
    return s_instance;
}

TPlayerMovementGameActionHandler::TPlayerMovementGameActionHandler(QObject* parent)
    : cip::tibia::input::TPlayerMovementGameActionHandler(parent)
    , m_replanTimer(new QTimer(this))
{
    s_instance = this;
    m_replanTimer->setInterval(TargetReplanTickMs);
    connect(m_replanTimer, &QTimer::timeout, this, &TPlayerMovementGameActionHandler::onReplanTick);

    setActionHandler(A::SET_PLAYER_MOVEMENT_INTENT, [this](const TGameAction& action) {
        // CipSoft TPlayerMovementGameActionHandler : stop prior
        // auto-walk / follow, APPLY_AUTOCHASE, then apply Target intent.
        publish(makeAction(A::APPLY_AUTOCHASE_SETTING));
        const auto mode = static_cast<tibia::client::AutoWalkPathMode>(action.autoWalkPathMode);
        (void)TPlayerMovementIntent::target(action.position.x, action.position.y, action.position.z);
        if (tibia::game::localPlayer()) {
            // Cancel in-flight GoPath on the wire before arming a new path.
            if (tibia::game::localPlayerIsAutoWalking() || tibia::game::localPlayerHasAutoWalkDirs())
                tibia::game::interruptAutoWalk();
            else if (tibia::game::isFollowing())
                tibia::game::interruptAutoWalk();
        }
        setTargetIntent(action.position, mode);
        tibia::game::localPlayerAutoWalk(action.position, false, mode);
        return true;
    });
}

TPlayerMovementGameActionHandler::~TPlayerMovementGameActionHandler()
{
    setActionHandler(A::SET_PLAYER_MOVEMENT_INTENT, nullptr);
    if (s_instance == this)
        s_instance = nullptr;
}

void TPlayerMovementGameActionHandler::setTargetIntent(const Position& position, const tibia::client::AutoWalkPathMode pathMode)
{
    m_targetPosition = position;
    m_targetPathMode = pathMode;
    m_targetLive = position.isValid();
    m_replanElapsed.restart();
    if (m_targetLive)
        m_replanTimer->start();
    else
        m_replanTimer->stop();
}

void TPlayerMovementGameActionHandler::clearTargetIntent()
{
    if (!m_targetLive && !m_replanTimer->isActive())
        return;
    m_targetLive = false;
    m_targetPosition = {};
    m_targetPathMode = tibia::client::AutoWalkPathMinimap;
    m_replanTimer->stop();
}

void TPlayerMovementGameActionHandler::onReplanTick()
{
    if (!m_targetLive) {
        m_replanTimer->stop();
        return;
    }
    if (!tibia::game::isOnline()) {
        clearTargetIntent();
        return;
    }
    if (!tibia::game::localPlayer()) {
        clearTargetIntent();
        return;
    }
    // Arrival: LocalPlayerSessionThing::onPositionChange also stopAutoWalk → clearTargetIntent.
    if (tibia::game::localPlayerPosition() == m_targetPosition) {
        clearTargetIntent();
        return;
    }

    // CipSoft Target replan only when GoPath is not live: in-flight tween,
    // pending prewalk, or armed ≤9 dirs. Destination alone is not “live” —
    // that blocked stalled replan until stopAutoWalk cleared the goal.
    if (tibia::game::localPlayerIsWalking() || tibia::game::localPlayerIsPreWalking()
        || tibia::game::localPlayerHasAutoWalkDirs())
        return;
    if (m_replanElapsed.elapsed() < TargetReplanIntervalMs)
        return;

    tibia::game::localPlayerAutoWalk(m_targetPosition, true, m_targetPathMode);
    m_replanElapsed.restart();
}

}
