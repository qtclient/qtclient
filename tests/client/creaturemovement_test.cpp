#include <gtest/gtest.h>

#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/movementdirection.h"
#include "creatures/creaturemovementtween.h"

#include <stdexcept>

using namespace tibia::creatures;
using tibia::worldmap::TWorldMapCoordinate;
using tibia::worldmap::TWorldMapExtent;
using tibia::worldmap::TWorldMapSubfieldExtent;

namespace {

TCreatureMovementSpeedCalculator crystalCalculator()
{
    TCreatureMovementSpeedCalculator calculator;
    calculator.setSpeedFormula(857.36, 261.29, -4795.01);
    calculator.setServerBeat(50);
    return calculator;
}

}

TEST(MovementDirection, OffsetsAndDirectionsRoundTrip)
{
    for (int d = 0; d < 8; ++d) {
        const auto direction = static_cast<EMovementDirection>(d);
        EXPECT_EQ(directionOfStep(stepOffset(direction)), direction);
        const TWorldMapExtent reverse = reverseStepOffset(direction);
        EXPECT_EQ(reverse.width, -stepOffset(direction).width);
        EXPECT_EQ(reverse.height, -stepOffset(direction).height);
    }
    EXPECT_TRUE(stepOffset(EMovementDirection::North) == TWorldMapExtent(0, -1, 0));
    EXPECT_TRUE(stepOffset(EMovementDirection::SouthWest) == TWorldMapExtent(-1, 1, 0));
    EXPECT_THROW(directionOfStep(TWorldMapExtent(2, 0, 0)), std::invalid_argument);
    EXPECT_THROW(directionOfStep(TWorldMapExtent(0, 0, 1)), std::invalid_argument);
}

TEST(MovementDirection, LookDirectionFollowsHorizontalComponent)
{
    using L = ECreatureLookDirection;
    using M = EMovementDirection;
    EXPECT_EQ(lookDirectionAfter(L::South, M::North), L::North);
    EXPECT_EQ(lookDirectionAfter(L::South, M::NorthEast), L::East);
    EXPECT_EQ(lookDirectionAfter(L::South, M::SouthEast), L::East);
    EXPECT_EQ(lookDirectionAfter(L::North, M::South), L::South);
    EXPECT_EQ(lookDirectionAfter(L::North, M::SouthWest), L::West);
    EXPECT_EQ(lookDirectionAfter(L::North, M::NorthWest), L::West);
    EXPECT_EQ(lookDirectionAfter(L::East, M::None), L::East);
    EXPECT_EQ(lookDirectionAfter(L::East, M::TurnSouth), L::South);
    EXPECT_TRUE(isDiagonal(M::NorthEast));
    EXPECT_FALSE(isDiagonal(M::East));
}

TEST(CreatureMovementSpeedCalculator, EffectiveSpeedUsesTheLogFormula)
{
    const auto calculator = crystalCalculator();
    EXPECT_EQ(calculator.effectiveSpeed(220), 500);
    EXPECT_EQ(calculator.effectiveSpeed(602), 1001);
    EXPECT_EQ(calculator.effectiveSpeed(1204), 1455);
}

TEST(CreatureMovementSpeedCalculator, StepDurationIsBeatRounded)
{
    const auto calculator = crystalCalculator();
    const TWorldMapExtent east(1, 0, 0);
    EXPECT_EQ(calculator.stepDuration(220, east, 150), 300);
    EXPECT_EQ(calculator.stepDuration(1204, east, 100), 100);
    EXPECT_EQ(calculator.stepDuration(1204, east, 200), 150);
    EXPECT_EQ(calculator.stepDuration(602, east, 200), 200);
}

TEST(CreatureMovementSpeedCalculator, OnlyTheDiagonalVariantTriplesGroundSpeed)
{
    const auto calculator = crystalCalculator();
    const TWorldMapExtent northEast(1, -1, 0);
    EXPECT_EQ(calculator.stepDuration(1204, northEast, 150), 150);
    EXPECT_EQ(calculator.stepDurationWithDiagonalCost(1204, northEast, 150), 350);
    EXPECT_EQ(calculator.stepDuration(1204, northEast, 150, false), 150);
    EXPECT_EQ(calculator.stepDurationWithDiagonalCost(1204, TWorldMapExtent(1, 0, 0), 150), 150);
    EXPECT_THROW(calculator.stepDuration(1204, TWorldMapExtent(2, 0, 0), 150), std::invalid_argument);
}

