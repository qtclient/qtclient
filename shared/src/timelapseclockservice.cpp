#include "timelapseclockservice.h"


namespace tibia::shared {

TTimeLapseClockService::TTimeLapseClockService(std::shared_ptr<IClockService> clockService)
    : m_clockService(std::move(clockService))
{
    const qint64 now = m_clockService->nowNanos();
    m_startNanos = now;
    m_lastBaseNanos = now;
}

TTimeLapseClockService::~TTimeLapseClockService() = default;

qint64 TTimeLapseClockService::nowMillis() { return nowNanos() / 1000000; }

qint64 TTimeLapseClockService::nowNanos()
{
    const qint64 now = m_clockService->nowNanos();
    const qint64 last = m_lastBaseNanos;
    m_lastBaseNanos = now;
    if (m_speed != 0.0)
        m_elapsedNanos += qint64(m_speed * double(now - last));
    return m_elapsedNanos + m_startNanos;
}

qint64 TTimeLapseClockService::nowMicros() { return nowNanos() / 1000; }

QDateTime TTimeLapseClockService::currentDateTime() { return QDateTime::fromMSecsSinceEpoch(nowMillis()); }

qint64 TTimeLapseClockService::startNanos() const { return m_startNanos; }

void TTimeLapseClockService::pause()
{
    if (m_speed > 0.0) {
        m_pausedSpeed = m_speed;
        setSpeed(0.0);
    }
}

void TTimeLapseClockService::resume() { setSpeed(m_pausedSpeed); }

void TTimeLapseClockService::setSpeed(const double speed)
{
    if (m_speed == speed)
        return;
    const qint64 now = m_clockService->nowNanos();
    const qint64 last = m_lastBaseNanos;
    m_lastBaseNanos = now;
    if (m_speed != 0.0)
        m_elapsedNanos += qint64(m_speed * double(now - last));
    m_speed = speed;
}

void TTimeLapseClockService::setStartTime(const QDateTime& startTime) { m_startNanos = startTime.toMSecsSinceEpoch() * 1000000; }

void TTimeLapseClockService::setElapsedNanos(const qint64& elapsedNanos) { m_elapsedNanos = elapsedNanos; }

double TTimeLapseClockService::speed() const { return m_speed; }

}
