#include "ringbufferstopwatch.h"

namespace tibia::shared {

TRingBufferStopWatch::TRingBufferStopWatch(const std::size_t capacity, std::shared_ptr<IClockService> clockService)
    : d(std::make_unique<TPrivate>())
{
    d->buffer.resize(capacity);
    d->clockService = std::move(clockService);
}

TRingBufferStopWatch::~TRingBufferStopWatch() = default;

void TRingBufferStopWatch::start()
{
    d->startNanos = d->clockService->nowNanos();
    d->running = true;
}

void TRingBufferStopWatch::stop()
{
    if (!d->running)
        return;
    const qint64 duration = d->clockService->nowNanos() - d->startNanos;
    if (d->size == d->buffer.size()) {
        if (!d->buffer.empty()) {
            d->buffer[d->first] = duration;
            d->first = (d->first + 1) % d->buffer.size();
        }
    } else {
        d->buffer[(d->first + d->size) % d->buffer.size()] = duration;
        ++d->size;
    }
    d->running = false;
}

bool TRingBufferStopWatch::isRunning() const
{
    return d->running;
}

void TRingBufferStopWatch::reset()
{
    if (d->size != 0)
        d->first = (d->first + d->size) % d->buffer.size();
    d->size = 0;
    d->running = false;
}

qint64 TRingBufferStopWatch::elapsed() const
{
    if (d->running)
        return d->clockService->nowNanos() - d->startNanos;
    if (d->size == 0)
        return 0;
    return d->at(d->size - 1);
}

qint64 TRingBufferStopWatch::maximum() const
{
    if (d->size == 0)
        return 0;
    qint64 result = d->at(0);
    for (std::size_t i = 1; i < d->size; ++i) {
        if (d->at(i) > result)
            result = d->at(i);
    }
    return result;
}

qint64 TRingBufferStopWatch::minimum() const
{
    if (d->size == 0)
        return 0;
    qint64 result = d->at(0);
    for (std::size_t i = 1; i < d->size; ++i) {
        if (d->at(i) < result)
            result = d->at(i);
    }
    return result;
}

qint64 TRingBufferStopWatch::sum() const
{
    qint64 result = 0;
    for (std::size_t i = 0; i < d->size; ++i)
        result += d->at(i);
    return result;
}

qint64 TRingBufferStopWatch::average() const
{
    if (d->size == 0)
        return 0;
    return qint64(quint64(sum()) / quint64(d->size));
}

std::size_t TRingBufferStopWatch::count() const
{
    return d->size;
}

std::size_t TRingBufferStopWatch::capacity() const
{
    return d->buffer.size();
}

}
