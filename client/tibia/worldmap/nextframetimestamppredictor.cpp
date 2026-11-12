#include "nextframetimestamppredictor.h"

#include "client/gamecore.h"

#include <numeric>

namespace tibia::worldmap {

TFrameDurationRing::TFrameDurationRing(const size_t capacity)
    : m_durations(capacity, 0)
{
    m_clock.start();
}

void TFrameDurationRing::start()
{
    const QMutexLocker lock(&m_mutex);
    m_start = m_clock.nsecsElapsed();
    m_started = true;
}

void TFrameDurationRing::finish()
{
    const QMutexLocker lock(&m_mutex);
    if (!m_started || m_durations.empty())
        return;
    m_durations[m_next] = m_clock.nsecsElapsed() - m_start;
    m_next = (m_next + 1) % m_durations.size();
    m_count = std::min(m_count + 1, m_durations.size());
    m_started = false;
}

qint64 TFrameDurationRing::averageNs() const
{
    const QMutexLocker lock(&m_mutex);
    if (m_count == 0)
        return 0;
    return std::accumulate(m_durations.begin(), m_durations.begin() + static_cast<std::ptrdiff_t>(m_count), qint64{ 0 })
        / static_cast<qint64>(m_count);
}

qint64 TNextFrameTimestampPredictor::predict() const
{
    return (client::gamecore::nowMicros() + m_offsetMicros) / 1000;
}

}
