#include "worldmap/worldmapviewport.h"

#include "renderer/worldmaprenderprovider.h"

namespace tibia::worldmap {

TWorldMapViewport::TWorldMapViewport(qmlcomponents::TWorldMapRenderProvider* provider, QObject* parent)
    : cip::tibia::worldmap::TWorldMapViewport(parent)
    , m_provider(provider)
{
    if (m_provider) {
        connect(m_provider, &qmlcomponents::TWorldMapRenderProvider::frameAdvanced, this, &TWorldMapViewport::syncFromProvider);
        syncFromProvider();
    }
}

void TWorldMapViewport::syncFromProvider()
{
    if (!m_provider)
        return;
    const TWorldMapExtent extent(m_provider->visibleWidth(), m_provider->visibleHeight(),
                                 static_cast<int>(m_provider->lastFloor()) - static_cast<int>(m_provider->firstFloor()) + 1);
    const TWorldMapSubfieldExtent subfieldExtent(extent);
    const TWorldMapCoordinate camera(m_provider->camera());
    const uint8_t first = m_provider->firstFloor();
    const uint8_t last = m_provider->lastFloor();
    if (extent == m_extent && subfieldExtent == m_subfieldExtent && camera == m_camera && first == m_firstFloor
        && last == m_lastFloor)
        return;
    m_extent = extent;
    m_subfieldExtent = subfieldExtent;
    m_camera = camera;
    m_firstFloor = first;
    m_lastFloor = last;
    Q_EMIT worldMapVolumeChanged();
}

}
