#pragma once

#include "scheduledevent.h"

#include <cstdint>
#include <functional>

extern int16_t g_eventThreadId;
extern int16_t g_mainThreadId;

namespace tibia::schedule {

/// Run on the Qt GUI thread after the current event returns.
EventPtr post(std::function<void()> callback);
/// Single-shot QTimer.
ScheduledEventPtr after(std::function<void()> callback, int delay);
/// Repeating QTimer until cancel().
ScheduledEventPtr every(std::function<void()> callback, int delay);

}
