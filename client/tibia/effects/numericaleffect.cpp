#include "effects/numericaleffect.h"

#include "config/clientsettings.h"

namespace tibia::effects {

qint64 TNumericalEffect::duration()
{
    static const qint64 value = config::clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("numericalEffectDisplayDurationMsec"), 1000);
    return value;
}

qint64 TNumericalEffect::mergeDuration()
{
    static const qint64 value = config::clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("numericalEffectMergeDurationMsec"), 100);
    return value;
}

TNumericalEffect::TNumericalEffect(const Position& coordinate, const qint64 value, const QColor& color, const qint64 startMs)
    : m_coordinate(coordinate)
    , m_value(value)
    , m_color(color)
{
    m_tween.reset(startMs, duration());
}

bool TNumericalEffect::mergeWith(const TNumericalEffect& other)
{
    if (!canMerge(other))
        return false;
    m_value += other.value();
    return true;
}

bool TNumericalEffect::canMerge(const TNumericalEffect& other) const
{
    return m_color == other.color() && m_coordinate == other.coordinate()
        && other.startTime() < startTime() + mergeDuration();
}

QColor numericalEffectColorFrom8bit(const int color)
{
    if (color < 0 || color > 215)
        return QColor(0, 0, 0);
    return QColor(color / 36 * 51, color / 6 % 6 * 51, color % 6 * 51);
}

}
