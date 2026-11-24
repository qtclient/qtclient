#pragma once

#include <client/outfit.h>
#include <client/position.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace tibia {
namespace game {

/// CIP game::TConfigureCreaturePodiumData: a ConfigureCreaturePodium message
/// as the podium dialog reads it (the player protocol handler stores it in the
/// outfit dialog storage, then asks for the dialog).
struct TConfigureCreaturePodiumData
{
    /// The podium's outfit (no outfit for an empty look).
    Outfit selectedOutfit;
    /// The race on the podium.
    uint16_t selectedRace = 0;
    bool bossPodium = false;
    /// Race id, name and (boss podium only) outfit of every choosable race.
    std::vector<std::tuple<uint16_t, std::string, Outfit>> races;
    Position position;
    uint16_t objectId = 0;
    uint8_t stackPosition = 0;
    bool podiumVisible = false;
    bool monsterVisible = false;
    uint8_t direction = 0;
};

}
}
