#pragma once

#include <QtGlobal>

#include <cstdint>
#include <utility>
#include <vector>

namespace tibia::animation {

/// The minimum and maximum duration of one phase, in milliseconds.
struct TAnimationPhaseDuration
{
    qint64 first = 0;
    qint64 second = 0;

    TAnimationPhaseDuration() = default;
    TAnimationPhaseDuration(const qint64 minimum_, const qint64 maximum_)
        : first(minimum_)
        , second(maximum_)
    {
    }
    /// A phase that always lasts `duration`.
    explicit TAnimationPhaseDuration(const qint64 duration)
        : first(duration)
        , second(duration)
    {
    }

    qint64 minimum() const { return first; }
    qint64 maximum() const { return second; }
    /// The phase has no random range.
    bool isFixed() const { return first == second; }
    TAnimationPhaseDuration& operator+=(const TAnimationPhaseDuration& other)
    {
        first += other.first;
        second += other.second;
        return *this;
    }
    bool operator==(const TAnimationPhaseDuration& other) const { return first == other.first && second == other.second; }
};

/// The animation of a sprite sequence, as the appearance data describes it.
struct TAnimationInfo
{
    enum class ELoopType : quint8 { Infinite = 0, PingPong = 1, Counted = 2 };

    /// The minimum and maximum duration of each phase, in milliseconds.
    std::vector<TAnimationPhaseDuration> phases;
    ELoopType loopType = ELoopType::Infinite;
    /// Every instance follows one clock: an animator starts at time 0.
    bool synchronized = false;
    /// Every phase lasts exactly its minimum (no random duration).
    bool fixedDurations = true;
    /// Cycles a counted animation plays.
    quint32 loopCount = 0;
    /// The shortest and the longest cycle.
    qint64 minimumCycle = 0;
    qint64 maximumCycle = 0;

    /// No phases, infinite loop, fixed durations until `finish` says otherwise.
    TAnimationInfo() = default;

    quint32 phaseCount() const { return static_cast<quint32>(phases.size()); }
    qint64 minimumPhaseDuration(const quint32 phase) const { return phases[phase].first; }
    qint64 maximumPhaseDuration(const quint32 phase) const { return phases[phase].second; }
    qint64 minimumCycleDuration() const { return minimumCycle; }
    /// Play the animation a counted number of times.
    TAnimationInfo& setCounted()
    {
        loopType = ELoopType::Counted;
        return *this;
    }
    TAnimationInfo& setLoopCount(const quint32 count)
    {
        loopCount = count;
        return *this;
    }
    /// Same phases, loop type, loop count and clock (the derived fields are not compared).
    bool operator==(const TAnimationInfo& other) const
    {
        return phases == other.phases && loopType == other.loopType && loopCount == other.loopCount
               && synchronized == other.synchronized;
    }

    /// Fill `fixedDurations` and the cycle lengths from `phases` and `loopType`.
    void finish()
    {
        fixedDurations = true;
        for (const auto& [minimum, maximum] : phases) {
            if (minimum != maximum) {
                fixedDurations = false;
                break;
            }
        }
        minimumCycle = 0;
        maximumCycle = 0;
        for (const auto& [minimum, maximum] : phases) {
            minimumCycle += minimum;
            maximumCycle += maximum;
        }
        // A ping-pong cycle passes the inner phases twice.
        if (loopType == ELoopType::PingPong && phases.size() > 2) {
            for (size_t i = 1; i + 1 < phases.size(); ++i) {
                minimumCycle += phases[i].first;
                maximumCycle += phases[i].second;
            }
        }
    }
};

}
