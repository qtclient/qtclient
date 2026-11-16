#pragma once

#include "shared/timelapseclockservice.h"

#include <limits>

namespace tibia {
namespace sessiondump {

/// CIP `sessiondump::TSessiondumpProgressClockService`: the time-lapse clock of
/// a dump playback, measuring the progress in milliseconds since start().
class TSessiondumpProgressClockService : public shared::TTimeLapseClockService
{
public:
    explicit TSessiondumpProgressClockService(std::function<qint64()> nanosecondClock = {});
    ~TSessiondumpProgressClockService() override;

    virtual void start();
    /// Milliseconds since start(); frozen while the progress is paused.
    virtual qint64 progress();
    virtual void pauseProgress();
    virtual void resumeProgress();
    virtual bool isProgressRunning() const;
    virtual void setProgress(const qint64& progress);

private:
    static constexpr qint64 Running = std::numeric_limits<qint64>::max();

    qint64 m_startMillis = 0;
    qint64 m_pausedProgress = 0;
};

}
}
