#pragma once

#include <QtGlobal>

#include <algorithm>

namespace tibia::animation {

/// Linear progress from 0 to 1 over the time span [start, end] in milliseconds.
/// A span that is empty or reversed is finished from the start.
class TDurationTween final
{
public:
    TDurationTween() = default;
    TDurationTween(qint64 startMs, qint64 durationMs) { reset(startMs, durationMs); }

    void reset(qint64 startMs, qint64 durationMs)
    {
        m_start = startMs;
        m_end = startMs + durationMs;
        const qint64 span = m_end - m_start;
        m_finished = span == 0 || m_end < m_start;
        m_invRate = span == 0 ? 1.0 : 1.0 / static_cast<double>(span);
        m_progress = 0.0;
    }

    /// Advance the cached progress; true only on the call where the tween finishes.
    bool v2(qint64 nowMs)
    {
        const bool wasFinished = m_finished;
        m_finished = m_end <= nowMs;
        m_progress = progress(nowMs);
        return m_end <= nowMs && !wasFinished;
    }
    bool v3() const { return m_finished; }
    double v4() const { return m_progress; }
    qint64 v5() const { return m_start; }
    qint64 v6() const { return m_end; }

    qint64 start() const { return m_start; }
    qint64 end() const { return m_end; }
    qint64 duration() const { return m_end - m_start; }

    double progress(qint64 nowMs) const
    {
        const double p = std::min(static_cast<double>(nowMs - m_start) * m_invRate, 1.0);
        return p <= 0.0 ? 0.0 : p;
    }
    bool isFinished(qint64 nowMs) const { return m_end <= nowMs; }

    bool operator==(const TDurationTween& other) const
    {
        return m_start == other.m_start && m_end == other.m_end && m_invRate == other.m_invRate
            && m_progress == other.m_progress && m_finished == other.m_finished;
    }

private:
    qint64 m_start = 0;
    qint64 m_end = 0;
    double m_invRate = 0.0;
    double m_progress = 0.0;
    bool m_finished = false;
};

}
