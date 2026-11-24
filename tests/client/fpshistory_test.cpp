#include <gtest/gtest.h>

#include "systemsurvey/fpscounter.h"
#include "systemsurvey/fpshistory.h"
#include "systemsurvey/fpshistorycontroller.h"
#include "systemsurvey/fpshistorytojsonconverter.h"
#include "systemsurvey/fpshistoryuploader.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTimeZone>

#include "client/gameapplication.h"

using namespace tibia::systemsurvey;

// The test builds the FPS classes alone: the client id comes from here.
QString tibia::client::TGameApplication::hardwareId() { return QStringLiteral("0123456789abcdef"); }

namespace {

class TFakeClock final : public tibia::shared::IClockService
{
public:
    qint64 nowMillis() override { return nanos / 1000000; }
    qint64 nowNanos() override { return nanos; }
    qint64 nowMicros() override { return nanos / 1000; }
    QDateTime currentDateTime() override { return dateTime; }

    qint64 nanos = 0;
    QDateTime dateTime;
};

class TRecordingUploader final : public TFPSHistoryUploader
{
public:
    TRecordingUploader()
        : TFPSHistoryUploader(QUrl(), std::make_shared<TFPSHistoryToJsonConverter>(), nullptr)
    {
    }
    void uploadHistory(const TFPSHistory& history, int renderer) override
    {
        uploadedSamples = history.size();
        uploadedRenderer = renderer;
        ++uploads;
    }
    qint64 uploadedSamples = 0;
    int uploadedRenderer = 0;
    int uploads = 0;
};

}

TEST(FPSCounter, KeepsShortestAndLongestFrameAndMeanRate)
{
    auto clock = std::make_shared<TFakeClock>();
    TFPSCounter counter(clock);
    EXPECT_EQ(counter.averageFPS(), 0.0);
    EXPECT_EQ(counter.minimumFPS(), 0.0);

    clock->nanos = 1000;
    counter.startNewFrame();
    clock->nanos += 10000000; // 10 ms
    counter.startNewFrame();
    clock->nanos += 40000000; // 40 ms
    counter.startNewFrame();

    EXPECT_EQ(counter.frameCount(), 3);
    EXPECT_EQ(counter.measuredDuration(), 50000000);
    EXPECT_DOUBLE_EQ(counter.maximumFPS(), 100.0);
    EXPECT_DOUBLE_EQ(counter.minimumFPS(), 25.0);
    EXPECT_DOUBLE_EQ(counter.averageFPS(), 40.0);

    counter.reset();
    EXPECT_EQ(counter.frameCount(), 0);
    EXPECT_EQ(counter.measuredDuration(), 0);
    EXPECT_DOUBLE_EQ(counter.maximumFPS(), 1000000000.0 / double(std::numeric_limits<qint64>::max()));
}

TEST(FPSCounter, AverageUsesWholeNanosecondsPerFrame)
{
    auto clock = std::make_shared<TFakeClock>();
    TFPSCounter counter(clock);
    counter.startNewFrame();
    clock->nanos = 3;
    counter.startNewFrame();
    clock->nanos = 10;
    counter.startNewFrame();
    // 10 ns over 2 frames is 5 ns each.
    EXPECT_DOUBLE_EQ(counter.averageFPS(), 200000000.0);
}

TEST(FPSHistory, SamplesTheCounterAndResetsIt)
{
    auto clock = std::make_shared<TFakeClock>();
    auto counter = std::make_shared<TFPSCounter>(clock);
    TFPSHistory history(counter, clock);
    clock->dateTime = QDateTime(QDate(2026, 11, 16), QTime(12, 30, 5, 42), QTimeZone::UTC);

    counter->startNewFrame();
    clock->nanos = 20000000;
    counter->startNewFrame();
    history.sampleCurrentFPSInformationAndClearFPSCounter();

    ASSERT_EQ(history.size(), 1);
    const TFPSSample& sample = history.samples().front();
    EXPECT_DOUBLE_EQ(sample.maximumFPS, 50.0);
    EXPECT_DOUBLE_EQ(sample.minimumFPS, 50.0);
    EXPECT_DOUBLE_EQ(sample.averageFPS, 50.0);
    EXPECT_EQ(sample.time, clock->dateTime);
    EXPECT_EQ(counter->frameCount(), 0);

    history.clear();
    EXPECT_EQ(history.size(), 0);
}

TEST(FPSHistoryToJsonConverter, WritesRoundedRatesAndTimeStamps)
{
    auto clock = std::make_shared<TFakeClock>();
    auto counter = std::make_shared<TFPSCounter>(clock);
    TFPSHistory history(counter, clock);
    counter->startNewFrame();
    clock->nanos = 16000000; // 62.5 fps
    counter->startNewFrame();
    clock->nanos = 46000000; // 33.3 fps
    counter->startNewFrame();
    history.addSample(QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5, 6)));

    const QJsonObject root = TFPSHistoryToJsonConverter().convert(history, 3).object();
    EXPECT_EQ(root.value(QStringLiteral("dataType")).toInt(), 1);
    EXPECT_EQ(root.value(QStringLiteral("renderer")).toInt(), 3);
    EXPECT_EQ(root.value(QStringLiteral("clientID")).toString(), QStringLiteral("0123456789abcdef"));
    const QJsonArray samples = root.value(QStringLiteral("FPSHistory")).toArray();
    ASSERT_EQ(samples.size(), 1);
    const QJsonObject sample = samples.at(0).toObject();
    EXPECT_EQ(sample.value(QStringLiteral("maxFPS")).toInt(), 63);
    EXPECT_EQ(sample.value(QStringLiteral("minFPS")).toInt(), 33);
    EXPECT_EQ(sample.value(QStringLiteral("averageFPS")).toInt(), 43);
    EXPECT_EQ(sample.value(QStringLiteral("timeStamp")).toString(), QStringLiteral("2026-01-02 03:04:05,006"));
}

TEST(FPSHistoryController, UploadsOnlyAHistoryWithSamplesAndClearsIt)
{
    auto clock = std::make_shared<TFakeClock>();
    auto history = std::make_shared<TFPSHistory>(std::make_shared<TFPSCounter>(clock), clock);
    auto uploader = std::make_shared<TRecordingUploader>();
    TFPSHistoryController controller(uploader, history, 2);

    controller.uploadHistoryAndClearIt();
    EXPECT_EQ(uploader->uploads, 0);

    history->addSample(QDateTime());
    history->addSample(QDateTime());
    controller.uploadHistoryAndClearIt();
    EXPECT_EQ(uploader->uploads, 1);
    EXPECT_EQ(uploader->uploadedSamples, 2);
    EXPECT_EQ(uploader->uploadedRenderer, 2);
    EXPECT_EQ(history->size(), 0);
}
