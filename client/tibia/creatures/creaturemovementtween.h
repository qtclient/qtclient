#pragma once

#include "animation/durationtween.h"
#include "creatures/movementdirection.h"
#include "worldmap/coordinates.h"

#include <QtGlobal>

namespace tibia::creatures {

/// Motion of a creature across one field, from `origin` in `direction`.
///
/// Drawing a creature asks the tween which field it currently covers relative
/// to the field its placeholder is on, and the subfield offset within that
/// field. In mode 1 the placeholder is already on the destination and the
/// creature slides in from behind it; in mode 0 (a predicted step) the
/// placeholder is still on the origin and the creature moves ahead of it.
/// A step that starts where the previous one ends before that one finished
/// keeps up to half a step of the old motion (`continueScale` above 1).
class TCreatureMovementTween
{
public:
    TCreatureMovementTween();

    void start(qint64 startMs, qint64 endMs, const worldmap::TWorldMapCoordinate& origin,
               EMovementDirection direction, int elevationFrom, int elevationTo, int mode);
    /// Freeze the motion where it is now.
    void stop();

    bool advance(qint64 nowMs) { return m_duration.v2(nowMs); }
    qint64 startTime() const { return m_duration.v5(); }
    qint64 endTime() const { return m_duration.v6(); }
    bool isTweenFinished() const { return m_duration.v3(); }
    double progress() const { return m_duration.v4(); }
    /// The step is over and the position has caught up with it.
    bool isFinished() const { return m_duration.v3() && m_mode == 1; }

    /// Field the creature covers, relative to the field its placeholder is on.
    worldmap::TWorldMapExtent coveredFieldOffset() const;
    /// Subfield offset within the covered field (before normalisation).
    worldmap::TWorldMapSubfieldExtent drawOffset() const;
    /// Elevation of the field the creature covers.
    int elevation() const { return isPastHalf() ? m_elevationTo : m_elevationFrom; }

    void setMode(int mode) { m_mode = mode; }
    int mode() const { return m_mode; }

    bool isPastHalf() const;
    const worldmap::TWorldMapCoordinate& origin() const { return m_origin; }
    worldmap::TWorldMapCoordinate destination() const;
    /// Offset towards the source field, in subfields.
    worldmap::TWorldMapSubfieldExtent remainingOffset() const;
    /// Offset from the covered source field to the drawn position, in subfields.
    worldmap::TWorldMapSubfieldExtent sourceFieldOffset() const;
    /// Part of the remaining motion beyond one field (only after a continued step).
    double overshoot() const;

    EMovementDirection direction() const { return m_direction; }
    double continueScale() const { return m_continueScale; }
    const animation::TDurationTween& durationTween() const { return m_duration; }

private:
    animation::TDurationTween m_duration;
    worldmap::TWorldMapCoordinate m_origin;
    int m_elevationFrom = 0;
    int m_elevationTo = 0;
    worldmap::TWorldMapExtent m_stepOffset{ 0, 0, 0 };
    worldmap::TWorldMapExtent m_reverseOffset{ 0, 0, 0 };
    worldmap::TWorldMapExtent m_previousReverseOffset{ 0, 0, 0 };
    double m_continueScale = 1.0;
    EMovementDirection m_direction = EMovementDirection::North;
    int m_mode = 1;
};

}
