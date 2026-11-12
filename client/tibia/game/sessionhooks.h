#pragma once

// World-time + session-end listener hub (CIP ambient clock / game-end hooks).
// Lives under game/ (namespace tibia::world); not the retired world/ tip tree.

#include <cstdint>
#include <functional>

namespace tibia {
namespace world {

void setWorldTime(int hour, int minute);
int worldHour();
int worldMinute();
int subscribeWorldTime(std::function<void(int hour, int minute)> listener);
void unsubscribeWorldTime(int id);

int subscribeSessionEnd(std::function<void()> listener);
void unsubscribeSessionEnd(int id);
void notifySessionEnd();

}
}
