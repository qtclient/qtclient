#include "world.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace tibia::world {

namespace {

struct ListenerList
{
    template <class F>
    int add(F listener)
    {
        const int id = next++;
        entries.emplace_back(id, std::move(listener));
        return id;
    }

    template <class... Args>
    void publish(Args&&... args) const
    {
        const auto copy = entries;
        for (const auto& [id, listener] : copy) {
            if (listener)
                listener(std::forward<Args>(args)...);
        }
    }

    void remove(const int id)
    {
        std::erase_if(entries, [id](const auto& entry) { return entry.first == id; });
    }

    int next = 1;
    std::vector<std::pair<int, std::function<void(int, int)>>> entries;
};

ListenerList& worldTimeListeners()
{
    static ListenerList listeners;
    return listeners;
}

struct EndList
{
    int add(std::function<void()> listener)
    {
        const int id = next++;
        entries.emplace_back(id, std::move(listener));
        return id;
    }
    void publish() const
    {
        const auto copy = entries;
        for (const auto& [id, listener] : copy) {
            if (listener)
                listener();
        }
    }
    void remove(const int id)
    {
        std::erase_if(entries, [id](const auto& entry) { return entry.first == id; });
    }
    int next = 1;
    std::vector<std::pair<int, std::function<void()>>> entries;
};

EndList& sessionEndListeners()
{
    static EndList listeners;
    return listeners;
}

int g_hour = 0;
int g_minute = 0;

}

void setWorldTime(const int hour, const int minute)
{
    g_hour = hour;
    g_minute = minute;
    worldTimeListeners().publish(hour, minute);
}

int worldHour() { return g_hour; }
int worldMinute() { return g_minute; }

int subscribeWorldTime(std::function<void(int, int)> listener)
{
    const int id = worldTimeListeners().add(std::move(listener));
    return id;
}

void unsubscribeWorldTime(const int id) { worldTimeListeners().remove(id); }

int subscribeSessionEnd(std::function<void()> listener) { return sessionEndListeners().add(std::move(listener)); }
void unsubscribeSessionEnd(const int id) { sessionEndListeners().remove(id); }
void notifySessionEnd() { sessionEndListeners().publish(); }

}
