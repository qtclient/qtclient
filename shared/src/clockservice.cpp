#include "clockservice.h"

#include <QDateTime>

#include <atomic>

namespace tibia::shared {

TClockService::TClockService(const qint64& startMillis)
    : m_startMillis(startMillis)
{
    m_elapsedTimer.start();
}

qint64 TClockService::nowMillis()
{
    return nowNanos() / 1000000;
}

qint64 TClockService::nowNanos()
{
    return m_elapsedTimer.nsecsElapsed() + m_startMillis * 1000000;
}

qint64 TClockService::nowMicros()
{
    return nowNanos() / 1000;
}

QDateTime TClockService::currentDateTime()
{
    return QDateTime::currentDateTime();
}

quint32 TClockService::currentUnixTimestamp()
{
    return quint32(QDateTime::currentSecsSinceEpoch());
}

qint64 TClockService::microsToMillis(const qint64 micros)
{
    return micros / 1000;
}

namespace {

std::atomic<qint64>& frameClock()
{
    static std::atomic<qint64> micros{ TClockService::startMillis() * 1000 };
    return micros;
}

}

const std::shared_ptr<TClockService>& TClockService::process()
{
    static const std::shared_ptr<TClockService> clock = std::make_shared<TClockService>(QDateTime::currentMSecsSinceEpoch());
    return clock;
}

qint64 TClockService::startMillis()
{
    return process()->m_startMillis;
}

qint64 TClockService::frameMicros()
{
    return frameClock().load(std::memory_order_relaxed);
}

qint64 TClockService::frameMillis()
{
    return frameMicros() / 1000;
}

void TClockService::setFrameMicros(const qint64 micros)
{
    frameClock().store(micros, std::memory_order_relaxed);
}

void TClockService::updateFrame()
{
    setFrameMicros(process()->nowMicros());
}

}
