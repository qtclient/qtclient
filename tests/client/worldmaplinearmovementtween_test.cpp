#include <gtest/gtest.h>

#include "animation/worldmaplinearmovementtween.h"

using tibia::animation::TWorldMapLinearMovementTween;
using tibia::worldmap::TWorldMapCoordinate;
using tibia::worldmap::TWorldMapSubfieldCoordinate;

TEST(WorldMapLinearMovementTween, StartsAtFromAndMovesLinearly)
{
    TWorldMapLinearMovementTween tween(1000, 1400, { 100, 200, 7 }, { 164, 232, 7 });
    EXPECT_EQ(tween.position(), TWorldMapSubfieldCoordinate(100, 200, 7));
    EXPECT_FALSE(tween.advance(1100));
    EXPECT_EQ(tween.position(), TWorldMapSubfieldCoordinate(116, 208, 7));
    EXPECT_FALSE(tween.advance(1300));
    EXPECT_EQ(tween.position(), TWorldMapSubfieldCoordinate(148, 224, 7));
}

TEST(WorldMapLinearMovementTween, ReportsTheFinishingCallOnceAndStaysAtTheEnd)
{
    TWorldMapLinearMovementTween tween(0, 100, { 0, 0, 7 }, { 32, -32, 7 });
    EXPECT_TRUE(tween.advance(100));
    // The rounding truncates toward zero, so a negative axis ends one sub-field
    // short of `to`: -32 + 0.5 = -31.5 -> -31.
    EXPECT_EQ(tween.position(), TWorldMapSubfieldCoordinate(32, -31, 7));
    EXPECT_FALSE(tween.advance(500));
    EXPECT_EQ(tween.position(), TWorldMapSubfieldCoordinate(32, -31, 7));
}

TEST(WorldMapLinearMovementTween, RoundsEachStepByAddingAHalfAndTruncating)
{
    TWorldMapLinearMovementTween tween(0, 10, { 0, 0, 0 }, { 8, -8, 0 });
    // 8 * 0.3 = 2.4 -> 2; -8 * 0.3 = -2.4 -> (int)-1.9 = -1, not -2.
    EXPECT_EQ(tween.positionAt(0.3), TWorldMapSubfieldCoordinate(2, -1, 0));
    // 8 * 0.7 = 5.6 -> 6; -5.6 -> (int)-5.1 = -5.
    EXPECT_EQ(tween.positionAt(0.7), TWorldMapSubfieldCoordinate(6, -5, 0));
}

TEST(WorldMapLinearMovementTween, KeepsTheFloorOfFrom)
{
    TWorldMapLinearMovementTween tween(0, 10, { 0, 0, 6 }, { 32, 32, 8 });
    EXPECT_EQ(tween.positionAt(0.5).z, 6);
    EXPECT_EQ(tween.positionAt(1.0).z, 6);
}

TEST(WorldMapLinearMovementTween, EmptySpanFollowsTheDurationTween)
{
    // An empty span is finished from the start, so no advance reports the
    // finishing edge; progress is 0 at the start time and 1 after it.
    TWorldMapLinearMovementTween tween(50, 50, { 0, 0, 7 }, { 64, 0, 7 });
    EXPECT_TRUE(tween.v3());
    EXPECT_FALSE(tween.advance(50));
    EXPECT_EQ(tween.position(), TWorldMapSubfieldCoordinate(0, 0, 7));
    EXPECT_FALSE(tween.advance(51));
    EXPECT_EQ(tween.position(), TWorldMapSubfieldCoordinate(64, 0, 7));
}

TEST(WorldMapSubfieldCoordinate, FromFieldElevatedIsTheTopLeftSubfieldMovedUpAndLeft)
{
    EXPECT_EQ(TWorldMapSubfieldCoordinate::fromFieldElevated(TWorldMapCoordinate(10, 20, 7), 0),
              TWorldMapSubfieldCoordinate(320, 640, 7));
    EXPECT_EQ(TWorldMapSubfieldCoordinate::fromFieldElevated(TWorldMapCoordinate(10, 20, 7), 8),
              TWorldMapSubfieldCoordinate(312, 632, 7));
}
