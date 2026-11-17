#pragma once

#include <QString>

namespace tibia {
namespace game {

/// The translated name of a world's PvP type (0 open .. 4 retro hardcore),
/// "dummy_unknown" otherwise (CIP: a classless helper of the character
/// selection, world transfer and character trade).
inline QString worldTypeName(const int worldType)
{
    static constexpr const char* Names[] = { "world_pvp_open", "world_pvp_optional", "world_pvp_hardcore", "world_pvp_retro_open",
                                             "world_pvp_retro_hardcore" };
    return qtTrId(static_cast<unsigned>(worldType) < 5 ? Names[worldType] : "dummy_unknown");
}

}
}
