#include "ambientlightstorage.h"

#include "config/clientsettings.h"

#include <tibia/game/sessionhooks.h>

#include <algorithm>

namespace tibia::worldmap {

namespace {

constexpr float DayLengthMs = 3600000.0f;
constexpr int InterpolationIntervalMs = 10000;
constexpr qint64 DefaultChangeDurationMs = 10200;

TAmbientLightStorage* g_instance = nullptr;

/// One step of `light` toward `target`: brightness, red, green, blue each
/// move by one.
void stepToward(TAmbientLight& light, const TAmbientLight& target)
{
    if (target.intensity < light.intensity)
        --light.intensity;
    if (light.intensity < target.intensity)
        ++light.intensity;
    QColor& color = light.color;
    if (target.color.red() < color.red())
        color.setRed(color.red() - 1);
    if (color.red() < target.color.red())
        color.setRed(color.red() + 1);
    if (target.color.green() < color.green())
        color.setGreen(color.green() - 1);
    if (color.green() < target.color.green())
        color.setGreen(color.green() + 1);
    if (target.color.blue() < color.blue())
        color.setBlue(color.blue() - 1);
    if (color.blue() < target.color.blue())
        color.setBlue(color.blue() + 1);
}

}

TAmbientLightStorage::TAmbientLightStorage(QObject* parent)
    : cip::tibia::worldmap::TAmbientLightStorage(parent)
    , m_changeDurationMs(config::clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("millisecondsForAmbientLightChange"),
                                               DefaultChangeDurationMs))
{
    g_instance = this;
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
    if (g_instance == this)
        g_instance = nullptr;
}

TAmbientLightStorage* TAmbientLightStorage::instance()
{
    return g_instance;
}

void TAmbientLightStorage::update(const qint64 nowMs)
{
    const qint64 elapsed = std::max<qint64>(0, std::min(nowMs - m_lastUpdateMs, m_changeDurationMs));
    m_pendingMs += elapsed;
    if (m_target && m_changeDurationMs > 0) {
        const double stepMs = static_cast<double>(m_changeDurationMs) / 255.0;
        const int steps = static_cast<int>(static_cast<double>(m_pendingMs) / stepMs);
        for (int i = 0; i < steps; ++i)
            stepToward(m_light, *m_target);
        m_pendingMs -= static_cast<qint64>(stepMs * steps);
    }
    m_lastUpdateMs = nowMs;
}

void TAmbientLightStorage::setLight(const TAmbientLight& light)
{
    if (!m_target || m_changeDurationMs == 0) {
        m_target = std::make_unique<TAmbientLight>();
        m_light = light;
    }
    *m_target = light;
}

QColor TAmbientLightStorage::color() const
{
    const int intensity = m_light.intensity;
    return QColor::fromRgb(m_light.color.red() * intensity / 255, m_light.color.green() * intensity / 255,
                           m_light.color.blue() * intensity / 255, 255);
}

void TAmbientLightStorage::setNormalizedTibiaTimeOfDay(const float normalized)
{
    m_dayStartMs = m_lastUpdateMs - static_cast<qint64>(normalized * DayLengthMs);
    Q_EMIT normalizedTibiaTimeOfDayWasChanged();
}

float TAmbientLightStorage::normalizedTibiaTimeOfDay() const
{
    return static_cast<float>(m_lastUpdateMs - m_dayStartMs) / DayLengthMs;
}

}
