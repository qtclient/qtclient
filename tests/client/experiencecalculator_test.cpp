#include <gtest/gtest.h>

#include "game/experiencecalculator.h"

using tibia::game::TExperienceCalculator;

namespace {
constexpr qint64 kMinute = 60000;
}

TEST(ExperienceCalculator, SameMinuteSharesABucket)
{
    TExperienceCalculator calc(10);
    calc.addExperience(100, 5 * kMinute);
    calc.addExperience(50, 5 * kMinute + 30000);
    calc.addExperience(250, 6 * kMinute);
    // (150 + 250) over the 10-minute window.
    EXPECT_EQ(calc.experiencePerMinute(6 * kMinute), 40);
    EXPECT_EQ(calc.experiencePerHour(6 * kMinute), 2400);
}

TEST(ExperienceCalculator, OldMinutesLeaveTheWindow)
{
    TExperienceCalculator calc(3);
    calc.addExperience(300, 1 * kMinute);
    calc.addExperience(600, 3 * kMinute);
    EXPECT_EQ(calc.experiencePerMinute(3 * kMinute), 300);
    // At minute 4 the window is minutes 2..4: minute 1 drops out.
    EXPECT_EQ(calc.experiencePerMinute(4 * kMinute), 200);
    EXPECT_EQ(calc.experiencePerMinute(10 * kMinute), 0);
}

TEST(ExperienceCalculator, ClearEmptiesTheWindow)
{
    TExperienceCalculator calc(5);
    calc.addExperience(500, kMinute);
    calc.clear();
    EXPECT_EQ(calc.experiencePerMinute(kMinute), 0);
}

TEST(ExperienceCalculator, ExperienceForLevel)
{
    EXPECT_EQ(TExperienceCalculator::experienceForLevel(1), 0);
    EXPECT_EQ(TExperienceCalculator::experienceForLevel(2), 100);
    EXPECT_EQ(TExperienceCalculator::experienceForLevel(8), 4200);
    EXPECT_EQ(TExperienceCalculator::experienceForLevel(100), 15694800);
}

TEST(ExperienceCalculator, LevelProgress)
{
    EXPECT_FLOAT_EQ(TExperienceCalculator::levelProgress(125, 2), 0.25f);
    EXPECT_FLOAT_EQ(TExperienceCalculator::levelProgress(100, 2), 0.0f);
    // At or past the next level: no progress.
    EXPECT_FLOAT_EQ(TExperienceCalculator::levelProgress(300, 2), 0.0f);
}
