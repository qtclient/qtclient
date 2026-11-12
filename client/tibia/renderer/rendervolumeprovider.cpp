#include "renderer/rendervolumeprovider.h"

#include "renderer/lightmodel.h"

#include <tibia/config/runtimegameconfig.h>
#include "worldmap/worldmapfield.h"
#include <algorithm>
#include <array>
#include "worldmap/worldmapstorage.h"

namespace tibia::renderer {

uint8_t TRenderVolumeProvider::firstVisibleFloor(const Position& camera)
{
    const int z = camera.z;
    const int sea = tibia::config::mapSeaFloor();
    int first = z <= sea ? 0 : std::clamp(z - tibia::config::mapAwareUndergroundFloorRange(), sea + 1,
                                          static_cast<int>(tibia::config::mapMaxZ()));

    static constexpr std::array<std::array<int, 2>, 5> Plus{ { { 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } } };
    for (const auto& [dx, dy] : Plus) {
        const Position field = camera.translated(dx, dy);
        if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(field);
            tile && qmlcomponents::light::fieldFlags(*tile).unsight)
            continue;
        for (int floor = z; floor > 0 && floor > first; --floor) {
            const int up = z - floor + 1;
            if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(Position(field.x + up, field.y + up, floor - 1))) {
                const qmlcomponents::light::FieldFlags flags = qmlcomponents::light::fieldFlags(*tile);
                if ((flags.bank || flags.bottom) && !flags.dontHide) {
                    first = floor;
                    break;
                }
            }
            if (tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(Position(field.x, field.y, floor - 1))) {
                const qmlcomponents::light::FieldFlags flags = qmlcomponents::light::fieldFlags(*tile);
                if (flags.bank && !flags.dontHide) {
                    first = floor;
                    break;
                }
            }
        }
    }
    return static_cast<uint8_t>(std::clamp<int>(first, 0, tibia::config::mapMaxZ()));
}

uint8_t TRenderVolumeProvider::lastVisibleFloor(const Position& camera)
{
    const int z = camera.z > tibia::config::mapSeaFloor()
        ? camera.z + tibia::config::mapAwareUndergroundFloorRange()
        : tibia::config::mapSeaFloor();
    return static_cast<uint8_t>(std::clamp<int>(z, 0, tibia::config::mapMaxZ()));
}

}
