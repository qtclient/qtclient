#pragma once

#include "animation/animationinfo.h"

#include <QtGlobal>

#include <unordered_map>

namespace tibia::animation {

/// The phase of an animated sprite sequence. Phases follow the appearance's
/// durations from a start time; a synchronized animation starts at time 0, so
/// every instance shows the same phase.
class TAnimator final
{
public:
    TAnimator(TAnimationInfo info, qint64 startMs);

    /// Advance to `nowMs`: whole cycles are skipped, then phase by phase.
    void update(qint64 nowMs);
    int phase() const { return m_phase; }
    int phaseCount() const { return static_cast<int>(m_info.phaseCount()); }
    /// A counted animation played all its cycles.
    bool isFinished() const { return m_finished; }
    /// Loop even when the appearance asks for a ping-pong or a counted animation.
    void setAlwaysLoop(bool alwaysLoop) { m_alwaysLoop = alwaysLoop; }
    qint64 nextPhaseMs() const { return m_nextPhaseMs; }
    const TAnimationInfo& info() const { return m_info; }

    /// The duration of `phase`: its minimum when fixed, else random within its range.
    static qint64 phaseDuration(const TAnimationInfo& info, int phase);

private:
    int m_phase = 0;
    bool m_alwaysLoop = false;
    qint64 m_nextPhaseMs = 0;
    TAnimationInfo m_info;
    quint32 m_loop = 0;
    int m_direction = 1;
    bool m_finished = false;
};

/// The standing animators of the appearance types one owner draws (a podium
/// figure, a UI preview), each started at time 0.
class TIdleAnimators
{
public:
    /// The phase of the animation `info`, kept under `key`, at `nowMs`; 0 without one.
    int phaseAt(const void* key, const TAnimationInfo* info, qint64 nowMs);
    void clear() { m_animators.clear(); }

private:
    std::unordered_map<const void*, TAnimator> m_animators;
};

}
