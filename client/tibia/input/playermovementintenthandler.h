#pragma once

#include "creatures/movementdirection.h"
#include "game/playermovementintenthandlerconfiguration.h"
#include "input/inputciptypes.h"
#include "input/movementpathplanner.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>
#include <client/const.h>

#include <QElapsedTimer>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tibia::chat {
class TTextStorage;
}
namespace tibia::creatures {
class TCreature;
}
namespace tibia::worldmap {
class TWorldMapStorage;
}

namespace tibia::input {

creatures::EMovementDirection toMovementDirection(tibia::client::Direction direction);

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
        /// The session's text storage: the status line shows why a walk failed.
        std::function<chat::TTextStorage*()> textStorage;
        /// qtclient pack extension, not CipSoft: a map target (no entering an unwalkable goal)
        /// the planner refuses may be taken over (planner result; true: no failure message).
        std::function<bool(const worldmap::TWorldMapCoordinate&, int result)> claimUnreachable;
        /// The local player's state bits (bit 19 rooted, bit 20 feared).
        std::function<quint64()> playerStates;
        /// How early the next step goes out (the session meta info's
        /// configuration; none: the defaults).
        std::shared_ptr<const game::TPlayerMovementIntentHandlerConfiguration> configuration;
    };

    explicit TPlayerMovementIntentHandler(Environment environment, QObject* parent = nullptr);

    /// CIP v16: the player's new movement intent. A target coordinate plans a
    /// path there; a direction is held: it steps now and repeats until another
    /// intent (after a turn the turn's direction counts as held); a rotation
    /// turns the player; a stop ends the held direction. Without a player the
    /// intent kept for the next one is a stop.
    void handleMovementIntent(const TMovementIntentPtr& intent);
    /// One step in `direction`.
    void step(creatures::EMovementDirection direction);
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

    int goTo(const worldmap::TWorldMapCoordinate& goal, bool enterUnwalkableGoal, bool allowDiagonalStep);
    void goToAndSend(const worldmap::TWorldMapCoordinate& goal, bool enterUnwalkableGoal, bool allowDiagonalStep);
    int plan();
    void predictNextStep();
    void predictStep(int dx, int dy);
    bool advance();
    bool resetAfterRejectedStep(creatures::ECreatureLookDirection direction);
    void clearPath();
    /// The steps of the planned path.
    int pathLength() const { return static_cast<int>(m_path.size()); }
    void sendPath();
    void sendStep(creatures::EMovementDirection direction);
    void sendRotate(creatures::ECreatureLookDirection direction);
    /// The status line shows `text` in the overlay colour of message mode 21.
    void showStatusMessage(const QString& text);
    /// The time until the next step may go out: the step's duration less the
    /// configured latency, but at least the configured share of the duration.
    qint64 nextStepDelay(const worldmap::TWorldMapCoordinate& from, const worldmap::TWorldMapCoordinate& to) const;
    EFieldState fieldState(const worldmap::TWorldMapCoordinate& coordinate) const;
    bool canStepTowards(creatures::EMovementDirection direction) const;
    /// A held direction steps now; holding it again counts a repeat.
    void holdDirection(creatures::EMovementDirection direction);
    /// CIP v23: the player holds a movement intent unless it is a stop.
    void noteIntent(TMovementIntentKind kind);
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
    /// +0xd0: the last intent.
    TMovementIntentPtr m_intent;
};

}
