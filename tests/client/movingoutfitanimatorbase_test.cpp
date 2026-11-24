#include <gtest/gtest.h>

#include "animation/movingoutfitanimator.h"

using tibia::animation::TAnimationPhaseDuration;
using tibia::animation::TAnimator;
using tibia::animation::TMovingOutfitAnimator;

// The moving outfit animator is an animator of one fixed phase of no duration.
TEST(MovingOutfitAnimatorBase, BaseAnimatorHasOneFixedPhaseOfNoDuration)
{
    const TMovingOutfitAnimator walk(8, false);
    const TAnimator& base = walk;
    ASSERT_EQ(base.info().phaseCount(), 1u);
    EXPECT_EQ(base.info().phases[0], TAnimationPhaseDuration(0, 0));
    EXPECT_EQ(base.phase(), 0);
}

// Updating through the animator interface computes the walking phase.
TEST(MovingOutfitAnimatorBase, UpdateThroughTheAnimatorSetsTheWalkingPhase)
{
    TMovingOutfitAnimator walk(8, false);
    walk.setKey(400, 1000);
    TAnimator& base = walk;
    base.update(1124);
    EXPECT_EQ(base.phase(), 2);
    base.update(1000 + 50 * 9);
    EXPECT_EQ(base.phase(), 1);
}
