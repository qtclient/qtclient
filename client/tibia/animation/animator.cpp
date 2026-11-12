#include "animation/animator.h"

#include "framework/core/clock.h"

#include <algorithm>

namespace tibia::animation {

TAnimator::TAnimator(Animator* animator)
    : m_animator(animator)
{
}

int TAnimator::phase() const
{
    return m_animator ? m_animator->getPhase() : 0;
}

int TAnimator::phaseAt(Timer& timer, const float durationFactor) const
{
    return m_animator ? m_animator->getPhaseAt(timer, durationFactor) : 0;
}

int TAnimator::phaseCount() const
{
    return m_animator ? m_animator->getAnimationPhases() : 0;
}

bool TAnimator::isComplete() const
{
    return m_animator ? m_animator->isComplete() : true;
}

TDurationTween::TDurationTween(const qint64 startMs, const qint64 durationMs)
    : m_start(startMs)
    , m_duration(durationMs)
{
}

void TDurationTween::reset(const qint64 startMs, const qint64 durationMs)
{
    m_start = startMs;
    m_duration = durationMs;
}

double TDurationTween::progress(const qint64 nowMs) const
{
    if (m_duration <= 0)
        return 1.0;
    return std::clamp(static_cast<double>(nowMs - m_start) / static_cast<double>(m_duration), 0.0, 1.0);
}

bool TDurationTween::isFinished(const qint64 nowMs) const
{
    return nowMs >= m_start + m_duration;
}

TWorldMapLinearMovementTween::TWorldMapLinearMovementTween(const Point delta, const float durationMs)
    : m_delta(delta)
    , m_duration(durationMs)
{
}

Point TWorldMapLinearMovementTween::offsetAt(const float elapsedMs) const
{
    const float fraction = m_duration > 0.f ? elapsedMs / m_duration : 1.f;
    return m_delta * fraction;
}

TMovingOutfitAnimator::TMovingOutfitAnimator(Animator* moving, Animator* idle)
    : m_moving(moving)
    , m_idle(idle)
{
}

int TMovingOutfitAnimator::phaseDurationMs(const int phaseCount)
{
    // CipSoft v6 clamps to maxPerPhase = 720 / count while the walk window is live.
    return std::max(1, kMaxWalkCycleMs / std::max(1, phaseCount));
}

int TMovingOutfitAnimator::phaseIndexForElapsed(const ticks_t elapsedMs, const int phaseCount,
                                                const bool pingPong)
{
    const int phases = std::max(1, phaseCount);
    const int duration = phaseDurationMs(phases);
    const int steps = static_cast<int>(std::max<ticks_t>(0, elapsedMs) / duration);
    if (!pingPong || phases < 2)
        return steps % phases;

    // CipSoft v3 ping-pong: period = count*2-2, then fold the upper half.
    const int period = phases * 2 - 2;
    const int u = steps % period;
    if (u <= phases - 1)
        return u;
    return 2 * (phases - 1) - u;
}

int TMovingOutfitAnimator::phaseAt(const ticks_t elapsedMs) const
{
    if (!m_moving)
        return 0;
    const int phases = std::max(1, m_moving->getAnimationPhases());
    const bool pingPong =
        m_moving->getLoopType() == AnimationLoopType::ANIMATION_LOOP_TYPE_PINGPONG;
    return phaseIndexForElapsed(elapsedMs, phases, pingPong);
}

int TMovingOutfitAnimator::phase(const bool moving, const int millisecondsPerField) const
{
    (void)millisecondsPerField;
    Animator* animator = moving && m_moving ? m_moving : m_idle;
    if (!animator)
        return 0;
    if (!(moving && m_moving))
        return animator->getPhase();
    // Preview / UI: shared wall clock, same CipSoft cadence as world creatures.
    return phaseAt(g_clock.millis());
}

}
