#pragma once

#include "shared/cip_typed.h"

#include "animation/phaseanimator.h"
#include "animation/durationtween.h"
#include <client/position.h>

#include <QtGlobal>

#include <cstdint>

namespace tibia {
namespace animation {

/// CIP animation::TWorldMapLinearMovementTween — missile / walk field slide.
class TWorldMapLinearMovementTween final : public ::tibia::CipTyped
{
public:
    TWorldMapLinearMovementTween() = default;
    TWorldMapLinearMovementTween(Point delta, float durationMs);

    Point offsetAt(float elapsedMs) const;
    float duration() const { return m_duration; }
    const Point& delta() const { return m_delta; }

private:
    Point m_delta;
    float m_duration = 0.f;
};


}
}
