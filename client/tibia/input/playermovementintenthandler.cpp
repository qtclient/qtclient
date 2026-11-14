#include "input/playermovementintenthandler.h"

#include "creatures/creaturestorage.h"
#include "creatures/walkdebug.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <algorithm>
#include <cstdlib>

namespace tibia::input {

namespace {

using creatures::EMovementDirection;
using creatures::lcWalkGoPath;
using worldmap::TWorldMapCoordinate;

constexpr int RepeatIntervalMs = 50;
constexpr int StuckLimit = 14;
constexpr int RootedState = 19;
constexpr int FearedState = 20;

TWorldMapCoordinate offsetBy(const TWorldMapCoordinate& c, const int dx, const int dy)
{
    return { c.x + dx, c.y + dy, c.z };
}

}

EMovementDirection toMovementDirection(const tibia::client::Direction direction)
{
    switch (direction) {
    case tibia::client::North: return EMovementDirection::North;
    case tibia::client::NorthEast: return EMovementDirection::NorthEast;
    case tibia::client::East: return EMovementDirection::East;
    case tibia::client::SouthEast: return EMovementDirection::SouthEast;
    case tibia::client::South: return EMovementDirection::South;
    case tibia::client::SouthWest: return EMovementDirection::SouthWest;
    case tibia::client::West: return EMovementDirection::West;
    case tibia::client::NorthWest: return EMovementDirection::NorthWest;
    default: return EMovementDirection::None;
    }
}

tibia::client::Direction toClientDirection(const EMovementDirection direction)
{
    switch (direction) {
    case EMovementDirection::North: return tibia::client::North;
    case EMovementDirection::NorthEast: return tibia::client::NorthEast;
    case EMovementDirection::East: return tibia::client::East;
    case EMovementDirection::SouthEast: return tibia::client::SouthEast;
    case EMovementDirection::South: return tibia::client::South;
    case EMovementDirection::SouthWest: return tibia::client::SouthWest;
    case EMovementDirection::West: return tibia::client::West;
    case EMovementDirection::NorthWest: return tibia::client::NorthWest;
    default: return tibia::client::InvalidDirection;
    }
}

TPlayerMovementIntentHandler::TPlayerMovementIntentHandler(Environment environment, QObject* parent)
    : cip::tibia::input::TPlayerMovementIntentHandler(parent)
    , m_env(std::move(environment))
{
}

TWorldMapCoordinate TPlayerMovementIntentHandler::predictedPosition() const
{
    const creatures::TCreature* player = m_env.player();
    if (!player)
        return {};
    return offsetBy(TWorldMapCoordinate(player->position()), m_pendingX, m_pendingY);
}

void TPlayerMovementIntentHandler::walkInDirection(const EMovementDirection direction)
{
    // Walking on after a turn continues the held direction the turn faced.
    if (m_lastIntent == EIntent::Rotate)
        m_heldDirection = directionOfLook(m_lastRotation);
    m_repeatTimer.restart();
    m_repeats = m_heldDirection == direction ? m_repeats + 1 : 0;
    m_heldDirection = direction;
    step(direction);
    noteIntent(EIntent::Direction);
}

void TPlayerMovementIntentHandler::walkTo(const TWorldMapCoordinate& goal, const bool enterUnwalkableGoal)
{
    goToAndSend(goal, enterUnwalkableGoal, false);
    noteIntent(EIntent::Target);
}

void TPlayerMovementIntentHandler::rotate(const creatures::ECreatureLookDirection direction)
{
    sendRotate(direction);
    m_heldDirection.reset();
    m_lastRotation = direction;
    noteIntent(EIntent::Rotate);
}

void TPlayerMovementIntentHandler::stopIntent()
{
    m_heldDirection.reset();
    noteIntent(EIntent::Stop);
}

void TPlayerMovementIntentHandler::stopWalking()
{
    m_heldDirection.reset();
    clearPath();
}

void TPlayerMovementIntentHandler::clearGoal()
{
    clearPath();
}

void TPlayerMovementIntentHandler::noteIntent(const EIntent intent)
{
    m_lastIntent = intent;
    if (creatures::TCreature* player = m_env.player())
        player->setHoldsMovementIntent(intent != EIntent::Stop);
}

EMovementDirection TPlayerMovementIntentHandler::directionOfLook(const creatures::ECreatureLookDirection look)
{
    switch (look) {
    case creatures::ECreatureLookDirection::North: return EMovementDirection::North;
    case creatures::ECreatureLookDirection::East: return EMovementDirection::East;
    case creatures::ECreatureLookDirection::South: return EMovementDirection::South;
    case creatures::ECreatureLookDirection::West: return EMovementDirection::West;
    }
    return EMovementDirection::None;
}

bool TPlayerMovementIntentHandler::canStepTowards(const EMovementDirection direction) const
{
    if (creatures::isDiagonal(direction))
        return false;
    const creatures::TCreature* player = m_env.player();
    if (!player)
        return false;
    const worldmap::TWorldMapExtent offset = creatures::stepOffset(direction);
    const TWorldMapCoordinate position(player->position());
    const worldmap::TWorldMapField* field = m_env.map()->fieldAt(offsetBy(position, offset.width, offset.height));
    if (!field || !field->hasBank())
        return true;
    return m_env.map()->elevationAt(position.toPosition()) != 0;
}

void TPlayerMovementIntentHandler::step(const EMovementDirection direction)
{
    const worldmap::TWorldMapExtent offset = creatures::stepOffset(direction);
    goToAndSend(offsetBy(predictedPosition(), offset.width, offset.height), true, creatures::isDiagonal(direction));
}

void TPlayerMovementIntentHandler::goToAndSend(const TWorldMapCoordinate& goal, const bool enterUnwalkableGoal,
                                               const bool allowDiagonalStep)
{
    const int result = goTo(goal, enterUnwalkableGoal, allowDiagonalStep);
    if (result == 2) {
        if (m_env.stopChaseAndFollow)
            m_env.stopChaseAndFollow();
        Q_EMIT sendStop();
    }
    if (result == 1)
        sendPath();
}

int TPlayerMovementIntentHandler::goTo(const TWorldMapCoordinate& goal, const bool enterUnwalkableGoal,
                                       const bool allowDiagonalStep)
{
    creatures::TCreature* player = m_env.player();
    if (!player || (m_goal && *m_goal == goal) || goal == offsetBy(predictedPosition(), m_pendingX, m_pendingY))
        return 0;
    if (const quint64 states = m_env.playerStates ? m_env.playerStates() : 0; states & (quint64(1) << RootedState | quint64(1) << FearedState))
        return 0;
    m_goal = goal;
    m_enterUnwalkableGoal = enterUnwalkableGoal;
    m_allowDiagonalStep = allowDiagonalStep;

    // A blocked field in view: turn towards it (when the option asks for that)
    // or explain that the step is impossible. A straight step the player may
    // still take (onto a field without ground, or up from a raised field) is
    // planned after the turn. A goal more than one step away has no step
    // direction: it can't be stepped onto, and with the turn option on it is
    // dropped without a turn or a message.
    const TWorldMapCoordinate position(player->position());
    const int dx = goal.x - position.x;
    const int dy = goal.y - position.y;
    const bool adjacent = std::abs(dx) <= 1 && std::abs(dy) <= 1;
    const EMovementDirection direction =
        adjacent ? creatures::directionBetween(position, goal) : EMovementDirection::None;
    const bool canStep = adjacent && canStepTowards(direction);
    bool turned = false;
    const bool inView = m_env.isInView && m_env.isInView(goal);
    if (inView && goal.z == position.z && (dx != 0 || dy != 0) && (fieldState(goal) == EFieldState::Blocked || canStep)) {
        const bool turnOption = m_env.alwaysTurnTowardsMoveDirection && m_env.alwaysTurnTowardsMoveDirection();
        if (turnOption && !adjacent) {
            m_goal.reset();
            return 0;
        }
        if (turnOption) {
            const auto current = static_cast<creatures::ECreatureLookDirection>(
                player->isMoving() ? player->movementLook() : player->patternX());
            const auto look = creatures::lookDirectionAfter(current, direction);
            if (look != current) {
                turned = true;
                sendRotate(look);
            }
        }
        if (!canStep) {
            qCDebug(lcWalkGoPath) << "blocked" << creatures::walkPos(goal.toPosition()) << "turned" << turned;
            if (!turned && m_env.showFailure)
                m_env.showFailure(QStringLiteral("sorry_not_possible"));
            m_goal.reset();
            return 0;
        }
    }

    if (m_replan)
        return 0;
    if (m_path.empty())
        return plan();
    if (!turned && m_stepsTaken < static_cast<int>(m_path.size())) {
        m_replan = true;
        return 2;
    }
    return 0;
}

int TPlayerMovementIntentHandler::plan()
{
    m_path.clear();
    m_replan = false;
    if (!m_goal || !m_env.player())
        return 0;
    if (std::abs(m_pendingX) + std::abs(m_pendingY) > 1) {
        m_waiting = true;
        return 0;
    }
    const std::vector<TWorldMapCoordinate> blocked = m_env.blockedFields ? m_env.blockedFields()
                                                                          : std::vector<TWorldMapCoordinate>{};
    const int result = planMovementPath(predictedPosition(), *m_goal, m_enterUnwalkableGoal, m_allowDiagonalStep,
                                        m_env.waypointCost(), blocked, m_path);
    qCDebug(lcWalkGoPath) << "plan" << "from" << creatures::walkPos(predictedPosition().toPosition()) << "goal"
                          << creatures::walkPos(m_goal->toPosition()) << "result" << result << "steps" << m_path.size();
    if (result == PathComplete && m_path.size() < 2)
        m_goal.reset();
    if (result == PathComplete || result == PathPartial) {
        m_stepsTaken = 0;
        predictNextStep();
        return 1;
    }
    m_goal.reset();
    const char* message = nullptr;
    switch (result) {
    case PathInternalError: message = "error_code_internal"; break;
    case PathNoWay: message = "there_is_no_way"; break;
    case PathOutOfRange: message = "destination_out_of_range"; break;
    case PathGoDownstairs: message = "first_go_downstairs"; break;
    case PathGoUpstairs: message = "first_go_upstairs"; break;
    default: break;
    }
    if (message && m_env.showFailure)
        m_env.showFailure(QString::fromLatin1(message));
    return 0;
}

void TPlayerMovementIntentHandler::predictNextStep()
{
    creatures::TCreature* player = m_env.player();
    if (!player || m_replan || player->healthPercent() == 0 || m_stepsTaken >= static_cast<int>(m_path.size()))
        return;
    if (m_pendingX != 0 || m_pendingY != 0) {
        m_waiting = true;
        return;
    }
    const worldmap::TWorldMapExtent offset = creatures::stepOffset(m_path[static_cast<size_t>(m_stepsTaken)]);
    const TWorldMapCoordinate position(player->position());
    const TWorldMapCoordinate target = offsetBy(position, offset.width, offset.height);
    const EFieldState state = fieldState(target);
    if (state == EFieldState::Blocked)
        return;
    bool alreadyPending = false;
    if (m_env.map()->groundSpeedAt(target.toPosition()) != 0) {
        if (m_pendingX == offset.width && m_pendingY == offset.height)
            alreadyPending = true;
        else
            ++m_stepsTaken;
    }
    if (!alreadyPending)
        m_waiting = true;
    m_pendingX = offset.width;
    m_pendingY = offset.height;
    if (state == EFieldState::Free)
        predictStep(offset.width, offset.height);
    m_nextStepMs = m_env.now() + nextStepDelay(position, target);
}

void TPlayerMovementIntentHandler::predictStep(const int dx, const int dy)
{
    creatures::TCreature* player = m_env.player();
    if (!player)
        return;
    worldmap::TWorldMapStorage* map = m_env.map();
    const Position from = player->position();
    const Position to = offsetBy(TWorldMapCoordinate(from), dx, dy).toPosition();
    map->moveCreature(*player, from, to, map->elevationAt(from), map->elevationAt(to));
}

qint64 TPlayerMovementIntentHandler::nextStepDelay(const TWorldMapCoordinate& from, const TWorldMapCoordinate& to) const
{
    const creatures::TCreature* player = m_env.player();
    worldmap::TWorldMapStorage* map = m_env.map();
    const worldmap::TWorldMapExtent step(to.x - from.x, to.y - from.y, to.z - from.z);
    return map->speedCalculator().stepDurationWithDiagonalCost(player->speed(), step,
                                                               map->groundSpeedAt(to.toPosition()));
}

bool TPlayerMovementIntentHandler::advance()
{
    creatures::TCreature* player = m_env.player();
    if (!player || !m_waiting || !player->movementTween().isTweenFinished() || m_env.now() <= m_nextStepMs)
        return false;
    m_waiting = false;
    if (static_cast<int>(m_path.size()) <= m_stepsTaken)
        return plan() == 1;
    predictNextStep();
    return false;
}

void TPlayerMovementIntentHandler::tick()
{
    if (!m_repeatTimer.isValid())
        m_repeatTimer.start();
    if (m_heldDirection) {
        const int interval = m_repeats > 0 ? RepeatIntervalMs : (m_env.keyboardDelayMs ? m_env.keyboardDelayMs() : 250);
        if (m_repeatTimer.elapsed() > interval) {
            m_repeatTimer.restart();
            ++m_repeats;
            step(*m_heldDirection);
        }
    }
    if (advance())
        sendPath();
}

void TPlayerMovementIntentHandler::onPositionChanged(const TWorldMapCoordinate& position)
{
    if (m_lastPosition && !(*m_lastPosition == position)) {
        const int dx = position.x - m_lastPosition->x;
        const int dy = position.y - m_lastPosition->y;
        const bool jump = !(std::abs(dx) < 2 && std::abs(dy) < 2 && position.z == m_lastPosition->z);
        const bool atGoal = m_goal && m_goal->x == position.x && m_goal->y == position.y;
        qCDebug(lcWalkGoPath) << "position" << creatures::walkPos(position.toPosition()) << "step" << dx << dy << "pending"
                              << m_pendingX << m_pendingY << "jump" << jump << "atGoal" << atGoal;
        if ((atGoal && (jump || m_goal->z == position.z)) || (!atGoal && jump)) {
            clearPath();
            m_pendingX = m_pendingY = 0;
            m_waiting = false;
        } else {
            bool expected = false;
            if (m_env.map()->groundSpeedAt(position.toPosition()) != 0) {
                expected = m_pendingX == dx && m_pendingY == dy;
                m_pendingX = m_pendingY = 0;
                if (!expected)
                    ++m_stepsTaken;
            }
            if (!expected)
                m_waiting = true;
        }
    }
    m_lastPosition = position;
}

void TPlayerMovementIntentHandler::onCancelWalk(const creatures::ECreatureLookDirection direction)
{
    qCDebug(lcWalkGoPath) << "cancel" << "pending" << m_pendingX << m_pendingY << "steps" << m_stepsTaken << "of"
                          << m_path.size();
    m_nextStepMs = m_env.now() + 1;
    bool stuck = false;
    if (m_lastPosition && m_stuckPosition && *m_lastPosition == *m_stuckPosition) {
        stuck = m_stuckCount > StuckLimit;
        ++m_stuckCount;
    } else {
        m_stuckCount = 1;
    }
    if (m_lastPosition)
        m_stuckPosition = m_lastPosition;
    if (stuck) {
        clearPath();
        if (m_env.stopFollowing)
            m_env.stopFollowing();
        m_stuckCount = 0;
    }
    if (resetAfterRejectedStep(direction))
        sendPath();
}

bool TPlayerMovementIntentHandler::resetAfterRejectedStep(const creatures::ECreatureLookDirection direction)
{
    m_path.clear();
    creatures::TCreature* player = m_env.player();
    if (!player) {
        m_pendingX = m_pendingY = 0;
        return false;
    }
    player->setDirection(static_cast<uint8_t>(direction));
    const bool pending = m_pendingX != 0 || m_pendingY != 0;
    if (!pending && m_waiting)
        return false;
    m_pendingX = m_pendingY = 0;
    if (pending) {
        ++m_stepsTaken;
        m_waiting = true;
    }
    predictStep(0, 0);
    return plan() == 1;
}

void TPlayerMovementIntentHandler::clearPath()
{
    m_goal.reset();
    m_path.clear();
    m_stepsTaken = 0;
    m_replan = false;
}

void TPlayerMovementIntentHandler::reset()
{
    clearPath();
    m_pendingX = m_pendingY = 0;
    m_waiting = false;
    m_heldDirection.reset();
    m_lastPosition.reset();
    m_stuckPosition.reset();
    m_stuckCount = 0;
}

void TPlayerMovementIntentHandler::sendPath()
{
    if (m_path.empty())
        return;
    qCDebug(lcWalkGoPath) << "send" << "steps" << m_path.size() << "first" << int(m_path.front());
    if (m_env.stopChaseAndFollow)
        m_env.stopChaseAndFollow();
    if (m_path.size() == 1)
        sendStep(m_path.front());
    else
        Q_EMIT sendGoPath(m_path);
}

void TPlayerMovementIntentHandler::sendStep(const EMovementDirection direction)
{
    switch (direction) {
    case EMovementDirection::North: Q_EMIT sendGoNorth(); break;
    case EMovementDirection::NorthEast: Q_EMIT sendGoNorthEast(); break;
    case EMovementDirection::East: Q_EMIT sendGoEast(); break;
    case EMovementDirection::SouthEast: Q_EMIT sendGoSouthEast(); break;
    case EMovementDirection::South: Q_EMIT sendGoSouth(); break;
    case EMovementDirection::SouthWest: Q_EMIT sendGoSouthWest(); break;
    case EMovementDirection::West: Q_EMIT sendGoWest(); break;
    case EMovementDirection::NorthWest: Q_EMIT sendGoNorthWest(); break;
    default: break;
    }
}

void TPlayerMovementIntentHandler::sendRotate(const creatures::ECreatureLookDirection direction)
{
    switch (direction) {
    case creatures::ECreatureLookDirection::North: Q_EMIT sendRotateNorth(); break;
    case creatures::ECreatureLookDirection::East: Q_EMIT sendRotateEast(); break;
    case creatures::ECreatureLookDirection::South: Q_EMIT sendRotateSouth(); break;
    case creatures::ECreatureLookDirection::West: Q_EMIT sendRotateWest(); break;
    }
}

TPlayerMovementIntentHandler::EFieldState TPlayerMovementIntentHandler::fieldState(const TWorldMapCoordinate& coordinate) const
{
    const worldmap::TWorldMapField* field = m_env.map()->fieldAt(coordinate);
    if (!field)
        return EFieldState::Blocked;
    switch (field->walkState()) {
    case 0: return EFieldState::Free;
    case 1: return EFieldState::Blocked;
    default: return EFieldState::Unsure;
    }
}

}
