#pragma once

#include "shared/cip_typed.h"

#include <client/animator.h>
#include <client/position.h>

#include <QtGlobal>

#include <cstdint>

namespace tibia::animation {

/// CIP animation::TAnimator — phase clock over a ThingType Animator.
class TAnimator final : public ::tibia::CipTyped
{
public:
    explicit TAnimator(Animator* animator = nullptr);

    Animator* animator() const { return m_animator; }
    void setAnimator(Animator* animator) { m_animator = animator; }

    int phase() const;
    int phaseAt(Timer& timer, float durationFactor = 1.f) const;
    int phaseCount() const;
    bool isComplete() const;

private:
    Animator* m_animator = nullptr;
};

/// CIP animation::TDurationTween — linear 0..1 over a fixed duration (ms).
class TDurationTween final : public ::tibia::CipTyped
{
public:
    TDurationTween() = default;
    TDurationTween(qint64 startMs, qint64 durationMs);

    void reset(qint64 startMs, qint64 durationMs);
    qint64 start() const { return m_start; }
    qint64 duration() const { return m_duration; }
    double progress(qint64 nowMs) const;
    bool isFinished(qint64 nowMs) const;

private:
    qint64 m_start = 0;
    qint64 m_duration = 0;
};

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

/// CIP animation::TMovingOutfitAnimator — walking outfit phase from moving Animator.
class TMovingOutfitAnimator final : public ::tibia::CipTyped
{
public:
    explicit TMovingOutfitAnimator(Animator* moving = nullptr, Animator* idle = nullptr);

    int phase(bool moving, int millisecondsPerField) const;
    Animator* movingAnimator() const { return m_moving; }
    Animator* idleAnimator() const { return m_idle; }

private:
    Animator* m_moving = nullptr;
    Animator* m_idle = nullptr;
};

}
