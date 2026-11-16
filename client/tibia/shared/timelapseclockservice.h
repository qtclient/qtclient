#pragma once

#include <QDateTime>
#include <QtGlobal>

#include <functional>

namespace tibia {
namespace shared {

/// CIP `shared::TTimeLapseClockService`: a clock that starts at real time and
/// runs at an adjustable speed over a base nanosecond clock.
class TTimeLapseClockService
{
public:
    /// `nanosecondClock` is the base clock (default: TClockService::nowNanos).
    explicit TTimeLapseClockService(std::function<qint64()> nanosecondClock = {});
    virtual ~TTimeLapseClockService();

    virtual qint64 nowMillis();
    /// The start time plus the base clock's elapsed time scaled by the speed.
    virtual qint64 nowNanos();
    virtual qint64 nowMicros();
    virtual QDateTime currentDateTime();
    virtual qint64 startNanos() const;
    /// Stops the clock, remembering the speed.
    virtual void pause();
    virtual void resume();
    virtual void setSpeed(double speed);
    virtual void setStartTime(const QDateTime& startTime);
    virtual void setElapsedNanos(const qint64& elapsedNanos);
    double speed() const;

private:
    void advance();

    std::function<qint64()> m_nanosecondClock;
    qint64 m_startNanos = 0;
    qint64 m_lastBaseNanos = 0;
    qint64 m_elapsedNanos = 0;
    double m_speed = 1.0;
    double m_pausedSpeed = 0.0;
};

}
}
