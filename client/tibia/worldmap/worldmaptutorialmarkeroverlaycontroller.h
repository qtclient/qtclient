#pragma once

#include "input/gameaction.h"

#include <client/position.h>
#include <cip_gen.h>

#include <QHash>
#include <QPointer>

class QQuickItem;

namespace tibia::qmlcomponents {
class TTutorialMarkerQuickItem;
class TWorldMapRenderProvider;
}

namespace tibia::worldmap {

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

    /// qtclient entry from TTutorialShell ADD_GUI / REMOVE_ALL.
    void addGuiMarker(const input::TGameAction& action);
    void removeAllMarkers();

private:
    struct MarkerRecord {
        QPointer<qmlcomponents::TTutorialMarkerQuickItem> item;
        Position field;
        uint32_t creatureId = 0;
    };

    void ensureMarker(int markerId, const Position& field, uint32_t creatureId);
    QPoint overlayPosFor(const Position& field) const;

    QPointer<QQuickItem> m_overlay;
    QPointer<qmlcomponents::TWorldMapRenderProvider> m_provider;
    QHash<int, MarkerRecord> m_markers;

    static TWorldMapTutorialMarkerOverlayController* s_instance;
};

}
