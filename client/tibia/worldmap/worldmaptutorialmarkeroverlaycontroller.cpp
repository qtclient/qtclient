#include "worldmap/worldmaptutorialmarkeroverlaycontroller.h"

#include "creatures/creaturestorage.h"
#include "qmlcomponents/tutorialhelper.h"
#include "renderer/worldmaprenderprovider.h"

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

void TWorldMapTutorialMarkerOverlayController::setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider)
{
    if (m_provider == provider)
        return;
    if (m_provider)
        disconnect(m_provider, nullptr, this, nullptr);
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, &qmlcomponents::TWorldMapRenderProvider::overlaysAdvanced, this,
                &TWorldMapTutorialMarkerOverlayController::update);
    }
}

QPoint TWorldMapTutorialMarkerOverlayController::overlayPosFor(const Position& field) const
{
    if (!m_provider || !field.isValid())
        return {};
    const auto& frame = m_provider->frame();
    if (!frame.valid)
        return {};
    const QPoint mapPixel = m_provider->mapPixelOf(field);
    return QPoint(mapPixel.x() - frame.camera.x(), mapPixel.y() - frame.camera.y());
}

void TWorldMapTutorialMarkerOverlayController::update()
{
    if (!m_overlay || !m_provider)
        return;
    for (const auto& [markerId, field] : m_markerFields) {
        QPointer<qmlcomponents::TTutorialMarkerQuickItem>& item = m_markerItems[markerId];
        if (!item) {
            item = new qmlcomponents::TTutorialMarkerQuickItem(m_overlay);
            item->setWidth(32);
            item->setHeight(32);
            item->setMarkerID(markerId);
            item->setZ(999);
            item->setVisible(true);
        }
        item->setPosition(overlayPosFor(field));
    }
}

void TWorldMapTutorialMarkerOverlayController::handleGameAction(const input::TGameActionPtr& action)
{
    if (!action)
        return;
    if (action->type == input::EGameAction::REMOVE_ALL_TUTORIAL_MARKERS) {
        removeAllMarkers();
        return;
    }
    if (action->type == input::EGameAction::ADD_GUI_TUTORIAL_MARKER)
        addGuiMarker(*action);
}

void TWorldMapTutorialMarkerOverlayController::addGuiMarker(const input::TGameAction& action)
{
    // A marker on a map field, or on the field of a creature when it is added.
    Position field = action.position;
    if (action.creatureId != 0) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> creature = storage->creatureAt(action.creatureId))
                field = creature->position();
        }
    }
    if (field.isValid())
        m_markerFields[action.parameter] = field;
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
