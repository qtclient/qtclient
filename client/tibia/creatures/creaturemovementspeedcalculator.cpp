#include "creatures/creaturemovementspeedcalculator.h"

#include <cmath>
#include <limits>

namespace tibia::creatures {

namespace {

// Out-of-range and NaN conversions yield the integer minimum, as the x86
// truncating conversions do; the original relies on that before login, when
// the formula and the beat are still zero.
qint32 truncateToInt32(const double value)
{
    if (!(value > -2147483649.0 && value < 2147483648.0))
        return std::numeric_limits<qint32>::min();
    return static_cast<qint32>(value);
}

qint64 truncateToInt64(const double value)
{
    if (!(value >= -9223372036854775808.0 && value < 9223372036854775808.0))
        return std::numeric_limits<qint64>::min();
    return static_cast<qint64>(value);
}

}

quint16 TCreatureMovementSpeedCalculator::effectiveSpeed(const quint16 speed) const
{
    const double value = std::log(static_cast<double>(speed) + m_b) * m_a + m_c + 0.5;
    return static_cast<quint16>(truncateToInt32(value) & 0xffff);
}

void TCreatureMovementSpeedCalculator::setSpeedFormula(const double a, const double b, const double c)
{
    m_a = a;
    m_b = b;
    m_c = c;
}

qint64 TCreatureMovementSpeedCalculator::beatRoundedDuration(const int effectiveSpeed, const quint32 groundSpeed,
                                                            const int beatMs)
{
    const double floored = std::floor(static_cast<double>(groundSpeed) * 1000.0 / static_cast<double>(effectiveSpeed));
    const double beats = std::ceil(floored / static_cast<double>(beatMs));
    return truncateToInt64(beats * static_cast<double>(beatMs));
}

qint64 TCreatureMovementSpeedCalculator::stepDuration(const quint16 speed, const worldmap::TWorldMapExtent& step,
                                                     const int groundSpeed, const bool diagonalCost) const
{
    const EMovementDirection direction = directionOfStep(step);
    int ground = groundSpeed;
    if (diagonalCost && isDiagonal(direction))
        ground *= 3;
    return beatRoundedDuration(effectiveSpeed(speed), static_cast<quint32>(ground), static_cast<qint16>(m_serverBeat));
}

qint64 TCreatureMovementSpeedCalculator::stepDuration(const quint16 speed, const worldmap::TWorldMapExtent& step,
                                                     const int groundSpeed) const
{
    directionOfStep(step); // only one-field steps have a duration
    return beatRoundedDuration(effectiveSpeed(speed), static_cast<quint32>(groundSpeed),
                               static_cast<qint16>(m_serverBeat));
}

qint64 TCreatureMovementSpeedCalculator::stepDurationWithDiagonalCost(const quint16 speed,
                                                                     const worldmap::TWorldMapExtent& step,
                                                                     const int groundSpeed) const
{
    return stepDuration(speed, step, groundSpeed, true);
}

}
