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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tibia::chat {
class TTextStorage;
}
namespace tibia::config {
class TClientOptions;
}
namespace tibia::creatures {
class TCreature;
class TCreatureMovementSpeedCalculator;
class TCreatureStorage;
}
namespace tibia::game {
class TPlayerData;
}
namespace tibia::minimap {
class TMinimapTileStorage;
}
namespace tibia::shared {
class IClockService;
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
    /// Host inputs qtclient computes outside the session's objects.
    struct Environment
    {
        /// Whether the client is aware of the field (the world map session state).
        std::function<bool(const worldmap::TWorldMapCoordinate&)> isInView;
        /// qtclient pack extension, not CipSoft: a map target (no entering an unwalkable goal)
        /// the planner refuses may be taken over (planner result; true: no failure message).
        std::function<bool(const worldmap::TWorldMapCoordinate&, int result)> claimUnreachable;
    };

    /// The session's objects the handler works with: the movement
    /// configuration (how early the next step goes out; none: the defaults), the
    /// client options (auto chase, turning, keyboard delay), the clock, the world
    /// map, the text storage (the status line), the creature storage (attack and
    /// follow targets), the movement speed calculator and the player data (the
    /// player's state bits and tactics).
    TPlayerMovementIntentHandler(std::shared_ptr<const game::TPlayerMovementIntentHandlerConfiguration> pConfiguration,
                                 config::TClientOptions* pClientOptions,
                                 std::shared_ptr<shared::IClockService> pClockService,
                                 std::shared_ptr<worldmap::TWorldMapStorage> pWorldMapStorage,
                                 std::shared_ptr<chat::TTextStorage> pTextStorage,
                                 std::shared_ptr<creatures::TCreatureStorage> pCreatureStorage,
                                 std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> pCreatureMovementSpeedCalculator,
                                 std::shared_ptr<game::TPlayerData> pPlayerData, Environment environment,
                                 QObject* parent = nullptr);

    /// CIP v12: the minimap tile storage the planner reads (its waypoint costs
    /// and the fields to avoid); the session hands it over.
    void setMinimapTileStorage(std::shared_ptr<minimap::TMinimapTileStorage> pMinimapTileStorage);

    /// CIP v16: the player's new movement intent. A target coordinate plans a
    /// path there (after the tile storage dropped the avoided fields the world
    /// no longer knows); a direction is held: it steps now and repeats until
    /// another intent (after a turn the turn's direction counts as held); a
    /// rotation turns the player; a stop ends the held direction. Without a
    /// tile storage the intent kept for the next one is a stop.
    void handleMovementIntent(const TMovementIntentPtr& intent);
    /// One step in `direction`.
    void step(creatures::EMovementDirection direction);
    /// Drop the goal and the path; a step in flight still completes.
    void stopWalking();
    /// CIP v33: drop the goal and the path, keeping a held direction.
    void clearPath();

    /// The player's position changed (a confirmed step, a teleport).
    void onPositionChanged(const worldmap::TWorldMapCoordinate& position);
    /// The server rejected the step; the player looks in `direction`.
    void onCancelWalk(creatures::ECreatureLookDirection direction);
    /// Leaving the game.
    void reset();

    bool hasGoal() const { return m_goal.has_value(); }
    bool isPredicting() const { return m_pendingX != 0 || m_pendingY != 0; }
    worldmap::TWorldMapCoordinate predictedPosition() const;

public Q_SLOTS:
    /// CIP v13: the handler keeps the player's creature and follows its
    /// position writes (onPositionChanged).
    void setPlayer(std::weak_ptr<tibia::creatures::TCreature> pPlayer);
    /// CIP v14: called every frame, and when the session's movement timer
    /// fires: repeats a held direction and sends a pending path.
    void tick();

Q_SIGNALS:
    void sendGoPath(std::vector<tibia::creatures::EMovementDirection> path);
    /// Asks the session to call tick() again after `when` (the session's
    /// single-shot movement timer). CipSoft declares it; nothing emits it.
    void requestTimedCallback(std::chrono::milliseconds when);

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
    /// The steps of the planned path.
    int pathLength() const { return static_cast<int>(m_path.size()); }
    void sendPath();
    /// Before a path goes out: unless auto chase is on, attacking with chase on
    /// sends the tactics again with chase off; following stops following and
    /// attacking.
    void applyAutoChaseBeforeWalking();
    qint64 now() const;
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

    /// +0x10..+0xa0: the session's objects.
    std::shared_ptr<const game::TPlayerMovementIntentHandlerConfiguration> m_pConfiguration;
    config::TClientOptions* m_pClientOptions = nullptr;
    std::shared_ptr<shared::IClockService> m_pClockService;
    std::shared_ptr<worldmap::TWorldMapStorage> m_pWorldMapStorage;
    std::shared_ptr<chat::TTextStorage> m_pTextStorage;
    std::shared_ptr<creatures::TCreatureStorage> m_pCreatureStorage;
    std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> m_pCreatureMovementSpeedCalculator;
    std::shared_ptr<game::TPlayerData> m_pPlayerData;
    Environment m_env;
    /// +0x90: the player's creature (setPlayer).
    std::shared_ptr<creatures::TCreature> m_player;
    /// +0xb0: the minimap tile storage (setMinimapTileStorage).
    std::shared_ptr<minimap::TMinimapTileStorage> m_pMinimapTileStorage;
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
