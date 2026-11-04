#pragma once

// The world a frame reads: tiles, things, creatures, effects, missiles,
// containers and the local player. Protocol parsers write this state.

#include "attachedeffect.h"
#include "attachedeffectmanager.h"
#include "creature.h"
#include "effect.h"
#include "game.h"
#include "localplayer.h"
#include "map.h"
#include "paperdoll.h"
#include "paperdollmanager.h"
#include "worldmap/worldmapfield.h"
#include "thing.h"

#include <cstdint>
#include <functional>
#include <tibia/game/gamesession.h>

namespace tibia::world {

inline Map& fields() { return g_map; }
inline Game& session() { return g_game; }
inline LocalPlayerPtr player() { return tibia::game::localPlayer(); }
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
    bool blocksStep(tibia::client::Direction direction) const;
    /// Walk offset for world draw / camera (storage row; 0 if unknown).
    Point offset(uint32_t creatureId) const;
};

WalkCalculator& motion();

}