TEST(CreatureMovementSpeedCalculator, NoFormulaBeforeLogin)
{
    TCreatureMovementSpeedCalculator calculator;
    EXPECT_EQ(calculator.effectiveSpeed(220), 0);
    EXPECT_EQ(calculator.serverBeat(), 0);
}

// Mode 1: the placeholder is on the destination; covered field and offset are
// relative to it.
TEST(CreatureMovementTween, SlidesInFromBehindTheDestination)
{
    TCreatureMovementTween tween;
    tween.start(1000, 1200, TWorldMapCoordinate(99, 100, 7), EMovementDirection::East, 2, 5, 1);
    EXPECT_TRUE(tween.destination() == TWorldMapCoordinate(100, 100, 7));
    EXPECT_FALSE(tween.isFinished());

    tween.advance(1000);
    EXPECT_FALSE(tween.isPastHalf());
    EXPECT_TRUE(tween.coveredFieldOffset() == TWorldMapExtent(-1, 0, 0));
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(0, 0));
    EXPECT_EQ(tween.elevation(), 2);

    tween.advance(1050);
    EXPECT_TRUE(tween.coveredFieldOffset() == TWorldMapExtent(-1, 0, 0));
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(8, 0));

    tween.advance(1100);
    EXPECT_TRUE(tween.isPastHalf());
    EXPECT_TRUE(tween.coveredFieldOffset() == TWorldMapExtent(0, 0, 0));
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(-16, 0));
    EXPECT_EQ(tween.elevation(), 5);

    EXPECT_TRUE(tween.advance(1200));
    EXPECT_TRUE(tween.isFinished());
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(0, 0));
}

// Mode 0: the placeholder is still on the origin; the creature moves ahead of it.
TEST(CreatureMovementTween, PredictedStepMovesAheadOfThePlaceholder)
{
    TCreatureMovementTween tween;
    tween.start(0, 200, TWorldMapCoordinate(50, 50, 7), EMovementDirection::South, 0, 0, 0);
    tween.advance(0);
    EXPECT_TRUE(tween.coveredFieldOffset() == TWorldMapExtent(0, 0, 0));
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(0, 0));
    tween.advance(50);
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(0, 8));
    tween.advance(150);
    EXPECT_TRUE(tween.coveredFieldOffset() == TWorldMapExtent(0, 1, 0));
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(0, -8));
    EXPECT_FALSE(tween.isFinished());
    tween.advance(200);
    EXPECT_FALSE(tween.isFinished());
    tween.setMode(1);
    EXPECT_TRUE(tween.isFinished());
}

TEST(CreatureMovementTween, ChainedStepKeepsPartOfTheOldStep)
{
    TCreatureMovementTween tween;
    tween.start(0, 200, TWorldMapCoordinate(10, 9, 7), EMovementDirection::South, 0, 0, 1);
    tween.advance(150);
    tween.start(150, 350, TWorldMapCoordinate(10, 10, 7), EMovementDirection::South, 0, 0, 1);
    EXPECT_DOUBLE_EQ(tween.continueScale(), 1.25);

    tween.advance(150);
    EXPECT_DOUBLE_EQ(tween.overshoot(), 0.25);
    EXPECT_TRUE(tween.remainingOffset() == TWorldMapSubfieldExtent(0, -40));

    tween.advance(250);
    EXPECT_DOUBLE_EQ(tween.overshoot(), 0.0);
    EXPECT_TRUE(tween.remainingOffset() == TWorldMapSubfieldExtent(0, -20));
}

