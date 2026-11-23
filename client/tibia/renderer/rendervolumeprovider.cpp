#include "renderer/rendervolumeprovider.h"

#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

#include <algorithm>
#include <utility>

namespace tibia::renderer {

TRenderVolumeProvider::TRenderVolumeProvider(std::shared_ptr<worldmap::TWorldMapViewport> worldMapViewport,
                                             std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage)
    : m_worldMapViewport(std::move(worldMapViewport))
    , m_worldMapStorage(std::move(worldMapStorage))
{
}

void TRenderVolumeProvider::updateRenderVolume()
{
    const worldmap::TWorldMapCoordinate center = m_worldMapViewport->centerField();
    int firstFloor = center.z;
    if (m_firstFloorMode != CenterFloor) {
        firstFloor = worldmap::TWorldMapViewport::firstFloor(center.z);
        if (m_firstFloorMode == FirstVisibleFloor)
            firstFloor = firstVisibleFloor(m_worldMapStorage, center);
    }
    const worldmap::TShearedWorldMapCuboid volume = m_worldMapViewport->volume();
    m_renderVolume = volume.restrictedToFloors(firstFloor, volume.shearedLastFloorOrigin().z);
}

worldmap::TShearedWorldMapCuboid TRenderVolumeProvider::renderVolume() const
{
    return m_renderVolume;
}

void TRenderVolumeProvider::setRenderVolume(const worldmap::TShearedWorldMapCuboid& renderVolume)
{
    m_renderVolume = renderVolume;
}

worldmap::TShearedWorldMapCuboid TRenderVolumeProvider::visibleArea() const
{
    return m_worldMapViewport->visibleArea();
}

int TRenderVolumeProvider::visibleAreaFloor() const
{
    return m_worldMapViewport->visibleArea().getOrigin().z;
}

std::shared_ptr<worldmap::TWorldMapViewport> TRenderVolumeProvider::worldMapViewport() const
{
    return m_worldMapViewport;
}

int TRenderVolumeProvider::firstFloorMode() const
{
    return m_firstFloorMode;
}

void TRenderVolumeProvider::setFirstFloorMode(const int mode)
{
    if (m_firstFloorMode == mode)
        return;
    m_firstFloorMode = mode;
    updateRenderVolume();
}

int TRenderVolumeProvider::firstVisibleFloor(const std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage,
                                             const worldmap::TWorldMapCoordinate& center) const
{
    return scanFirstVisibleFloor(*worldMapStorage, center);
}

int TRenderVolumeProvider::scanFirstVisibleFloor(worldmap::TWorldMapStorage& worldMapStorage, const worldmap::TWorldMapCoordinate& center)
{
    const int cx = center.x;
    const int cy = center.y;
    const int cz = center.z;
    int first = worldmap::TWorldMapViewport::firstFloor(cz);
    const auto fieldAt = [&worldMapStorage](const int x, const int y, const int z) {
        return worldMapStorage.fieldAt(worldmap::TWorldMapCoordinate(x, y, z));
    };
    // The centre's field and its four neighbours; a floor above covers one of
    // them with a bank or bottom object over it, or a bank right above it.
    for (int x = std::max(1, cx) - 1; x <= cx + 1; ++x) {
        for (int y = std::max(1, cy) - 1; y <= cy + 1; ++y) {
            if (x != cx && y != cy)
                continue;
            // A field that blocks the view itself does not uncover the floors.
            if (const worldmap::TWorldMapField* own = fieldAt(x, y, cz); own && own->hasUnsight())
                continue;
            for (int z = std::max(cz, 0); z >= 1 && z > first; --z) {
                const int above = z - 1;
                const int shift = cz - above;
                const worldmap::TWorldMapField* over = fieldAt(x + shift, y + shift, above);
                if (over && (over->hasBank() || over->hasBottom()) && !over->hasDontHide()) {
                    first = z;
                    break;
                }
                const worldmap::TWorldMapField* roof = fieldAt(x, y, above);
                if (roof && roof->hasBank() && !roof->hasDontHide()) {
                    first = z;
                    break;
                }
            }
        }
    }
    return first;
}

}
