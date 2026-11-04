#pragma once

/// CIP-neutral pack helpers over tip creature storage — spectator / row peels only.
/// Implementation includes creature storage out-of-line; this header stays free of CreaturePtr.

#include "mapsession.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tibia::game {

/// Pack-facing outfit peel (HUD chrome). No Outfit in the header.
struct CreatureOutfitInfo {
    bool exists{ false };
    bool isObject{ false }; ///< looktype from aux id (item look) rather than creature looktype
    uint16_t looktype{ 0 };
    uint8_t head{ 0 };
    uint8_t body{ 0 };
    uint8_t legs{ 0 };
    uint8_t feet{ 0 };
    uint8_t addons{ 0 };
};

/// POD spectator / creature row (team HUD / context scan). No CreaturePtr / TCreature.
struct CreatureSnapshot {
    uint32_t id{ 0 };
    std::string name;
    MapPos pos;
    uint8_t healthPercent{ 0 };
    uint8_t shield{ 0 };
    uint8_t vocation{ 0 };
    uint8_t creatureType{ 0 }; ///< Proto creature type (summon / hidden / …)
    bool isPlayer{ false };
    bool isNpc{ false };
    bool canBeSeen{ false };
    bool isDead{ false };
    bool isLocalPlayer{ false };
    CreatureOutfitInfo outfit;
};

/// Matches `TCreatureStorage::spectatorsNear` (aware-range rectangle around `center`).
std::vector<CreatureSnapshot> spectatorsNear(MapPos center, bool multiFloor, int left, int right,
                                             int top, int bottom);

inline std::vector<CreatureSnapshot> spectatorsNear(const MapPos center, const bool multiFloor,
                                                    const MapAwareRange range)
{
    return spectatorsNear(center, multiFloor, range.left, range.right, range.top, range.bottom);
}

/// Single storage row by creature id; `id == 0` / empty name when missing.
CreatureSnapshot creatureAt(uint32_t id);

/// True when `creatureType` is a summon (own or other). Prefer over `protocolcodes.h` Proto enums.
bool creatureTypeIsSummon(uint8_t creatureType);

} // namespace tibia::game
