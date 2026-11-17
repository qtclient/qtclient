#pragma once

#include "iclockservice.h"

#include <QObject>
#include <QtGlobal>

#include <limits>

#include <memory>

namespace tibia {
namespace systemsurvey {

/// CIP `systemsurvey::TFPSCounter`: counts the frames started since the last
/// reset and keeps the shortest and the longest time between two of them
/// (nanoseconds of the clock service).
class TFPSCounter : public QObject
{
    Q_OBJECT
public:
    explicit TFPSCounter(std::shared_ptr<shared::IClockService> clockService, QObject* parent = nullptr);
    ~TFPSCounter() override;

    bool operator==(const TFPSCounter& other) const;

    virtual void reset();
    /// The rate of the shortest frame.
    virtual double maximumFPS() const;
    /// The rate of the longest frame.
    virtual double minimumFPS() const;
    /// The rate of the mean frame; 0 below two frames.
    virtual double averageFPS() const;
    virtual qint64 frameCount() const;
    /// From the first to the last frame started.
    virtual qint64 measuredDuration() const;
    /// Frames per second of a frame `frameDuration` nanoseconds long; 0 for 0.
    virtual double framesPerSecond(qint64 frameDuration) const;

public Q_SLOTS:
    /// A frame starts now.
    virtual void startNewFrame();

private:
    std::shared_ptr<shared::IClockService> m_clockService;
    qint64 m_frameCount = 0;
    qint64 m_firstFrameTime = 0;
    qint64 m_lastFrameTime = 0;
    qint64 m_maximumFrameDuration = 0;
    qint64 m_minimumFrameDuration = std::numeric_limits<qint64>::max();
};

}
}
