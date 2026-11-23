#pragma once

#include "minimap/minimapmarker.h"

#include <cip_gen.h>

#include <QPoint>

#include <memory>

class QQuickItem;

namespace tibia {
namespace qmlcomponents {
class TQmlIncubatorWithCallbackOnReady;
}
}

namespace tibia {
namespace minimap {

class TMinimapMarkerQmlRenderInfo;

/// One marker on the minimap's marker overlay (CIP: TMinimapMarkerController):
/// the MinimapMarker.qml item its incubator creates, shown under the overlay
/// item with the marker's render info and placed at its field.
class TMinimapMarkerController final : public cip::tibia::minimap::TMinimapMarkerController
{
    Q_OBJECT
public:
    /// Takes the item as soon as `incubator` is ready (at once when it already is).
    TMinimapMarkerController(const std::shared_ptr<qmlcomponents::TQmlIncubatorWithCallbackOnReady>& incubator,
                             QQuickItem* parentItem);
    /// Stops the incubation; hides the item and deletes it later.
    ~TMinimapMarkerController() override;

    /// Shows `marker` in the item's render info.
    void setMarker(const TMinimapMarker& marker);
    /// The overlay pixel of the marker's field.
    void setPosition(const QPoint& position);
    /// Takes the item once the incubator is ready, then moves it to six
    /// pixels left of and above the position when it is elsewhere.
    void update();

private:
    void setItem(QQuickItem* item);

    std::shared_ptr<qmlcomponents::TQmlIncubatorWithCallbackOnReady> m_incubator;
    QQuickItem* m_parentItem;
    std::shared_ptr<TMinimapMarkerQmlRenderInfo> m_renderInfo;
    QPoint m_position;
    QQuickItem* m_item = nullptr;
};

}
}
