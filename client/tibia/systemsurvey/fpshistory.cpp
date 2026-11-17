#include "systemsurvey/fpshistory.h"

#include "systemsurvey/fpscounter.h"

namespace tibia::systemsurvey {

TFPSSample::TFPSSample(const double maximumFPS, const double minimumFPS, const double averageFPS, const QDateTime& time)
    : maximumFPS(maximumFPS)
    , minimumFPS(minimumFPS)
    , averageFPS(averageFPS)
    , time(time)
{
}

bool TFPSSample::operator==(const TFPSSample& other) const
{
    return maximumFPS == other.maximumFPS && minimumFPS == other.minimumFPS && averageFPS == other.averageFPS
        && time == other.time;
}

bool TFPSSample::operator!=(const TFPSSample& other) const { return !(*this == other); }

TFPSHistory::TFPSHistory(std::shared_ptr<TFPSCounter> fpsCounter, std::shared_ptr<shared::IClockService> clockService,
                         QObject* parent)
    : QObject(parent)
    , m_fpsCounter(std::move(fpsCounter))
    , m_clockService(std::move(clockService))
{
}

TFPSHistory::~TFPSHistory() = default;

bool TFPSHistory::operator==(const TFPSHistory& other) const
{
    return m_samples == other.m_samples && m_fpsCounter == other.m_fpsCounter;
}

void TFPSHistory::addSample(const QDateTime& time)
{
    m_samples.emplace_back(m_fpsCounter->maximumFPS(), m_fpsCounter->minimumFPS(), m_fpsCounter->averageFPS(), time);
}

void TFPSHistory::clear() { m_samples.clear(); }

qint64 TFPSHistory::size() const { return qint64(m_samples.size()); }

const std::vector<TFPSSample>& TFPSHistory::samples() const { return m_samples; }

std::shared_ptr<TFPSCounter> TFPSHistory::fpsCounter() const { return m_fpsCounter; }

void TFPSHistory::sampleCurrentFPSInformationAndClearFPSCounter()
{
    addSample(m_clockService->currentDateTime());
    m_fpsCounter->reset();
}

}
