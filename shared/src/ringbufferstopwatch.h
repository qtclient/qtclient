#pragma once

#include "iclockservice.h"

#include <QtGlobal>

#include <cstddef>
#include <memory>
#include <vector>

namespace tibia {
namespace shared {

/// CIP `shared::TRingBufferStopWatch`: a stop watch that keeps the durations
/// (in the clock's nanoseconds) of its last `capacity` runs.
class TRingBufferStopWatch
{
public:
    TRingBufferStopWatch(std::size_t capacity, std::shared_ptr<IClockService> clockService);
    virtual ~TRingBufferStopWatch();

    void start();
    /// Records the running duration; the oldest one drops out when full.
    void stop();
    bool isRunning() const;
    /// Forgets every duration and stops.
    void reset();
    /// The running duration, else the last recorded one (0 if none).
    qint64 elapsed() const;
    qint64 maximum() const;
    qint64 minimum() const;
    qint64 sum() const;
    qint64 average() const;
    std::size_t count() const;
    std::size_t capacity() const;

private:
    /// A fixed-capacity ring of durations, oldest first.
    struct TPrivate
    {
        std::vector<qint64> buffer;
        std::size_t first = 0;
        std::size_t size = 0;
        std::shared_ptr<IClockService> clockService;
        bool running = false;
        qint64 startNanos = 0;

        qint64 at(std::size_t index) const { return buffer[(first + index) % buffer.size()]; }
    };

    std::unique_ptr<TPrivate> d;
};

}
}
