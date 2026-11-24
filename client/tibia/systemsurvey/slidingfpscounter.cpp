#include "slidingfpscounter.h"

namespace tibia {
namespace systemsurvey {

TSlidingFPSCounter::TSlidingFPSCounter(const qint64 WindowMilliseconds, std::shared_ptr<shared::IClockService> ClockService)
    : m_pClockService(std::move(ClockService))
    , m_windowMilliseconds(WindowMilliseconds)
{
}

void TSlidingFPSCounter::startNewFrame()
{
    const qint64 now = m_pClockService->nowMillis();
    bool measure = false;
    if (!m_frameStartTimes.empty()) {
        const qint64 previous = m_frameStartTimes.back();
        measure = true;
        if (now <= previous + 1000)
            measure = now % 1000 < previous % 1000;
    }
    m_frameStartTimes.push_back(now);
    while (m_frameStartTimes.size() > 2 && m_frameStartTimes.front() + m_windowMilliseconds < m_frameStartTimes.back())
        m_frameStartTimes.pop_front();
    if (!measure)
        return;
    const std::size_t count = m_frameStartTimes.size();
    double framesPerSecond = 0.0;
    if (count > 1)
        framesPerSecond = static_cast<double>(count - 1) * 1000.0
            / static_cast<double>(m_frameStartTimes.back() - m_frameStartTimes.front());
    m_framesPerSecond = framesPerSecond;
}

qint64 TSlidingFPSCounter::lastFrameDuration() const
{
    if (m_frameStartTimes.size() < 2)
        return 0;
    return m_frameStartTimes.back() - m_frameStartTimes[m_frameStartTimes.size() - 2];
}

}
}
