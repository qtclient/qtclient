#include "animation/animator.h"

#include "framework/core/clock.h"

#include <algorithm>

namespace tibia::animation {

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

}
