#include <gtest/gtest.h>

#include "animation/phaseanimator.h"
#include "randomnumbergenerator.h"

#include <memory>
#include <utility>
#include <vector>

using tibia::animation::TAnimationInfo;
using tibia::animation::TAnimator;
using tibia::shared::TRandomNumberGenerator;

namespace {

/// Answers every range with its maximum and records the ranges asked for.
class TMaximumGenerator final : public TRandomNumberGenerator
{
public:
    qint64 getRandomNumber(const qint64 min, const qint64 max) override
    {
        ranges.emplace_back(min, max);
        return max;
    }

    std::vector<std::pair<qint64, qint64>> ranges;
};

TAnimationInfo randomPhases()
{
    TAnimationInfo result;
    result.phases.emplace_back(100, 300);
    result.phases.emplace_back(50, 50);
    result.loopType = TAnimationInfo::ELoopType::Infinite;
    result.finish();
    return result;
}

}

TEST(AnimatorRandomSource, PhaseDurationsComeFromTheGeneratorSet)
{
    const auto generator = std::make_shared<TMaximumGenerator>();
    TAnimator::setRandomNumberGenerator(generator);
    const TAnimationInfo info = randomPhases();
    EXPECT_EQ(TAnimator::phaseDuration(info, 0), 300);
    // A fixed phase does not ask the generator.
    EXPECT_EQ(TAnimator::phaseDuration(info, 1), 50);
    ASSERT_EQ(generator->ranges.size(), 1u);
    EXPECT_EQ(generator->ranges[0], std::make_pair(qint64{ 100 }, qint64{ 300 }));
    // A new animator draws its first phase from it too.
    const TAnimator animator(info, 1'000);
    EXPECT_EQ(animator.nextPhaseMs(), 1'300);
    TAnimator::setRandomNumberGenerator(nullptr);
}

TEST(AnimatorRandomSource, WithoutOneTheProcessGeneratorIsUsed)
{
    TAnimator::setRandomNumberGenerator(nullptr);
    const TAnimationInfo info = randomPhases();
    for (int i = 0; i < 50; ++i) {
        const qint64 duration = TAnimator::phaseDuration(info, 0);
        EXPECT_GE(duration, 100);
        EXPECT_LE(duration, 300);
    }
}
