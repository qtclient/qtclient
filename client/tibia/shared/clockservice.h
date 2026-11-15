#pragma once

#include "shared/cip_typed.h"

#include <QtGlobal>

namespace tibia {
namespace shared {

/// CIP `shared::TClockService` — the game clock. `nowMicros` is the epoch time
/// the clock started at plus the monotonic time since; the frame clock (`frameMicros`/`frameMillis`) holds one value per
/// rendered frame so everything animated in a frame sees the same time.
class TClockService final : public ::tibia::CipTyped
{
public:
    TClockService() = default;
    /// Epoch microseconds: the start's epoch time plus the time since.
    static qint64 nowMicros();
    /// Epoch milliseconds, as nowMicros.
    static qint64 nowMillis();
    /// The epoch milliseconds the clock started at.
    static qint64 startMillis();

    /// The frame clock.
    static qint64 frameMicros();
    static qint64 frameMillis();
    /// Holds the frame clock at `micros` (nowMicros scale).
    static void setFrameMicros(qint64 micros);
    /// Moves the frame clock to real time.
    static void updateFrame();
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
