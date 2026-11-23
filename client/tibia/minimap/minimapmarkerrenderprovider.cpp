#include "minimap/minimapmarkerrenderprovider.h"

#include "minimap/minimapmarkerstorage.h"
#include "minimap/minimapvisiblearea.h"

namespace tibia::minimap {

TMinimapMarkerRenderProvider::TMinimapMarkerRenderProvider(const TMinimapVisibleArea* area, TMinimapMarkerStorage* storage)
    : m_area(area)
    , m_storage(storage)
{
}

std::vector<TMinimapMarkerPtr> TMinimapMarkerRenderProvider::renderInfos() const
{
    if (!m_storage || !m_area)
        return {};
    const TMinimapVisibleArea* area = m_area;
    return m_storage->markers(
        [area](const worldmap::TWorldMapCoordinate& coordinate) { return area->isNearArea(coordinate.toPosition()); });
}

}
