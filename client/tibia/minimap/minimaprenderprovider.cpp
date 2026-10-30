#include "minimaprenderprovider.h"

#include "minimap/minimapciptypes.h"
#include "minimap/minimaptilestorage.h"
#include "minimap/minimapvisiblearea.h"
#include "sprites/sprite.h"

namespace tibia::minimap {

TMinimapRenderProvider::TMinimapRenderProvider(TMinimapTileStorage* storage, const TMinimapVisibleArea* area)
    : m_storage(storage)
    , m_area(area)
{}

std::vector<TMinimapRenderProvider::TileRenderInfo> TMinimapRenderProvider::renderInfos() const
{
    // CIP TMinimapMarkerRenderProvider / scenegraph draw-command provider sit
    // beside tile colour blit when markers are drawn into the same frame.
    (void)TMinimapMarkerRenderProvider();
    (void)TMinimapRendererScenegraphDrawCommandProvider();
    std::vector<TileRenderInfo> infos;
    if (!m_storage || !m_area)
        return infos;
    for (const auto& [key, tile] : m_storage->tiles()) {
        if (!m_area->intersectsTile(tile->id()))
            continue;
        // CIP sprites::TMinimapTileSpritesheetIdentifier — tile keyed by packed sector id.
        const Position& id = tile->id();
        const uint32_t packed = (static_cast<uint32_t>(id.x) << 16) | (static_cast<uint32_t>(id.y) & 0xffffu);
        (void)sprites::TMinimapTileSpritesheetIdentifier(packed);
        infos.push_back({ id, tile->colorImage() });
    }
    return infos;
}

}
