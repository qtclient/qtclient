#pragma once

#include <cip_gen.h>

#include <client/const.h>
#include <client/position.h>

#include <QElapsedTimer>

class QTimer;

namespace tibia {
namespace input {

/// CIP TPlayerMovementGameActionHandler : SET_PLAYER_MOVEMENT_INTENT
/// stops following (via Game::autoWalk) and walks to the action's coordinate.
///
/// Also absorbs the CIP TPlayerMovementIntentHandler Target role: keeps a live
/// Target intent and re-issues autoWalk(retry=true) only when GoPath is not live
/// (stalled / failed) until Stop, keyboard direction walk, stopAutoWalk /
/// server cancel, or arrival. Mid-route continue is lastAutoWalkPosition.
/// Direction hold remains on TPlayerMovementInputInterpreter::onTick.
class TPlayerMovementGameActionHandler final : public cip::tibia::input::TPlayerMovementGameActionHandler
{
    Q_OBJECT
public:
    explicit TPlayerMovementGameActionHandler(QObject* parent = nullptr);
    ~TPlayerMovementGameActionHandler() override;
    TPlayerMovementGameActionHandler(const TPlayerMovementGameActionHandler&) = delete;
    TPlayerMovementGameActionHandler& operator=(const TPlayerMovementGameActionHandler&) = delete;

    static TPlayerMovementGameActionHandler* instance();

    /// CIP TMovementIntentStop / keyboard takeover / LocalPlayerSessionThing::stopAutoWalk / arrival.
    void clearTargetIntent();

private:
    void setTargetIntent(const Position& position, tibia::client::AutoWalkPathMode pathMode);
    /// CIP TPlayerMovementIntentHandler Target replan (timed wake).
    void onReplanTick();

    static TPlayerMovementGameActionHandler* s_instance;

    QTimer* m_replanTimer = nullptr;
    QElapsedTimer m_replanElapsed;
    Position m_targetPosition;
    tibia::client::AutoWalkPathMode m_targetPathMode{ tibia::client::AutoWalkPathMinimap };
    bool m_targetLive = false;
};

}
}
