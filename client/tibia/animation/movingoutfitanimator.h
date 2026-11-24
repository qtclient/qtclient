#pragma once

#include "animation/phaseanimator.h"

#include <QtGlobal>

namespace tibia::animation {

/// Phase of a walking outfit. The phase duration follows the duration of the
/// current step (its "key"), clamped so a full cycle takes between the minimum
/// and maximum total; when the key changes the clock is rebased so the phase
/// and the progress within it carry over. It is an animator of one fixed
/// phase of no duration whose phase it computes itself.
class TMovingOutfitAnimator : public TAnimator
{
public:
    static constexpr qint64 kMinimumCycleMs = 280;
    static constexpr qint64 kMaximumCycleMs = 720;

    TMovingOutfitAnimator(quint32 phaseCount, bool pingPong, qint64 minimumCycleMs = kMinimumCycleMs,
                          qint64 maximumCycleMs = kMaximumCycleMs);

    /// Recompute the phase at `nowMs`.
    void update(qint64 nowMs) override;
    void setDivisor(quint32 divisor);
    /// Start a step of `keyMs` at `nowMs` (no effect when the key is unchanged).
    void setKey(qint64 keyMs, qint64 nowMs);
    qint64 phaseDuration() const;
    /// Progress within the current phase at `timeMs`, from 0 to 1.
    double phaseFraction(qint64 timeMs) const;
    double lastPhaseFraction() const { return m_fraction; }
    qint64 minimumPhaseDuration() const { return m_minimumPhase; }
    qint64 maximumPhaseDuration() const { return m_maximumPhase; }

    quint32 phaseCount() const { return m_phaseCount; }
    bool isPingPong() const { return m_pingPong; }

    /// Phase `index` steps into a ping-pong cycle of `count` phases.
    static quint32 pingPongPhase(quint32 count, quint32 index);

private:
    /// The base animator's info: one fixed phase of no duration.
    static TAnimationInfo singlePhaseInfo();

    qint64 m_key = 0;
    qint64 m_keyStart = 0;
    qint64 m_accumulated = 0;
    quint32 m_divisor = 1;
    quint32 m_phaseCount = 1;
    bool m_pingPong = false;
    qint64 m_minimumCycle = kMinimumCycleMs;
    qint64 m_maximumCycle = kMaximumCycleMs;
    qint64 m_minimumPhase = 0;
    qint64 m_maximumPhase = 0;
    double m_fraction = 0.0;
};

}
