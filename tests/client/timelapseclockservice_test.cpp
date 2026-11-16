#include <gtest/gtest.h>

#include "sessiondump/sessiondumpprogressclockservice.h"
#include "timelapseclockservice.h"

using tibia::sessiondump::TSessiondumpProgressClockService;
using tibia::shared::TTimeLapseClockService;

namespace {

constexpr qint64 Ms = 1000000;

/// A base clock the test moves by hand.
class TFakeClock : public tibia::shared::IClockService
{
public:
    qint64 nanos = 0;
    qint64 nowMillis() override { return nanos / 1000000; }
    qint64 nowNanos() override { return nanos; }
    qint64 nowMicros() override { return nanos / 1000; }
    QDateTime currentDateTime() override { return {}; }
};

}

TEST(TimeLapseClockService, RunsAtItsSpeedFromTheStartTime)
{
    auto fake = std::make_shared<TFakeClock>();
    qint64& base = fake->nanos;
    base = 1000 * Ms;
    TTimeLapseClockService clock(fake);
    EXPECT_EQ(clock.nowMillis(), 1000);
    base += 100 * Ms;
    EXPECT_EQ(clock.nowMillis(), 1100);
    clock.setSpeed(2.0);
    base += 100 * Ms;
    EXPECT_EQ(clock.nowMillis(), 1300);
    clock.pause();
    base += 500 * Ms;
    EXPECT_EQ(clock.nowMillis(), 1300);
    clock.resume();
    EXPECT_DOUBLE_EQ(clock.speed(), 2.0);
    base += 10 * Ms;
    EXPECT_EQ(clock.nowMillis(), 1320);
    clock.setStartTime(QDateTime::fromMSecsSinceEpoch(0));
    EXPECT_EQ(clock.nowMillis(), 320);
    clock.setElapsedNanos(5 * Ms);
    EXPECT_EQ(clock.nowMillis(), 5);
}

TEST(SessiondumpProgressClockService, ProgressPausesAndResumes)
{
    auto fake = std::make_shared<TFakeClock>();
    qint64& base = fake->nanos;
    TSessiondumpProgressClockService clock(fake);
    clock.start();
    EXPECT_TRUE(clock.isProgressRunning());
    base += 250 * Ms;
    EXPECT_EQ(clock.progress(), 250);
    clock.pauseProgress();
    EXPECT_FALSE(clock.isProgressRunning());
    base += 1000 * Ms;
    EXPECT_EQ(clock.progress(), 250);
    clock.setProgress(400);
    EXPECT_EQ(clock.progress(), 400);
    clock.resumeProgress();
    base += 100 * Ms;
    EXPECT_EQ(clock.progress(), 500);
    clock.setProgress(50);
    EXPECT_EQ(clock.progress(), 50);
}
