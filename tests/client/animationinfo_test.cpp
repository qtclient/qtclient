#include <gtest/gtest.h>

#include "animation/animationinfo.h"

using tibia::animation::TAnimationInfo;
using tibia::animation::TAnimationPhaseDuration;

TEST(AnimationPhaseDuration, ConstructAccumulateCompare)
{
    TAnimationPhaseDuration zero;
    EXPECT_EQ(zero.minimum(), 0);
    EXPECT_EQ(zero.maximum(), 0);
    const TAnimationPhaseDuration fixed(250);
    EXPECT_TRUE(fixed.isFixed());
    const TAnimationPhaseDuration range(100, 300);
    EXPECT_FALSE(range.isFixed());
    zero += range;
    zero += fixed;
    EXPECT_EQ(zero, TAnimationPhaseDuration(350, 550));
    EXPECT_FALSE(zero == range);
}

TEST(AnimationInfo, AccessorsAndBuilders)
{
    TAnimationInfo info;
    EXPECT_TRUE(info.fixedDurations);
    info.phases.emplace_back(100, 200);
    info.phases.emplace_back(50, 50);
    EXPECT_EQ(info.minimumPhaseDuration(0), 100);
    EXPECT_EQ(info.maximumPhaseDuration(0), 200);
    EXPECT_EQ(info.maximumPhaseDuration(1), 50);
    info.setCounted().setLoopCount(3);
    EXPECT_EQ(info.loopType, TAnimationInfo::ELoopType::Counted);
    EXPECT_EQ(info.loopCount, 3u);
    info.finish();
    EXPECT_EQ(info.minimumCycleDuration(), 150);
}

TEST(AnimationInfo, EqualityIgnoresDerivedFields)
{
    TAnimationInfo a;
    a.phases.emplace_back(100, 200);
    TAnimationInfo b = a;
    b.finish();
    EXPECT_TRUE(a == b);
    b.synchronized = true;
    EXPECT_FALSE(a == b);
    b = a;
    b.setLoopCount(2);
    EXPECT_FALSE(a == b);
    b = a;
    b.phases[0] = TAnimationPhaseDuration(100, 201);
    EXPECT_FALSE(a == b);
}
