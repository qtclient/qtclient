#include "minimaprenderprovider.h"

#include "minimap/minimaptilestorage.h"
#include "minimap/minimapvisiblearea.h"

namespace tibia::minimap {

TMinimapRenderProvider::TMinimapRenderProvider(TMinimapTileStorage* storage, const TMinimapVisibleArea* area)
    : m_storage(storage)
    , m_area(area)
{}

std::vector<TMinimapRenderProvider::TileRenderInfo> TMinimapRenderProvider::renderInfos() const
{
    std::vector<TileRenderInfo> infos;
    if (!m_storage || !m_area)
        return infos;
    for (const auto& [key, tile] : m_storage->tiles()) {
        if (!m_area->intersectsTile(tile->id()))
            continue;
        infos.push_back({ tile->id(), tile->colorImage() });
    }
    return infos;
}

}
