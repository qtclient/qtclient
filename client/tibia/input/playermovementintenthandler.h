#pragma once

#include "creatures/movementdirection.h"
#include "input/movementpathplanner.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>
#include <client/const.h>

#include <QElapsedTimer>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

namespace tibia::creatures {
class TCreature;
}
namespace tibia::worldmap {
class TWorldMapStorage;
}

namespace tibia::input {

creatures::EMovementDirection toMovementDirection(tibia::client::Direction direction);
tibia::client::Direction toClientDirection(creatures::EMovementDirection direction);

/// Turns the player's movement intents into steps.
///
/// A direction or a target coordinate becomes a goal; the planner turns the
/// goal into up to ten steps, sent to the server as one step or as a path. The
/// steps are then predicted one at a time: the creature starts moving without
/// its position changing, and the next step waits until the server confirmed
/// the previous one, its motion finished and the server's step time passed.
/// The server's move confirms a prediction; a rejected step clears the path and
/// plans again.
class TPlayerMovementIntentHandler final : public cip::tibia::input::TPlayerMovementIntentHandler
{
    Q_OBJECT
public:
    struct Environment
    {
        std::function<qint64()> now;
        std::function<creatures::TCreature*()> player;
        std::function<worldmap::TWorldMapStorage*()> map;
        /// Waypoint costs for one plan.
        std::function<WaypointCost()> waypointCost;
        /// Fields other creatures block.
        std::function<std::vector<worldmap::TWorldMapCoordinate>()> blockedFields;
        std::function<bool()> alwaysTurnTowardsMoveDirection;
        /// Delay before a held direction repeats (the keyboard delay option).
        std::function<int()> keyboardDelayMs;
        /// Before a path is sent: chase off unless auto-chase is on, and stop
        /// following and attacking while following.
        std::function<void()> stopChaseAndFollow;
        std::function<void()> stopFollowing;
        std::function<bool(const worldmap::TWorldMapCoordinate&)> isInView;
        std::function<void(const QString& textId)> showFailure;
        /// The local player's state bits (bit 19 rooted, bit 20 feared).
        std::function<quint64()> playerStates;
    };

    explicit TPlayerMovementIntentHandler(Environment environment, QObject* parent = nullptr);

    /// A held direction: steps now, then repeats until `stopIntent`.
    void walkInDirection(creatures::EMovementDirection direction);
    /// One step in `direction`.
    void step(creatures::EMovementDirection direction);
    void walkTo(const worldmap::TWorldMapCoordinate& goal, bool enterUnwalkableGoal);
    void rotate(creatures::ECreatureLookDirection direction);
    void stopIntent();
    /// Drop the goal and the path; a step in flight still completes.
    void stopWalking();
    /// Drop the goal and the path, keeping a held direction.
    void clearGoal();

    /// Called every frame.
    void tick();
    /// The player's position changed (a confirmed step, a teleport).
    void onPositionChanged(const worldmap::TWorldMapCoordinate& position);
    /// The server rejected the step; the player looks in `direction`.
    void onCancelWalk(creatures::ECreatureLookDirection direction);
    /// Leaving the game.
    void reset();

    bool hasGoal() const { return m_goal.has_value(); }
    bool isPredicting() const { return m_pendingX != 0 || m_pendingY != 0; }
    worldmap::TWorldMapCoordinate predictedPosition() const;

Q_SIGNALS:
    void sendGoPath(std::vector<tibia::creatures::EMovementDirection> path);

private:
    /// Whether a step onto a field is predicted: Free fields are, Unsure ones
    /// (no ground known, a field to avoid, a creature that may let the player
    /// through) are only sent, Blocked ones (not loaded, unpassable) stop the path.
    enum class EFieldState { Free, Blocked, Unsure };
    enum class EIntent { None, Target, Direction, Rotate, Stop };

    int goTo(const worldmap::TWorldMapCoordinate& goal, bool enterUnwalkableGoal, bool allowDiagonalStep);
    void goToAndSend(const worldmap::TWorldMapCoordinate& goal, bool enterUnwalkableGoal, bool allowDiagonalStep);
    int plan();
    void predictNextStep();
    void predictStep(int dx, int dy);
    bool advance();
    bool resetAfterRejectedStep(creatures::ECreatureLookDirection direction);
    void clearPath();
    void sendPath();
    void sendStep(creatures::EMovementDirection direction);
    void sendRotate(creatures::ECreatureLookDirection direction);
    qint64 nextStepDelay(const worldmap::TWorldMapCoordinate& from, const worldmap::TWorldMapCoordinate& to) const;
    EFieldState fieldState(const worldmap::TWorldMapCoordinate& coordinate) const;
    bool canStepTowards(creatures::EMovementDirection direction) const;
    void noteIntent(EIntent intent);
    static creatures::EMovementDirection directionOfLook(creatures::ECreatureLookDirection look);

    Environment m_env;
    std::optional<worldmap::TWorldMapCoordinate> m_goal;
    bool m_enterUnwalkableGoal = false;
    bool m_allowDiagonalStep = false;
    std::vector<creatures::EMovementDirection> m_path;
    int m_stepsTaken = 0;
    int m_pendingX = 0;
    int m_pendingY = 0;
    bool m_waiting = false;
    bool m_replan = false;
    qint64 m_nextStepMs = 0;
    std::optional<creatures::EMovementDirection> m_heldDirection;
    int m_repeats = 0;
    QElapsedTimer m_repeatTimer;
    std::optional<worldmap::TWorldMapCoordinate> m_lastPosition;
    std::optional<worldmap::TWorldMapCoordinate> m_stuckPosition;
    int m_stuckCount = 0;
    EIntent m_lastIntent = EIntent::None;
    creatures::ECreatureLookDirection m_lastRotation = creatures::ECreatureLookDirection::North;
};

}
