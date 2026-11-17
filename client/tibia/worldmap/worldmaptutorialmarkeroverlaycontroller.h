#pragma once

#include "input/gameaction.h"

#include <client/position.h>
#include <cip_gen.h>

#include <map>
#include <QPointer>

class QQuickItem;

namespace tibia {
namespace qmlcomponents {
class TTutorialMarkerQuickItem;
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace worldmap {

/// Tutorial arrows over the world map (CIP: TWorldMapTutorialMarkerOverlayController;
/// MapWindowPane `tutorialMarkerOverlay`). Marker placement uses
/// qmlcomponents::TTutorialHelperQmlService / TTutorialMarkerQuickItem.
class TWorldMapTutorialMarkerOverlayController final
    : public cip::tibia::worldmap::TWorldMapTutorialMarkerOverlayController
{
    Q_OBJECT
public:
    explicit TWorldMapTutorialMarkerOverlayController(QQuickItem* overlay, QObject* parent = nullptr);
    ~TWorldMapTutorialMarkerOverlayController() override;

    static TWorldMapTutorialMarkerOverlayController* instance();

    QQuickItem* overlay() const { return m_overlay; }
    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);

public Q_SLOTS:
    void update() override;
    void handleGameAction(const input::TGameActionPtr& action);

    /// ADD_GUI_TUTORIAL_MARKER: the marker stays on the field it was added to.
    void addGuiMarker(const input::TGameAction& action);
    /// REMOVE_ALL_TUTORIAL_MARKERS.
    void removeAllMarkers();

private:
    QPoint overlayPosFor(const Position& field) const;

    QPointer<QQuickItem> m_overlay;
    QPointer<qmlcomponents::TWorldMapRenderProvider> m_provider;
    /// The field of each marker by marker id.
    std::map<int, Position> m_markerFields;
    /// The shown marker items by marker id.
    std::map<int, QPointer<qmlcomponents::TTutorialMarkerQuickItem>> m_markerItems;

    static TWorldMapTutorialMarkerOverlayController* s_instance;
};

}
}
