#include "renderer/rendervolumeprovider.h"

#include "renderer/lightmodel.h"

#include <client/gameconfig.h>
#include <tibia/world/tile.h>
#include <tibia/world/world.h>

#include <algorithm>
#include <array>

namespace tibia::renderer {

uint8_t TRenderVolumeProvider::firstVisibleFloor(const Position& camera)
{
    const int z = camera.z;
    const int sea = g_gameConfig.getMapSeaFloor();
    int first = z <= sea ? 0 : std::clamp(z - g_gameConfig.getMapAwareUndergroundFloorRange(), sea + 1,
                                          static_cast<int>(g_gameConfig.getMapMaxZ()));

    static constexpr std::array<std::array<int, 2>, 5> Plus{ { { 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } } };
    for (const auto& [dx, dy] : Plus) {
        const Position field = camera.translated(dx, dy);
        if (const TilePtr& tile = tibia::world::fields().getTile(field);
            tile && qmlcomponents::light::fieldFlags(*tile).unsight)
            continue;
        for (int floor = z; floor > 0 && floor > first; --floor) {
            const int up = z - floor + 1;
            if (const TilePtr& tile = tibia::world::fields().getTile(Position(field.x + up, field.y + up, floor - 1))) {
                const qmlcomponents::light::FieldFlags flags = qmlcomponents::light::fieldFlags(*tile);
                if ((flags.bank || flags.bottom) && !flags.dontHide) {
                    first = floor;
                    break;
                }
            }
            if (const TilePtr& tile = tibia::world::fields().getTile(Position(field.x, field.y, floor - 1))) {
                const qmlcomponents::light::FieldFlags flags = qmlcomponents::light::fieldFlags(*tile);
                if (flags.bank && !flags.dontHide) {
                    first = floor;
                    break;
                }
            }
        }
    }
    return static_cast<uint8_t>(std::clamp<int>(first, 0, g_gameConfig.getMapMaxZ()));
}

uint8_t TRenderVolumeProvider::lastVisibleFloor(const Position& camera)
{
    const int z = camera.z > g_gameConfig.getMapSeaFloor()
        ? camera.z + g_gameConfig.getMapAwareUndergroundFloorRange()
        : g_gameConfig.getMapSeaFloor();
    return static_cast<uint8_t>(std::clamp<int>(z, 0, g_gameConfig.getMapMaxZ()));
}

}
