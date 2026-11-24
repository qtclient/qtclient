#include <gtest/gtest.h>

#include "systemsurvey/slidingfpscounter.h"

#include <QDateTime>

using tibia::systemsurvey::TSlidingFPSCounter;

namespace {

class TFakeClock final : public tibia::shared::IClockService
{
public:
    qint64 nowMillis() override { return millis; }
    qint64 nowNanos() override { return millis * 1000000; }
    qint64 nowMicros() override { return millis * 1000; }
    QDateTime currentDateTime() override { return {}; }

    qint64 millis = 0;
};

}

TEST(SlidingFPSCounter, NoRateAndNoDurationBeforeTwoFrames)
{
    auto clock = std::make_shared<TFakeClock>();
    TSlidingFPSCounter counter(1000, clock);
    EXPECT_EQ(counter.lastFrameDuration(), 0);
    clock->millis = 500;
    counter.startNewFrame();
    EXPECT_EQ(counter.framesPerSecond(), 0.0);
    EXPECT_EQ(counter.lastFrameDuration(), 0);
}

TEST(SlidingFPSCounter, MeasuresOnlyWhenTheSecondRollsOver)
{
    auto clock = std::make_shared<TFakeClock>();
    TSlidingFPSCounter counter(1000, clock);
    // 10 frames 100 ms apart from 100 to 1000: the second rolls over at 1000.
    for (qint64 t = 100; t < 1000; t += 100) {
        clock->millis = t;
        counter.startNewFrame();
        EXPECT_EQ(counter.framesPerSecond(), 0.0) << t;
    }
    EXPECT_EQ(counter.lastFrameDuration(), 100);
    clock->millis = 1000;
    counter.startNewFrame();
    // 10 frames over 900 ms.
    EXPECT_DOUBLE_EQ(counter.framesPerSecond(), 9 * 1000.0 / 900.0);
}

TEST(SlidingFPSCounter, DropsFramesOlderThanTheWindowButKeepsTwo)
{
    auto clock = std::make_shared<TFakeClock>();
    TSlidingFPSCounter counter(1000, clock);
    for (qint64 t = 0; t <= 3000; t += 250) {
        clock->millis = t;
        counter.startNewFrame();
    }
    // At 3000 the window keeps 2000..3000 (5 frames).
    EXPECT_DOUBLE_EQ(counter.framesPerSecond(), 4 * 1000.0 / 1000.0);
    // A long gap: only the last two frames stay.
    clock->millis = 9000;
    counter.startNewFrame();
    EXPECT_DOUBLE_EQ(counter.framesPerSecond(), 1000.0 / 6000.0);
    EXPECT_EQ(counter.lastFrameDuration(), 6000);
}
