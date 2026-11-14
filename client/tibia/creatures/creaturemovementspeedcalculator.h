#pragma once

#include "creatures/movementdirection.h"
#include "worldmap/coordinates.h"

#include <QtGlobal>

namespace tibia::creatures {

/// Step durations from creature speed, ground speed and the server beat.
/// One instance lives on the world map storage; the game session sets the
/// speed formula and the beat at login.
class TCreatureMovementSpeedCalculator
{
public:
    /// Speed the step duration is computed from:
    /// `A * log(speed + B) + C`, rounded to the nearest integer.
    quint16 effectiveSpeed(quint16 speed) const;

    void setSpeedFormula(double a, double b, double c);
    void setServerBeat(quint16 beatMs) { m_serverBeat = beatMs; }
    double speedFormulaA() const { return m_a; }
    double speedFormulaB() const { return m_b; }
    double speedFormulaC() const { return m_c; }
    quint16 serverBeat() const { return m_serverBeat; }

    /// Duration of one step onto ground of `groundSpeed` (diagonals cost the same).
    qint64 stepDuration(quint16 speed, const worldmap::TWorldMapExtent& step, int groundSpeed) const;
    /// Same, with a diagonal step costing three times the ground speed.
    qint64 stepDurationWithDiagonalCost(quint16 speed, const worldmap::TWorldMapExtent& step,
                                        int groundSpeed) const;
    qint64 stepDuration(quint16 speed, const worldmap::TWorldMapExtent& step, int groundSpeed,
                        bool diagonalCost) const;

    /// `floor(groundSpeed * 1000 / effectiveSpeed)` rounded up to a multiple of the beat.
    static qint64 beatRoundedDuration(int effectiveSpeed, quint32 groundSpeed, int beatMs);

private:
    double m_a = 0.0;
    double m_b = 0.0;
    double m_c = 0.0;
    quint16 m_serverBeat = 0;
};

}
