#include <gtest/gtest.h>

#include "animation/durationtween.h"

using tibia::animation::TDurationTween;

TEST(DurationTween, ProgressIsLinearAndClamped)
{
    TDurationTween t(1000, 200);
    EXPECT_EQ(t.v5(), 1000);
    EXPECT_EQ(t.v6(), 1200);
    EXPECT_FALSE(t.v3());
    EXPECT_DOUBLE_EQ(t.progress(900), 0.0);
    EXPECT_DOUBLE_EQ(t.progress(1050), 0.25);
    EXPECT_DOUBLE_EQ(t.progress(1300), 1.0);
}

TEST(DurationTween, AdvanceReportsTheFinishingCallOnce)
{
    TDurationTween t(0, 100);
    EXPECT_FALSE(t.v2(50));
    EXPECT_DOUBLE_EQ(t.v4(), 0.5);
    EXPECT_TRUE(t.v2(100));
    EXPECT_TRUE(t.v3());
    EXPECT_FALSE(t.v2(150));
    EXPECT_DOUBLE_EQ(t.v4(), 1.0);
}

TEST(DurationTween, EmptySpanIsFinishedFromTheStart)
{
    TDurationTween t(500, 0);
    EXPECT_TRUE(t.v3());
    EXPECT_FALSE(t.v2(500));
    EXPECT_DOUBLE_EQ(t.v4(), 0.0);
    EXPECT_FALSE(t.v2(501));
    EXPECT_DOUBLE_EQ(t.v4(), 1.0);
}

TEST(DurationTween, ReversedSpanIsFinishedAndStaysAtZero)
{
    TDurationTween t(500, -100);
    EXPECT_TRUE(t.v3());
    EXPECT_TRUE(t.isFinished(400));
    EXPECT_DOUBLE_EQ(t.progress(600), 0.0);
}

TEST(DurationTween, EqualityComparesEveryField)
{
    TDurationTween a(0, 100);
    TDurationTween b(0, 100);
    EXPECT_TRUE(a == b);
    b.v2(10);
    EXPECT_FALSE(a == b);
}
