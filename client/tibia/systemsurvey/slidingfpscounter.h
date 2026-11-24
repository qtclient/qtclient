#pragma once

#include "iclockservice.h"

#include <QtGlobal>

#include <deque>
#include <memory>

namespace tibia {
namespace systemsurvey {

/// CIP `systemsurvey::TSlidingFPSCounter`: the frame rate over a sliding
/// window of the most recent frame start times (milliseconds of the clock
/// service). The rate is measured again once per second of the clock.
class TSlidingFPSCounter
{
public:
    /// `WindowMilliseconds`: how far the first kept frame may lie before the last one.
    TSlidingFPSCounter(qint64 WindowMilliseconds, std::shared_ptr<shared::IClockService> ClockService);

    /// A frame starts now. Frames older than the window go (at least two stay);
    /// when the clock passed into a new second since the previous frame (or
    /// more than a second went by), the rate is measured again.
    void startNewFrame();
    /// The last measured rate in frames per second.
    double framesPerSecond() const { return m_framesPerSecond; }
    /// Milliseconds between the last two frames; 0 below two frames.
    qint64 lastFrameDuration() const;

private:
    std::shared_ptr<shared::IClockService> m_pClockService;
    qint64 m_windowMilliseconds = 0;
    double m_framesPerSecond = 0.0;
    std::deque<qint64> m_frameStartTimes;
};

}
}
