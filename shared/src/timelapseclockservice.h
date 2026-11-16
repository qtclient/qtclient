#pragma once

#include "iclockservice.h"

#include <QDateTime>
#include <QtGlobal>

#include <memory>

namespace tibia {
namespace shared {

/// CIP `shared::TTimeLapseClockService`: a clock that starts at the base
/// clock's current time and runs at an adjustable speed over it.
class TTimeLapseClockService : public IClockService
{
public:
    explicit TTimeLapseClockService(std::shared_ptr<IClockService> clockService);
    ~TTimeLapseClockService() override;

    qint64 nowMillis() override;
    /// The start time plus the base clock's elapsed time scaled by the speed.
    qint64 nowNanos() override;
    qint64 nowMicros() override;
    QDateTime currentDateTime() override;
    virtual qint64 startNanos() const;
    /// Stops the clock, remembering the speed.
    virtual void pause();
    virtual void resume();
    virtual void setSpeed(double speed);
    virtual void setStartTime(const QDateTime& startTime);
    virtual void setElapsedNanos(const qint64& elapsedNanos);
    double speed() const;

private:
    std::shared_ptr<IClockService> m_clockService;
    qint64 m_startNanos = 0;
    qint64 m_lastBaseNanos = 0;
    qint64 m_elapsedNanos = 0;
    double m_speed = 1.0;
    double m_pausedSpeed = 0.0;
};

}
}
