#pragma once

/// Tip schedule façade (Phase 9B.3). Call sites prefer this over the retired
/// framework/core/eventdispatcher.h keep-alive.
#include <framework/core/scheduledevent.h>

#include <cstdint>
#include <functional>

extern int16_t g_eventThreadId;
extern int16_t g_mainThreadId;

namespace tibia {
namespace schedule {

/// Run on the Qt GUI thread after the current event returns.
EventPtr post(std::function<void()> callback);
/// Single-shot QTimer.
ScheduledEventPtr after(std::function<void()> callback, int delay);
/// Repeating QTimer until cancel().
ScheduledEventPtr every(std::function<void()> callback, int delay);

}
}
