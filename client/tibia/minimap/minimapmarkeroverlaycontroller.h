#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <cstddef>
#include <memory>
#include <unordered_map>

class QQmlComponent;
class QQuickItem;

namespace tibia {
namespace minimap {

class TMinimapMarkerController;
class TMinimapMarkerRenderProvider;
class TMinimapVisibleArea;

/// Hash of a field for the overlay's controller map: z + 997 y + 999983 x.
struct TMinimapMarkerCoordinateHash
{
    std::size_t operator()(const worldmap::TWorldMapCoordinate& coordinate) const;
};

/// The minimap's marker overlay (CIP: TMinimapMarkerOverlayController): one
/// MinimapMarker.qml item per marker the render provider lists, created under
/// the panel's "minimapMarkerOverlay" item and kept at its field.
class TMinimapMarkerOverlayController final : public cip::tibia::minimap::TMinimapMarkerOverlayController
{
    Q_OBJECT
public:
    /// Loads MinimapMarker.qml with the engine of `parentItem`; throws
    /// std::runtime_error when the component has errors.
    TMinimapMarkerOverlayController(QQuickItem* parentItem, std::shared_ptr<TMinimapMarkerRenderProvider> renderProvider,
                                    const TMinimapVisibleArea* area);
    ~TMinimapMarkerOverlayController() override;

public Q_SLOTS:
    /// Keeps the controllers of the listed markers (one per field, created
    /// when new), shows each marker at its field and drops the others; without
    /// render provider or area, drops every controller.
    void refreshMinimapMarkers() override;
    void update() override;

private:
    QQuickItem* m_parentItem;
    std::shared_ptr<TMinimapMarkerRenderProvider> m_renderProvider;
    const TMinimapVisibleArea* m_area;
    std::unordered_map<worldmap::TWorldMapCoordinate, std::shared_ptr<TMinimapMarkerController>,
                       TMinimapMarkerCoordinateHash>
        m_controllers;
    std::shared_ptr<QQmlComponent> m_component;
};

}
}
