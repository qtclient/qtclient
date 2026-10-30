#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

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

    QQuickItem* overlay() const { return m_overlay; }

public Q_SLOTS:
    void update() override;
    void handleGameAction(const input::TGameActionPtr& action);

private:
    QPointer<QQuickItem> m_overlay;
};

}
