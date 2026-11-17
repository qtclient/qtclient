#include "renderer/rendervolumeprovider.h"


#include <tibia/config/runtimegameconfig.h>
#include "worldmap/worldmapfield.h"
#include <algorithm>
#include "worldmap/worldmapstorage.h"

namespace tibia::renderer {

uint8_t TRenderVolumeProvider::firstVisibleFloor(const Position& camera)
{
    const int cx = camera.x;
    const int cy = camera.y;
    const int cz = camera.z;
    const int sea = tibia::config::mapSeaFloor();
    int first = cz <= sea ? 0 : std::clamp(cz - tibia::config::mapAwareUndergroundFloorRange(), sea + 1,
                                           static_cast<int>(tibia::config::mapMaxZ()));
    const auto fieldAt = [](const int x, const int y, const int z) {
        return tibia::worldmap::fieldAt(Position(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z)));
    };
    // The camera's field and its four neighbours; a floor above covers one of
    // them with a bank or bottom object over it, or a bank right above it.
    for (int x = std::max(1, cx) - 1; x <= cx + 1; ++x) {
        for (int y = std::max(1, cy) - 1; y <= cy + 1; ++y) {
            if (x != cx && y != cy)
                continue;
            // A field that blocks the view itself does not uncover the floors.
            if (const tibia::worldmap::TWorldMapField* own = fieldAt(x, y, cz); own && own->hasUnsight())
                continue;
            for (int z = std::max(cz, 0); z >= 1 && z > first; --z) {
                const int above = z - 1;
                const int shift = cz - above;
                const tibia::worldmap::TWorldMapField* over = fieldAt(x + shift, y + shift, above);
                if (over && (over->hasBank() || over->hasBottom()) && !over->hasDontHide()) {
                    first = z;
                    break;
                }
                const tibia::worldmap::TWorldMapField* roof = fieldAt(x, y, above);
                if (roof && roof->hasBank() && !roof->hasDontHide()) {
                    first = z;
                    break;
                }
            }
        }
    }
    return static_cast<uint8_t>(first);
}

uint8_t TRenderVolumeProvider::lastVisibleFloor(const Position& camera)
{
    const int z = camera.z > tibia::config::mapSeaFloor()
        ? camera.z + tibia::config::mapAwareUndergroundFloorRange()
        : tibia::config::mapSeaFloor();
    return static_cast<uint8_t>(std::clamp<int>(z, 0, tibia::config::mapMaxZ()));
}

}
