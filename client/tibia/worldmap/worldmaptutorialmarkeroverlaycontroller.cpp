#include "worldmap/worldmaptutorialmarkeroverlaycontroller.h"

#include "game/gamesessionbase.h"
#include "qmlcomponents/tutorialhelper.h"
#include "renderer/worldmapcamera.h"
#include "worldmap/coordinates.h"
#include "worldmap/worldmapviewport.h"

#include <client/position.h>

#include <QQuickItem>

namespace tibia::worldmap {

TWorldMapTutorialMarkerOverlayController* TWorldMapTutorialMarkerOverlayController::s_instance = nullptr;

TWorldMapTutorialMarkerOverlayController::TWorldMapTutorialMarkerOverlayController(QQuickItem* overlay, QObject* parent)
    : cip::tibia::worldmap::TWorldMapTutorialMarkerOverlayController(parent)
    , m_overlay(overlay)
{
    s_instance = this;
}

TWorldMapTutorialMarkerOverlayController::~TWorldMapTutorialMarkerOverlayController()
{
    if (s_instance == this)
        s_instance = nullptr;
}

TWorldMapTutorialMarkerOverlayController* TWorldMapTutorialMarkerOverlayController::instance()
{
    return s_instance;
}

void TWorldMapTutorialMarkerOverlayController::setSession(const std::shared_ptr<game::IGameSession>& session)
{
    // Host: the map window has no frame tick that updates the overlay; the
    // camera's change stands for it.
    if (m_camera)
        disconnect(m_camera.get(), nullptr, this, nullptr);
    const auto* base = dynamic_cast<const game::TGameSessionBase*>(session.get());
    if (!base) {
        m_viewport.reset();
        m_camera.reset();
        update();
        return;
    }
    m_viewport = base->worldMapViewport();
    m_camera = base->worldMapCamera();
    if (m_camera) {
        connect(m_camera.get(), &renderer::TWorldMapCamera::cameraViewportChanged, this,
                &TWorldMapTutorialMarkerOverlayController::update);
    }
}

void TWorldMapTutorialMarkerOverlayController::update()
{
    if (!m_viewport || !m_camera)
        return;
    const qreal size = static_cast<qreal>(m_camera->scaleFactor()) * 32.0;
    for (const auto& [markerId, field] : m_markerFields) {
        const QPoint projected =
            TWorldMapSubfieldCoordinate::fromField(TWorldMapCoordinate(field), 0, 0).projectedToLayer(0);
        const QPointF position = m_camera->matrix().map(QPointF(projected));
        QPointer<qmlcomponents::TTutorialMarkerQuickItem>& item = m_markerItems[markerId];
        if (!item) {
            if (!m_overlay)
                continue;
            item = new qmlcomponents::TTutorialMarkerQuickItem(m_overlay);
            item->setMarkerID(markerId);
            item->setZ(999);
            item->setVisible(true);
        }
        item->setPosition(position);
        item->setSize(QSizeF(size, size));
    }
}

void TWorldMapTutorialMarkerOverlayController::handleGameAction(const input::IGameActionPtr& action)
{
    if (!action)
        return;
    if (action->gameActionType() == input::EGameAction::REMOVE_ALL_TUTORIAL_MARKERS) {
        removeAllMarkers();
        return;
    }
    if (const auto* marker = input::gameActionAs<input::TGameActionAddGUITutorialMarker>(action))
        addGuiMarker(*marker);
}

void TWorldMapTutorialMarkerOverlayController::addGuiMarker(const input::TGameActionAddGUITutorialMarker& action)
{
    // A marker on a map field. A marker on a creature is published again by
    // the creatures' game action handler, located at the creature's field.
    const Position field = action.position();
    if (field.isValid())
        m_markerFields[action.markerId()] = field;
    update();
}

void TWorldMapTutorialMarkerOverlayController::removeAllMarkers()
{
    for (auto& [markerId, item] : m_markerItems) {
        if (item)
            item->deleteLater();
    }
    m_markerItems.clear();
    m_markerFields.clear();
}

}
