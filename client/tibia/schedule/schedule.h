#pragma once

#include <cstdint>
#include <functional>
#include <memory>

class QTimer;


namespace tibia {
namespace schedule {

/// A callback queued on the Qt GUI thread or on a QTimer; cancel() drops it.
class TScheduledTask final : public std::enable_shared_from_this<TScheduledTask>
{
public:
    explicit TScheduledTask(std::function<void()> callback);
    ~TScheduledTask();

    void cancel();
    bool isCanceled() const { return m_canceled; }
    bool isExecuted() const { return m_executed; }
    bool isPending() const { return !m_canceled && !m_executed; }

private:
    friend std::shared_ptr<TScheduledTask> post(std::function<void()> callback);
    friend std::shared_ptr<TScheduledTask> after(std::function<void()> callback, int delay);
    friend std::shared_ptr<TScheduledTask> every(std::function<void()> callback, int delay);

    void runOnce();
    void armQueued();
    void armTimer(int delayMs, bool repeat);
    void stopTimer();

    std::function<void()> m_callback;
    bool m_canceled = false;
    bool m_executed = false;
    QTimer* m_timer = nullptr;
};

using TScheduledTaskPtr = std::shared_ptr<TScheduledTask>;

/// Run on the Qt GUI thread after the current event returns.
TScheduledTaskPtr post(std::function<void()> callback);
/// Single-shot QTimer.
TScheduledTaskPtr after(std::function<void()> callback, int delay);
/// Repeating QTimer until cancel().
TScheduledTaskPtr every(std::function<void()> callback, int delay);

}
}
