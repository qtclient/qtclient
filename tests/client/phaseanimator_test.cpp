#include <gtest/gtest.h>

#include "animation/phaseanimator.h"

using tibia::animation::TAnimationInfo;
using tibia::animation::TAnimator;
using L = TAnimationInfo::ELoopType;

namespace {

TAnimationInfo info(std::vector<qint64> durations, const L loop, const quint32 loopCount = 0, const bool synchronized = false)
{
    TAnimationInfo result;
    for (const qint64 duration : durations)
        result.phases.emplace_back(duration, duration);
    result.loopType = loop;
    result.loopCount = loopCount;
    result.synchronized = synchronized;
    result.finish();
    return result;
}

int phaseAt(TAnimator& animator, const qint64 nowMs)
{
    animator.update(nowMs);
    return animator.phase();
}

}

TEST(PhaseAnimator, CycleLengths)
{
    const TAnimationInfo loop = info({ 100, 200, 300 }, L::Infinite);
    EXPECT_TRUE(loop.fixedDurations);
    EXPECT_EQ(loop.minimumCycle, 600);
    EXPECT_EQ(loop.maximumCycle, 600);
    // A ping-pong cycle passes the inner phases twice.
    const TAnimationInfo pingPong = info({ 100, 200, 300 }, L::PingPong);
    EXPECT_EQ(pingPong.maximumCycle, 800);
    TAnimationInfo random = loop;
    random.phases[1] = { 150, 250 };
    random.finish();
    EXPECT_FALSE(random.fixedDurations);
    EXPECT_EQ(random.minimumCycle, 550);
    EXPECT_EQ(random.maximumCycle, 650);
}

TEST(PhaseAnimator, InfiniteLoopFollowsTheClock)
{
    TAnimator animator(info({ 100, 200, 300 }, L::Infinite), 0);
    EXPECT_EQ(phaseAt(animator, 0), 0);
    EXPECT_EQ(phaseAt(animator, 150), 1);
    EXPECT_EQ(phaseAt(animator, 350), 2);
    EXPECT_EQ(phaseAt(animator, 650), 0);
    // Far ahead, whole cycles are skipped: 10000 lies 400 ms into a cycle.
    EXPECT_EQ(phaseAt(animator, 10'000), 2);
    EXPECT_FALSE(animator.isFinished());
}

TEST(PhaseAnimator, PingPongTurnsAtBothEnds)
{
    TAnimator animator(info({ 100, 100, 100 }, L::PingPong), 0);
    EXPECT_EQ(phaseAt(animator, 50), 0);
    EXPECT_EQ(phaseAt(animator, 150), 1);
    EXPECT_EQ(phaseAt(animator, 250), 2);
    EXPECT_EQ(phaseAt(animator, 350), 1);
    EXPECT_EQ(phaseAt(animator, 450), 0);
    EXPECT_EQ(phaseAt(animator, 550), 1);
}

TEST(PhaseAnimator, CountedAnimationFinishesOnItsLastPhase)
{
    TAnimator animator(info({ 100, 100 }, L::Counted, 2), 0);
    EXPECT_EQ(phaseAt(animator, 50), 0);
    EXPECT_EQ(phaseAt(animator, 150), 1);
    EXPECT_EQ(phaseAt(animator, 250), 0);
    EXPECT_EQ(phaseAt(animator, 350), 1);
    EXPECT_FALSE(animator.isFinished());
    EXPECT_EQ(phaseAt(animator, 450), 1);
    EXPECT_TRUE(animator.isFinished());
    EXPECT_EQ(phaseAt(animator, 5'000), 1);
}

TEST(PhaseAnimator, AlwaysLoopIgnoresTheCount)
{
    TAnimator animator(info({ 100, 100 }, L::Counted, 1), 0);
    animator.setAlwaysLoop(true);
    EXPECT_EQ(phaseAt(animator, 250), 0);
    EXPECT_EQ(phaseAt(animator, 350), 1);
    EXPECT_FALSE(animator.isFinished());
}

TEST(PhaseAnimator, StartTimeUnlessSynchronized)
{
    TAnimator late(info({ 100, 200, 300 }, L::Infinite), 1'000);
    EXPECT_EQ(phaseAt(late, 1'050), 0);
    EXPECT_EQ(phaseAt(late, 1'150), 1);
    // A synchronized animation ignores the start: 1050 is 450 ms into a cycle.
    TAnimator synchronized(info({ 100, 200, 300 }, L::Infinite, 0, true), 1'000);
    EXPECT_EQ(phaseAt(synchronized, 1'050), 2);
}

TEST(PhaseAnimator, RandomDurationsStayInRange)
{
    TAnimationInfo random;
    random.phases = { { 100, 200 }, { 100, 200 } };
    random.finish();
    for (int i = 0; i < 100; ++i) {
        const qint64 duration = TAnimator::phaseDuration(random, 0);
        EXPECT_GE(duration, 100);
        EXPECT_LE(duration, 200);
    }
}

TEST(PhaseAnimator, NoCycleNoAnimation)
{
    TAnimator animator(info({ 0, 0 }, L::Infinite), 0);
    EXPECT_EQ(phaseAt(animator, 1'000), 0);
}
