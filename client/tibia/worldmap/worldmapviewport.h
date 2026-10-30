#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::worldmap {

/// Visible world volume driven by the render provider (CIP: TWorldMapViewport;
/// worldMapVolumeChanged when FOV / floors change).
class TWorldMapViewport final : public cip::tibia::worldmap::TWorldMapViewport
{
    Q_OBJECT
public:
    explicit TWorldMapViewport(qmlcomponents::TWorldMapRenderProvider* provider, QObject* parent = nullptr);

    TWorldMapExtent extent() const { return m_extent; }
    TWorldMapSubfieldExtent subfieldExtent() const { return m_subfieldExtent; }
    TWorldMapCoordinate camera() const { return m_camera; }
    uint8_t firstFloor() const { return m_firstFloor; }
    uint8_t lastFloor() const { return m_lastFloor; }

    /// Pull FOV and floors from the provider after a frame (CIP volume refresh).
    void syncFromProvider();

private:
    qmlcomponents::TWorldMapRenderProvider* m_provider = nullptr;
    TWorldMapExtent m_extent;
    TWorldMapSubfieldExtent m_subfieldExtent;
    TWorldMapCoordinate m_camera;
    uint8_t m_firstFloor = 0;
    uint8_t m_lastFloor = 0;
};

}
