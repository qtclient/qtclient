#include "creatures/creaturemovementtween.h"

#include <cmath>
#include <limits>

namespace tibia::creatures {

namespace {

int truncateToInt32(const double value)
{
    if (!(value > -2147483649.0 && value < 2147483648.0))
        return std::numeric_limits<int>::min();
    return static_cast<int>(value);
}

worldmap::TWorldMapSubfieldExtent subfieldExtent(const double x, const double y)
{
    return { truncateToInt32(x * 32.0), truncateToInt32(y * 32.0) };
}

worldmap::TWorldMapSubfieldExtent operator+(const worldmap::TWorldMapSubfieldExtent& a,
                                            const worldmap::TWorldMapSubfieldExtent& b)
{
    return { a.width + b.width, a.height + b.height };
}

}

TCreatureMovementTween::TCreatureMovementTween()
    : m_duration(0, 0)
{
}

void TCreatureMovementTween::start(const qint64 startMs, const qint64 endMs, const worldmap::TWorldMapCoordinate& origin,
                                   const EMovementDirection direction, const int elevationFrom, const int elevationTo,
                                   const int mode)
{
    m_continueScale = 1.0;
    if (!isFinished() && m_mode == 1 && destination() == origin) {
        m_previousReverseOffset = m_reverseOffset;
        advance(startMs);
        const double remaining = 1.0 - m_duration.v4();
        m_continueScale += remaining <= 0.5 ? remaining : 0.5;
    }
    m_origin = origin;
    m_elevationFrom = elevationFrom;
    m_elevationTo = elevationTo;
    m_direction = direction;
    m_stepOffset = stepOffset(direction);
    m_reverseOffset = reverseStepOffset(direction);
    m_duration = animation::TDurationTween(startMs, endMs - startMs);
    m_mode = mode;
}

void TCreatureMovementTween::stop()
{
    const qint64 end = m_duration.v6();
    const qint64 start = m_duration.v5();
    const double elapsed = std::floor(m_duration.v4() * static_cast<double>(end - start));
    const qint64 frozenEnd = start + static_cast<qint64>(elapsed);
    const qint64 frozenStart = frozenEnd - 1;
    m_duration = animation::TDurationTween(frozenStart, frozenEnd - frozenStart);
    m_duration.v2(m_mode == 0 ? frozenStart : frozenEnd);
}

bool TCreatureMovementTween::isPastHalf() const
{
    return 0.5 <= m_duration.v4() / m_continueScale;
}

worldmap::TWorldMapExtent TCreatureMovementTween::coveredFieldOffset() const
{
    const bool pastHalf = isPastHalf();
    if (m_mode == 1)
        return pastHalf ? worldmap::TWorldMapExtent{ 0, 0, 0 } : m_reverseOffset;
    return pastHalf ? m_stepOffset : worldmap::TWorldMapExtent{ 0, 0, 0 };
}

worldmap::TWorldMapSubfieldExtent TCreatureMovementTween::drawOffset() const
{
    return isPastHalf() ? remainingOffset() : sourceFieldOffset();
}

worldmap::TWorldMapCoordinate TCreatureMovementTween::destination() const
{
    return { m_origin.x + m_stepOffset.width, m_origin.y + m_stepOffset.height, m_origin.z + m_stepOffset.depth };
}

double TCreatureMovementTween::overshoot() const
{
    const double remaining = (1.0 - m_duration.v4()) * m_continueScale;
    return 1.0 < remaining ? remaining - 1.0 : 0.0;
}

worldmap::TWorldMapSubfieldExtent TCreatureMovementTween::remainingOffset() const
{
    const double progress = m_duration.v4();
    worldmap::TWorldMapSubfieldExtent extra{ 0, 0 };
    double factor = overshoot();
    if (factor <= 0.0) {
        factor = (1.0 - progress) * m_continueScale;
    } else {
        extra = subfieldExtent(m_previousReverseOffset.width * factor, m_previousReverseOffset.height * factor);
        factor = 1.0;
    }
    return subfieldExtent(m_reverseOffset.width * factor, m_reverseOffset.height * factor) + extra;
}

worldmap::TWorldMapSubfieldExtent TCreatureMovementTween::sourceFieldOffset() const
{
    const worldmap::TWorldMapCoordinate next = destination();
    const worldmap::TWorldMapSubfieldExtent offset = remainingOffset();
    return { next.x * 32 + offset.width - m_origin.x * 32, next.y * 32 + offset.height - m_origin.y * 32 };
}

}
