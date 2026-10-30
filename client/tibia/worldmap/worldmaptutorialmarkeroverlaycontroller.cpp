#include "worldmap/worldmaptutorialmarkeroverlaycontroller.h"

#include "input/gameaction.h"

#include <QQuickItem>

namespace tibia::worldmap {

TWorldMapTutorialMarkerOverlayController::TWorldMapTutorialMarkerOverlayController(QQuickItem* overlay, QObject* parent)
    : cip::tibia::worldmap::TWorldMapTutorialMarkerOverlayController(parent)
    , m_overlay(overlay)
{
}

void TWorldMapTutorialMarkerOverlayController::update()
{
    // CIP places TibiaTutorialMarker children under the overlay; markers are
    // created from QML via TTutorialHelperQmlService. Nothing to recompute until
    // a tutorial action adds/removes markers.
}

void TWorldMapTutorialMarkerOverlayController::handleGameAction(const input::TGameActionPtr& action)
{
    if (!action)
        return;
    // ADD/REMOVE tutorial-marker game actions are still stubs in input_ports;
    // update() remains the CIP refresh hook once those land.
    update();
}

}
