#pragma once

#include "shared/cip_typed.h"

#include <QElapsedTimer>
#include <QMutex>

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace shared {
class IClockService;
}
namespace worldmap {

/// How long the last frames took from the GUI tick (afterAnimating) to the swap
/// (CIP: , 10 frames). start runs on the GUI thread, finish on
/// the render thread.
class TFrameDurationRing : public ::tibia::CipTyped
{
public:
    explicit TFrameDurationRing(size_t capacity = 10);

    /// CIP: .
    void start();
    /// CIP: ; only after a start.
    void finish();
    /// Average in nanoseconds; 0 before the first frame (CIP:).
    qint64 averageNs() const;

private:
    mutable QMutex m_mutex;
    QElapsedTimer m_clock;
    std::vector<qint64> m_durations;
    size_t m_next = 0;
    size_t m_count = 0;
    qint64 m_start = 0;
    bool m_started = false;
};

/// The time the next frame will be shown: now on the current game session's
/// clock plus the recent tick-to-swap time (CIP:
/// worldmap::TNextFrameTimestampPredictor).
class TNextFrameTimestampPredictor : public ::tibia::CipTyped
{
public:
    /// `clockService` is read while no game session gives a clock of its own.
    explicit TNextFrameTimestampPredictor(std::shared_ptr<shared::IClockService> clockService);

    /// The next frame's time in milliseconds: the clock's microseconds plus
    /// the offset.
    qint64 predict() const;
    /// How far after now the next frame is shown, in microseconds.
    void setOffset(qint64 micros) { m_offsetMicros = micros; }
    /// CIP connector: the clock follows `GameSession`'s; without a session the
    /// default clock is read again.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    std::shared_ptr<shared::IClockService> m_defaultClockService;
    std::shared_ptr<shared::IClockService> m_clockService;
    qint64 m_offsetMicros = 0;
    std::shared_ptr<game::IGameSession> m_gameSession;
};

}
}
