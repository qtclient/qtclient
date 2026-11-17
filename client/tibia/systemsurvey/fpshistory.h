#pragma once

#include "iclockservice.h"

#include <QDateTime>
#include <QObject>

#include <memory>
#include <vector>

namespace tibia {
namespace systemsurvey {

class TFPSCounter;

/// One sample of the FPS history: the frame rates a counter measured until `time`.
struct TFPSSample
{
    TFPSSample(double maximumFPS, double minimumFPS, double averageFPS, const QDateTime& time);

    bool operator==(const TFPSSample& other) const;
    bool operator!=(const TFPSSample& other) const;

    double maximumFPS = 0.0;
    double minimumFPS = 0.0;
    double averageFPS = 0.0;
    QDateTime time;
};

/// CIP `systemsurvey::TFPSHistory`: samples of an FPS counter. Each sample
/// takes the counter's rates and starts it over.
class TFPSHistory : public QObject
{
    Q_OBJECT
public:
    TFPSHistory(std::shared_ptr<TFPSCounter> fpsCounter, std::shared_ptr<shared::IClockService> clockService,
                QObject* parent = nullptr);
    ~TFPSHistory() override;

    bool operator==(const TFPSHistory& other) const;

    /// Adds the counter's current rates as a sample taken at `time`.
    virtual void addSample(const QDateTime& time);
    virtual void clear();
    virtual qint64 size() const;
    virtual const std::vector<TFPSSample>& samples() const;
    virtual std::shared_ptr<TFPSCounter> fpsCounter() const;

public Q_SLOTS:
    void sampleCurrentFPSInformationAndClearFPSCounter();

private:
    std::vector<TFPSSample> m_samples;
    std::shared_ptr<TFPSCounter> m_fpsCounter;
    std::shared_ptr<shared::IClockService> m_clockService;
};

}
}
