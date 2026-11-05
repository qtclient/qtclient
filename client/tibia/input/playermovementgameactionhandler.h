#pragma once

#include <cip_gen.h>

#include <client/const.h>
#include <client/position.h>

#include <QElapsedTimer>

class QTimer;

namespace tibia {
namespace input {

/// CIP TPlayerMovementGameActionHandler (0x100e79d60): SET_PLAYER_MOVEMENT_INTENT
/// stops following (via Game::autoWalk) and walks to the action's coordinate.
///
/// Also absorbs the CIP TPlayerMovementIntentHandler Target role: keeps a live
/// Target intent and periodically re-issues autoWalk(retry=true) until Stop,
/// keyboard direction walk, stopAutoWalk / server cancel, or arrival. Direction
/// hold remains on TPlayerMovementInputInterpreter::onTick.
class TPlayerMovementGameActionHandler final : public cip::tibia::input::TPlayerMovementGameActionHandler
{
    Q_OBJECT
public:
    explicit TPlayerMovementGameActionHandler(QObject* parent = nullptr);
    ~TPlayerMovementGameActionHandler() override;
    TPlayerMovementGameActionHandler(const TPlayerMovementGameActionHandler&) = delete;
    TPlayerMovementGameActionHandler& operator=(const TPlayerMovementGameActionHandler&) = delete;

    static TPlayerMovementGameActionHandler* instance();

    /// CIP TMovementIntentStop / keyboard takeover / LocalPlayer::stopAutoWalk / arrival.
    void clearTargetIntent();

private:
    void setTargetIntent(const Position& position, tibia::client::AutoWalkPathMode pathMode);
    /// CIP TPlayerMovementIntentHandler Target replan (0x100e6adb0 / timed wake).
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
