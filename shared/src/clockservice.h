#pragma once

#include "iclockservice.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QtGlobal>

#include <memory>

namespace tibia {
namespace shared {

/// CIP `shared::TClockService`: a monotonic clock that reads the epoch
/// milliseconds it was given at construction plus the time elapsed since.
///
/// Host: the client's game code reads one process-wide instance (`process()`)
/// that starts at the wall-clock time of its first use, and a frame clock
/// (`frameMicros`/`frameMillis`) that holds one value per rendered frame so
/// everything animated in a frame sees the same time.
class TClockService : public IClockService
{
public:
    explicit TClockService(const qint64& startMillis = 0);
    ~TClockService() override = default;

    /// nowNanos in milliseconds.
    qint64 nowMillis() override;
    /// The start's epoch time plus the monotonic time since construction.
    qint64 nowNanos() override;
    /// nowNanos in microseconds.
    qint64 nowMicros() override;
    /// The wall-clock date and time.
    QDateTime currentDateTime() override;

    /// The wall-clock time in whole seconds since the epoch.
    static quint32 currentUnixTimestamp();
    /// `micros` in milliseconds (one unit coarser).
    static qint64 microsToMillis(qint64 micros);

    /// Host: the process clock.
    static const std::shared_ptr<TClockService>& process();
    /// Host: the epoch milliseconds the process clock started at.
    static qint64 startMillis();

    /// Host: the frame clock.
    static qint64 frameMicros();
    static qint64 frameMillis();
    /// Holds the frame clock at `micros` (nowMicros scale).
    static void setFrameMicros(qint64 micros);
    /// Moves the frame clock to the frame source's now.
    static void updateFrame();
    /// Host: the clock the frame clock follows (a replay's clock while it
    /// plays; null: the process clock). Setting it moves the frame clock there.
    static void setFrameSource(std::shared_ptr<IClockService> source);
    static const std::shared_ptr<IClockService>& frameSource();
    /// The frame source's nowMicros (the process clock's without a source).
    static qint64 frameSourceMicros();

private:
    QElapsedTimer m_elapsedTimer;
    qint64 m_startMillis = 0;
};

/// Milliseconds elapsed on the frame clock since restart() (stop() freezes it).
class TFrameStopWatch
{
public:
    TFrameStopWatch() { restart(); }

    void restart(const int shift = 0)
    {
        m_startTicks = TClockService::frameMillis() - shift;
        m_frozenElapsed = shift;
        m_stopped = false;
    }
    void stop()
    {
        if (m_stopped)
            return;
        m_frozenElapsed = TClockService::frameMillis() - m_startTicks;
        m_stopped = true;
    }
    void update(const qint64 ticks) { m_startTicks += ticks; }

    qint64 startTicks() const { return m_startTicks; }
    qint64 ticksElapsed() const { return m_stopped ? m_frozenElapsed : TClockService::frameMillis() - m_startTicks; }
    float timeElapsed() const { return ticksElapsed() / 1000.0f; }
    bool running() const { return !m_stopped; }

private:
    qint64 m_startTicks = 0;
    qint64 m_frozenElapsed = 0;
    bool m_stopped = false;
};

}
}
