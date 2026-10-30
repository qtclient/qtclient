#pragma once

// The world a frame reads: tiles, things, creatures, effects, missiles,
// containers and the local player. Protocol parsers write this state.

#include "attachedeffect.h"
#include "attachedeffectmanager.h"
#include "container.h"
#include "creature.h"
#include "effect.h"
#include "game.h"
#include "localplayer.h"
#include "map.h"
#include "missile.h"
#include "paperdoll.h"
#include "paperdollmanager.h"
#include "tile.h"
#include "thing.h"

#include <cstdint>
#include <functional>

namespace tibia::world {

inline Map& fields() { return g_map; }
inline Game& session() { return g_game; }
inline LocalPlayerPtr player() { return g_game.getLocalPlayer(); }
inline ContainerPtr container(int id) { return g_game.getContainer(id); }
inline int clientVersion() { return g_game.getClientVersion(); }

void setWorldTime(int hour, int minute);
int worldHour();
int worldMinute();
int subscribeWorldTime(std::function<void(int hour, int minute)> listener);
void unsubscribeWorldTime(int id);

int subscribeSessionEnd(std::function<void()> listener);
void unsubscribeSessionEnd(int id);
void notifySessionEnd();

/// Walk tweens at the time the next frame is shown, and whether a step may leave now.
class WalkCalculator
{
public:
    void advance(int64_t timestampMs);
    void resume();
    bool canStep(bool ignoreLock = false) const;
    /// True when the field in `direction` is known and not walkable.
    bool blocksStep(Otc::Direction direction) const;
    Point offset(const Creature& creature) const;
};

WalkCalculator& motion();

}
