#include <framework/core/eventdispatcher.h>

#include <framework/stdext/thread.h>

int16_t g_eventThreadId = -1;
int16_t g_mainThreadId = -1;

namespace tibia::schedule {

EventPtr post(std::function<void()> callback)
{
    auto event = std::make_shared<Event>(std::move(callback));
    event->armQueued();
    return event;
}

ScheduledEventPtr after(std::function<void()> callback, const int delay)
{
    auto event = std::make_shared<ScheduledEvent>("after", std::move(callback), delay, 1);
    event->armTimer(delay, false);
    return event;
}

ScheduledEventPtr every(std::function<void()> callback, const int delay)
{
    auto event = std::make_shared<ScheduledEvent>("every", std::move(callback), delay, 0);
    event->armTimer(delay, true);
    return event;
}

}