TEST(CreatureMovementTween, ATurnStillChainsButAJumpDoesNot)
{
    TCreatureMovementTween tween;
    tween.start(0, 200, TWorldMapCoordinate(10, 9, 7), EMovementDirection::South, 0, 0, 1);
    tween.advance(150);
    tween.start(150, 350, TWorldMapCoordinate(10, 10, 7), EMovementDirection::East, 0, 0, 1);
    EXPECT_DOUBLE_EQ(tween.continueScale(), 1.25);
    tween.advance(150);
    EXPECT_TRUE(tween.remainingOffset() == TWorldMapSubfieldExtent(-32, -8));
    tween.advance(200);
    EXPECT_TRUE(tween.remainingOffset() == TWorldMapSubfieldExtent(-30, 0));

    tween.start(200, 400, TWorldMapCoordinate(40, 40, 7), EMovementDirection::East, 0, 0, 1);
    EXPECT_DOUBLE_EQ(tween.continueScale(), 1.0);
}

TEST(CreatureMovementTween, StopSnapsToTheTarget)
{
    TCreatureMovementTween tween;
    tween.start(0, 200, TWorldMapCoordinate(10, 10, 7), EMovementDirection::West, 0, 0, 1);
    tween.advance(50);
    tween.stop();
    EXPECT_TRUE(tween.isFinished());
    EXPECT_TRUE(tween.drawOffset() == TWorldMapSubfieldExtent(0, 0));
}

#include "animation/movingoutfitanimator.h"

using tibia::animation::TMovingOutfitAnimator;

TEST(MovingOutfitAnimator, PhaseDurationIsClampedToTheCycleLimits)
{
    TMovingOutfitAnimator walk(8, false);
    EXPECT_EQ(walk.minimumPhaseDuration(), 35);
    EXPECT_EQ(walk.maximumPhaseDuration(), 90);
    walk.setKey(150, 0);
    EXPECT_EQ(walk.phaseDuration(), 35);
    walk.setKey(400, 0);
    EXPECT_EQ(walk.phaseDuration(), 50);
    walk.setKey(2000, 0);
    EXPECT_EQ(walk.phaseDuration(), 90);
}

TEST(MovingOutfitAnimator, PhasesAdvanceWithTheStepClock)
{
    TMovingOutfitAnimator walk(8, false);
    walk.setKey(400, 1000);
    walk.update(1000);
    EXPECT_EQ(walk.phase(), 0u);
    walk.update(1124);
    EXPECT_EQ(walk.phase(), 2u);
    walk.update(1000 + 50 * 9);
    EXPECT_EQ(walk.phase(), 1u);
}

TEST(MovingOutfitAnimator, PingPongFoldsBack)
{
    EXPECT_EQ(TMovingOutfitAnimator::pingPongPhase(3, 0), 0u);
    EXPECT_EQ(TMovingOutfitAnimator::pingPongPhase(3, 2), 2u);
    EXPECT_EQ(TMovingOutfitAnimator::pingPongPhase(3, 3), 1u);
    EXPECT_EQ(TMovingOutfitAnimator::pingPongPhase(3, 4), 0u);
    EXPECT_EQ(TMovingOutfitAnimator::pingPongPhase(1, 5), 0u);
    TMovingOutfitAnimator walk(3, true);
    EXPECT_EQ(walk.minimumPhaseDuration(), 70);
}

TEST(MovingOutfitAnimator, ChangingTheKeyKeepsThePhase)
{
    TMovingOutfitAnimator walk(8, false);
    walk.setKey(400, 0);
    walk.update(125);
    EXPECT_EQ(walk.phase(), 2u);
    walk.setKey(720, 125);
    walk.update(125);
    EXPECT_EQ(walk.phase(), 2u);
    walk.update(125 + 90);
    EXPECT_EQ(walk.phase(), 3u);
}

TEST(MovementDirection, DirectionBetweenFields)
{
    using M = EMovementDirection;
    const TWorldMapCoordinate c(10, 10, 7);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(10, 9, 7)), M::North);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(11, 9, 7)), M::NorthEast);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(11, 11, 7)), M::SouthEast);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(9, 11, 7)), M::SouthWest);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(9, 9, 7)), M::NorthWest);
    EXPECT_EQ(directionBetween(c, c), M::None);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(15, 3, 7)), M::TurnEast);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(10, 3, 7)), M::TurnNorth);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(10, 13, 7)), M::TurnSouth);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(4, 10, 7)), M::TurnWest);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(10, 10, 6)), M::None);
    EXPECT_EQ(directionBetween(c, TWorldMapCoordinate(11, 10, 6)), M::TurnEast);
}
