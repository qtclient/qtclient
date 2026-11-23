#include "animation/worldmaplinearmovementtween.h"

namespace tibia::animation {

TWorldMapLinearMovementTween::TWorldMapLinearMovementTween(const qint64 startMs, const qint64 endMs,
                                                           const worldmap::TWorldMapSubfieldCoordinate& from,
                                                           const worldmap::TWorldMapSubfieldCoordinate& to)
    : TDurationTween(startMs, endMs - startMs)
    , m_from(from)
    , m_position(from)
    , m_deltaX(to.subfieldX - from.subfieldX)
    , m_deltaY(to.subfieldY - from.subfieldY)
{
}

bool TWorldMapLinearMovementTween::advance(const qint64 nowMs)
{
    const bool finished = v2(nowMs);
    m_position = positionAt(v4());
    return finished;
}

worldmap::TWorldMapSubfieldCoordinate TWorldMapLinearMovementTween::positionAt(const double progress) const
{
    // Each step is rounded by adding a half and truncating toward zero, so a
    // negative step rounds up (-2.4 becomes -1).
    const int stepX = static_cast<int>(static_cast<double>(m_deltaX) * progress + 0.5);
    const int stepY = static_cast<int>(static_cast<double>(m_deltaY) * progress + 0.5);
    return { m_from.subfieldX + stepX, m_from.subfieldY + stepY, m_from.z };
}

}
