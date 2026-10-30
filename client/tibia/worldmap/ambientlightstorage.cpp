#include "ambientlightstorage.h"

#include "tibia/world/world.h"

namespace tibia::worldmap {

namespace {

constexpr float DayLengthMs = 3600000.0f;
constexpr int InterpolationIntervalMs = 10000;

}

TAmbientLightStorage::TAmbientLightStorage(QObject* parent)
    : cip::tibia::worldmap::TAmbientLightStorage(parent)
{
    m_clock.start();
    m_interpolationTimer.setInterval(InterpolationIntervalMs);
    connect(&m_interpolationTimer, &QTimer::timeout, this, &TAmbientLightStorage::interpolatedTibiaTimeOfDayChanged);
    m_interpolationTimer.start();

    // CIP's world-time message (0xef): hour / 24 + minute / 1440.
    m_worldTimeHook = tibia::world::subscribeWorldTime([this](const int hour, const int minute) {
        setNormalizedTibiaTimeOfDay(static_cast<float>(hour) / 24.0f + static_cast<float>(minute) / 1440.0f);
    });
}

TAmbientLightStorage::~TAmbientLightStorage()
{
    tibia::world::unsubscribeWorldTime(m_worldTimeHook);
}

float TAmbientLightStorage::normalizedTibiaTimeOfDay() const
{
    return static_cast<float>(m_clock.elapsed() - m_startMs) / DayLengthMs;
}

void TAmbientLightStorage::setNormalizedTibiaTimeOfDay(const float normalized)
{
    m_startMs = m_clock.elapsed() - static_cast<qint64>(normalized * DayLengthMs);
    Q_EMIT normalizedTibiaTimeOfDayWasChanged();
}

}
