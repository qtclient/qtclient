#include <gtest/gtest.h>

#include "input/playermovementintent.h"

using namespace tibia::input;

TEST(MovementIntent, DifferentKindsAreNotEqual)
{
    const TMovementIntentDirection direction(1);
    const TMovementIntentRotate rotate(1);
    const TMovementIntentStop stop;
    const TMovementIntentTargetCoordinate target(100, 200, 7);
    EXPECT_FALSE(direction == rotate);
    EXPECT_FALSE(rotate == stop);
    EXPECT_FALSE(stop == target);
    EXPECT_TRUE(target != direction);
}

TEST(MovementIntent, StopsAreAlwaysEqual)
{
    const TMovementIntentStop a;
    const TMovementIntentStop b;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(MovementIntent, DirectionsAndRotationsCompareTheirDirection)
{
    EXPECT_TRUE(TMovementIntentDirection(2) == TMovementIntentDirection(2));
    EXPECT_FALSE(TMovementIntentDirection(2) == TMovementIntentDirection(3));
    EXPECT_TRUE(TMovementIntentRotate(0) == TMovementIntentRotate(0));
    EXPECT_TRUE(TMovementIntentRotate(0) != TMovementIntentRotate(1));
}

TEST(MovementIntent, TargetsCompareCoordinateAndUnwalkableGoal)
{
    const TMovementIntentTargetCoordinate target(100, 200, 7, true);
    EXPECT_TRUE(target == TMovementIntentTargetCoordinate(100, 200, 7, true));
    EXPECT_FALSE(target == TMovementIntentTargetCoordinate(100, 200, 7, false));
    EXPECT_FALSE(target == TMovementIntentTargetCoordinate(101, 200, 7, true));
    EXPECT_FALSE(target == TMovementIntentTargetCoordinate(100, 201, 7, true));
    EXPECT_FALSE(target == TMovementIntentTargetCoordinate(100, 200, 6, true));
}
