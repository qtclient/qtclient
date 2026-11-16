#include "animation/animator.h"

#include "clockservice.h"

#include <algorithm>

namespace tibia::animation {

TWorldMapLinearMovementTween::TWorldMapLinearMovementTween(const QPoint delta, const float durationMs)
    : m_delta(delta)
    , m_duration(durationMs)
{
}

QPoint TWorldMapLinearMovementTween::offsetAt(const float elapsedMs) const
{
    const float fraction = m_duration > 0.f ? elapsedMs / m_duration : 1.f;
    return m_delta * fraction;
}

}
