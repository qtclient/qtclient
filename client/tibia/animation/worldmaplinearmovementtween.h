#pragma once

#include "animation/durationtween.h"
#include "worldmap/coordinates.h"

#include <QtGlobal>

namespace tibia::animation {

/// Moves a sub-field position in a straight line from `from` to `to` while its
/// duration tween runs. The floor stays the floor of `from`.
class TWorldMapLinearMovementTween : public TDurationTween
{
public:
    TWorldMapLinearMovementTween() = default;
    TWorldMapLinearMovementTween(qint64 startMs, qint64 endMs, const worldmap::TWorldMapSubfieldCoordinate& from,
                                 const worldmap::TWorldMapSubfieldCoordinate& to);

    /// Advances the duration tween and moves the position to its progress; true
    /// only on the call where the tween finishes.
    bool advance(qint64 nowMs);
    /// The position at the last advance; `from` until the first one.
    const worldmap::TWorldMapSubfieldCoordinate& position() const { return m_position; }
    /// The position at `progress` (0 is `from`, 1 is `to`).
    worldmap::TWorldMapSubfieldCoordinate positionAt(double progress) const;

private:
    worldmap::TWorldMapSubfieldCoordinate m_from;
    worldmap::TWorldMapSubfieldCoordinate m_position;
    int m_deltaX = 0;
    int m_deltaY = 0;
};

}
