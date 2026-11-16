#include "shared/clockservice.h"

#include <QDateTime>
#include <QElapsedTimer>

#include <atomic>

namespace tibia::shared {

namespace {

/// The clock's start: a running timer and the epoch time it started at.
struct TStart
{
    QElapsedTimer timer;
    qint64 epochMillis = 0;
};

const TStart& startup()
{
    static const TStart start = [] {
        TStart s;
        s.epochMillis = QDateTime::currentMSecsSinceEpoch();
        s.timer.start();
        return s;
    }();
    return start;
}

std::atomic<qint64> g_frameMicros{ startup().epochMillis * 1000 };

}

qint64 TClockService::nowNanos()
{
    const TStart& start = startup();
    return start.timer.nsecsElapsed() + start.epochMillis * 1000000;
}

qint64 TClockService::nowMicros()
{
    return nowNanos() / 1000;
}

QDateTime TClockService::currentDateTime()
{
    return QDateTime::currentDateTime();
}

qint64 TClockService::startMillis()
{
    return startup().epochMillis;
}

qint64 TClockService::nowMillis()
{
    return nowMicros() / 1000;
}

qint64 TClockService::frameMicros()
{
    return g_frameMicros.load(std::memory_order_relaxed);
}

qint64 TClockService::frameMillis()
{
    return frameMicros() / 1000;
}

void TClockService::setFrameMicros(const qint64 micros)
{
    g_frameMicros.store(micros, std::memory_order_relaxed);
}

void TClockService::updateFrame()
{
    setFrameMicros(nowMicros());
}

}
