#pragma once

#include "shared/cip_typed.h"

#include "animation/spriteanimator.h"
#include <client/position.h>

#include <QtGlobal>

#include <cstdint>

namespace tibia {
namespace animation {

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

/// CipSoft animation::TMovingOutfitAnimator — walking outfit phase from moving Animator.
///
/// Evidence (`tmp/re/tibia/walk-parity-decomp/`):
/// - ctor `` seeds cycle bounds 280..720 ms (`DAT_1022b9c78` / `DAT_1022b9c80`);
/// - `v4` `` is called with **phase count** (sprite sequence), not field ms;
/// - `v3` `` sets `phase = (elapsed / phaseDuration) % count` (or ping-pong);
/// - phase duration during a live walk clamps to `720 / phaseCount`.
/// OTC `stepDuration / footDelay` clamps are not CIP.
class TMovingOutfitAnimator final : public ::tibia::CipTyped
{
public:
    /// CipSoft constructor totals (ms) before dividing by phase count.
    static constexpr int kMinWalkCycleMs = 280;
    static constexpr int kMaxWalkCycleMs = 720;

    explicit TMovingOutfitAnimator(Animator* moving = nullptr, Animator* idle = nullptr);

    /// Idle uses the idle Animator clock. Walking uses absolute elapsed (CipSoft v3).
    /// `millisecondsPerField` kept for call-site compatibility; CipSoft feet are not
    /// paced by field duration (the movement tween owns that separately).
    int phase(bool moving, int millisecondsPerField = 0) const;
    /// Walking phase with an explicit elapsed (per-creature walk-anim clock).
    int phaseAt(ticks_t elapsedMs) const;
    Animator* movingAnimator() const { return m_moving; }
    Animator* idleAnimator() const { return m_idle; }

private:
    static int phaseDurationMs(int phaseCount);
    static int phaseIndexForElapsed(ticks_t elapsedMs, int phaseCount, bool pingPong);

    Animator* m_moving = nullptr;
    Animator* m_idle = nullptr;
};

}
}
