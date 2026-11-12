#pragma once

#include "shared/cip_typed.h"

#include <QElapsedTimer>
#include <QMutex>

#include <vector>

namespace tibia {
namespace worldmap {

/// How long the last frames took from the GUI tick (afterAnimating) to the swap
/// (CIP: , 10 frames). start runs on the GUI thread, finish on
/// the render thread.
class TFrameDurationRing : public ::tibia::CipTyped
{
public:
    explicit TFrameDurationRing(size_t capacity = 10);

    /// CIP: .
    void start();
    /// CIP: ; only after a start.
    void finish();
    /// Average in nanoseconds; 0 before the first frame (CIP:).
    qint64 averageNs() const;

private:
    mutable QMutex m_mutex;
    QElapsedTimer m_clock;
    std::vector<qint64> m_durations;
    size_t m_next = 0;
    size_t m_count = 0;
    qint64 m_start = 0;
    bool m_started = false;
};

/// The time the next frame will be shown: now plus the recent tick-to-swap
/// time (CIP: worldmap::TNextFrameTimestampPredictor).
class TNextFrameTimestampPredictor : public ::tibia::CipTyped
{
public:
    /// Milliseconds on the engine clock (CIP: v2).
    qint64 predict() const;
    /// CIP: v3.
    void setOffset(qint64 micros) { m_offsetMicros = micros; }

private:
    qint64 m_offsetMicros = 0;
};

}
}
