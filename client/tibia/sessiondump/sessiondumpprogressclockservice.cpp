#include "sessiondump/sessiondumpprogressclockservice.h"

namespace tibia::sessiondump {

TSessiondumpProgressClockService::TSessiondumpProgressClockService(std::shared_ptr<shared::IClockService> clockService)
    : TTimeLapseClockService(std::move(clockService))
{
}

TSessiondumpProgressClockService::~TSessiondumpProgressClockService() = default;

void TSessiondumpProgressClockService::start()
{
    m_startMillis = nowMillis();
    m_pausedProgress = Running;
}

qint64 TSessiondumpProgressClockService::progress()
{
    if (isProgressRunning())
        return nowMillis() - m_startMillis;
    return m_pausedProgress;
}

void TSessiondumpProgressClockService::pauseProgress()
{
    if (isProgressRunning())
        m_pausedProgress = nowMillis() - m_startMillis;
}

void TSessiondumpProgressClockService::resumeProgress()
{
    if (isProgressRunning())
        return;
    const qint64 paused = m_pausedProgress;
    m_pausedProgress = Running;
    setProgress(paused);
}

bool TSessiondumpProgressClockService::isProgressRunning() const { return m_pausedProgress == Running; }

void TSessiondumpProgressClockService::setProgress(const qint64& progress)
{
    const qint64 now = nowMillis();
    if (!isProgressRunning())
        m_pausedProgress = progress;
    else
        m_startMillis = now - progress;
}

}
