#include "systemsurvey/fpscounter.h"

namespace tibia::systemsurvey {

TFPSCounter::TFPSCounter(std::shared_ptr<shared::IClockService> clockService, QObject* parent)
    : QObject(parent)
    , m_clockService(std::move(clockService))
{
}

TFPSCounter::~TFPSCounter() = default;

bool TFPSCounter::operator==(const TFPSCounter& other) const
{
    return m_clockService == other.m_clockService && m_frameCount == other.m_frameCount
        && m_firstFrameTime == other.m_firstFrameTime && m_lastFrameTime == other.m_lastFrameTime
        && m_maximumFrameDuration == other.m_maximumFrameDuration
        && m_minimumFrameDuration == other.m_minimumFrameDuration;
}

void TFPSCounter::startNewFrame()
{
    const qint64 now = m_clockService->nowNanos();
    if (frameCount() == 0) {
        m_firstFrameTime = now;
    } else {
        const qint64 frameDuration = now - m_lastFrameTime;
        if (m_maximumFrameDuration < frameDuration)
            m_maximumFrameDuration = frameDuration;
        if (frameDuration < m_minimumFrameDuration)
            m_minimumFrameDuration = frameDuration;
    }
    m_lastFrameTime = now;
    ++m_frameCount;
}

void TFPSCounter::reset()
{
    m_frameCount = 0;
    m_firstFrameTime = 0;
    m_lastFrameTime = 0;
    m_maximumFrameDuration = 0;
    m_minimumFrameDuration = std::numeric_limits<qint64>::max();
}

double TFPSCounter::maximumFPS() const { return framesPerSecond(m_minimumFrameDuration); }

double TFPSCounter::minimumFPS() const { return framesPerSecond(m_maximumFrameDuration); }

double TFPSCounter::averageFPS() const
{
    quint64 meanFrameDuration = 0;
    if (frameCount() >= 2)
        meanFrameDuration = quint64(measuredDuration()) / quint64(frameCount() - 1);
    return framesPerSecond(qint64(meanFrameDuration));
}

qint64 TFPSCounter::frameCount() const { return m_frameCount; }

qint64 TFPSCounter::measuredDuration() const { return m_lastFrameTime - m_firstFrameTime; }

double TFPSCounter::framesPerSecond(const qint64 frameDuration) const
{
    if (frameDuration != 0)
        return 1000000000.0 / double(frameDuration);
    return 0.0;
}

}
