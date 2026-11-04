#include "playermovementgameactionhandler.h"

#include "gameaction.h"
#include "gameactionhandlers.h"
#include "inputciptypes.h"

#include <client/const.h>
#include <tibia/world/localplayer.h>

#include <QTimer>
#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;

/// CIP IntentHandler tick grain (same order as direction-hold RepeatDelayMs).
constexpr int TargetReplanTickMs = 50;
/// Floor between mid-route replans so findPathAsync / GoPath are not thrashed
/// every tick. Matches qtclient's first autoWalk retry delay and CIP keyboard delay.
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
        // CIP TPlayerMovementGameActionHandler (0x100e79d60) also stops
        // following, which Game::autoWalk does itself.
        publish(makeAction(A::APPLY_AUTOCHASE_SETTING));
        const auto mode = static_cast<tibia::client::AutoWalkPathMode>(action.autoWalkPathMode);
        // CIP TPlayerMovementIntent::Target — keep live and tick-replan.
        (void)TPlayerMovementIntent::target(action.position.x, action.position.y, action.position.z);
        setTargetIntent(action.position, mode);
        if (const LocalPlayerPtr& player = tibia::game::localPlayer())
            player->autoWalk(action.position, false, mode);
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
    const LocalPlayerPtr& player = tibia::game::localPlayer();
    if (!player) {
        clearTargetIntent();
        return;
    }
    // Arrival: LocalPlayer::onPositionChange also stopAutoWalk → clearTargetIntent.
    if (player->getPosition() == m_targetPosition) {
        clearTargetIntent();
        return;
    }

    // Re-issue when qtclient dropped autoWalk (path fail without stopAutoWalk) or
    // on the CIP-style periodic Target replan while still en route.
    const bool stalled = !player->isAutoWalking();
    if (!stalled && m_replanElapsed.elapsed() < TargetReplanIntervalMs)
        return;

    player->autoWalk(m_targetPosition, true, m_targetPathMode);
    m_replanElapsed.restart();
}

}
