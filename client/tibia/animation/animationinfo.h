#pragma once

#include <QtGlobal>

#include <cstdint>
#include <utility>
#include <vector>

namespace tibia::animation {

/// The animation of a sprite sequence, as the appearance data describes it.
struct TAnimationInfo
{
    enum class ELoopType : quint8 { Infinite = 0, PingPong = 1, Counted = 2 };

    /// The minimum and maximum duration of each phase, in milliseconds.
    std::vector<std::pair<qint64, qint64>> phases;
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

    quint32 phaseCount() const { return static_cast<quint32>(phases.size()); }

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
