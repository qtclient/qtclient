#pragma once

#include "input/gameaction.h"

#include <client/position.h>
#include <cip_gen.h>

#include <map>
#include <memory>
#include <QPointer>

class QQuickItem;

namespace tibia {
namespace game {
class IGameSession;
}
namespace qmlcomponents {
class TTutorialMarkerQuickItem;
}
namespace renderer {
class TWorldMapCamera;
}
namespace worldmap {
class TWorldMapViewport;
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

    QQuickItem* overlay() const { return m_overlay; }
    /// The markers are placed with the session's viewport and camera; without
    /// a session they are no longer placed.
    void setSession(const std::shared_ptr<game::IGameSession>& session);

public Q_SLOTS:
    /// Places every marker on its field with the camera (one field large at
    /// the camera's scale), making the marker items that are missing.
    void update() override;
    void handleGameAction(const input::IGameActionPtr& action);

    /// ADD_GUI_TUTORIAL_MARKER: the marker stays on the field it was added to.
    void addGuiMarker(const input::TGameActionAddGUITutorialMarker& action);
    /// REMOVE_ALL_TUTORIAL_MARKERS.
    void removeAllMarkers();

private:
    QPointer<QQuickItem> m_overlay;
    std::shared_ptr<TWorldMapViewport> m_viewport;
    std::shared_ptr<renderer::TWorldMapCamera> m_camera;
    /// The field of each marker by marker id.
    std::map<int, Position> m_markerFields;
    /// The shown marker items by marker id.
    std::map<int, QPointer<qmlcomponents::TTutorialMarkerQuickItem>> m_markerItems;
};

}
}
