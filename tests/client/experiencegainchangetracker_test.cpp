// CIP shared::TExperienceGainChangeTracker / TNullExperienceGainChangeTracker
// and shared::TNullExperiencePerHourProvider.

#include "shared/experiencegainchangetracker.h"
#include "shared/nullexperiencegainchangetracker.h"
#include "shared/nullexperienceperhourprovider.h"

#include <gtest/gtest.h>

#include <limits>

using tibia::shared::TExperienceGainChangeTracker;
using tibia::shared::TNullExperienceGainChangeTracker;
using tibia::shared::TNullExperiencePerHourProvider;

TEST(ExperienceGainChangeTracker, ReportsEveryDifferenceFromTheLastRate)
{
    TExperienceGainChangeTracker tracker;
    EXPECT_FALSE(tracker.update(0.0)); // starts at 0
    EXPECT_TRUE(tracker.update(150.0));
    EXPECT_FALSE(tracker.update(150.0));
    EXPECT_TRUE(tracker.update(100.0));
}

TEST(ExperienceGainChangeTracker, NaNAlwaysCountsAsAChange)
{
    TExperienceGainChangeTracker tracker;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(tracker.update(nan));
    EXPECT_TRUE(tracker.update(nan));
    EXPECT_TRUE(tracker.update(0.0));
}

TEST(NullExperienceGainChangeTracker, NeverReportsAChange)
{
    TNullExperienceGainChangeTracker tracker;
    EXPECT_FALSE(tracker.update(0.0));
    EXPECT_FALSE(tracker.update(150.0));
}

TEST(NullExperiencePerHourProvider, BothRatesAreZero)
{
    const TNullExperiencePerHourProvider provider;
    EXPECT_EQ(provider.experiencePerHour(), 0.0);
    EXPECT_EQ(provider.secondExperiencePerHour(), 0.0);
}
