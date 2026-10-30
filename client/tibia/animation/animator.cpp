#include "animation/animator.h"

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

int TMovingOutfitAnimator::phase(const bool moving, const int millisecondsPerField) const
{
    Animator* animator = moving && m_moving ? m_moving : m_idle;
    if (!animator)
        return 0;
    if (moving && m_moving && millisecondsPerField > 0) {
        // Match AppearanceInstanceRenderer outfitPhase: scale by walk pace.
        Timer timer;
        timer.restart();
        return m_moving->getPhaseAt(timer, 1.f);
    }
    return animator->getPhase();
}

}
