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

void TWorldMapTutorialMarkerOverlayController::ensureMarker(const int markerId, const Position& field,
                                                           const uint32_t creatureId)
{
    if (!m_overlay || markerId == 0)
        return;
    MarkerRecord& record = m_markers[markerId];
    if (field.isValid())
        record.field = field;
    if (creatureId != 0)
        record.creatureId = creatureId;
    if (!record.item) {
        auto* item = new qmlcomponents::TTutorialMarkerQuickItem(m_overlay);
        item->setWidth(32);
        item->setHeight(32);
        item->setMarkerID(markerId);
        item->setVisible(true);
        item->setZ(999);
        record.item = item;
    }
    if (record.item) {
        record.item->setPosition(overlayPosFor(record.field));
        record.item->setVisible(true);
    }
}

void TWorldMapTutorialMarkerOverlayController::update()
{
    for (auto it = m_markers.begin(); it != m_markers.end(); ++it) {
        MarkerRecord& record = it.value();
        if (!record.item)
            continue;
        if (record.creatureId != 0 && m_provider) {
            for (const auto& hud : m_provider->frame().creatures) {
                if (hud.creatureId == record.creatureId) {
                    const auto& frame = m_provider->frame();
                    record.item->setPosition(QPointF(hud.point.x() - frame.camera.x(),
                                                     hud.point.y() - frame.camera.y()));
                    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
                        if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(hud.creatureId))
                            record.field = row->position();
                    }
                    break;
                }
            }
        } else if (record.field.isValid()) {
            record.item->setPosition(overlayPosFor(record.field));
        }
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
    int markerId = action.parameter;
    if (markerId <= 0) {
        if (qmlcomponents::TTutorialHelperQmlService* helper = qmlcomponents::TTutorialHelperQmlService::instance())
            markerId = helper->nextUniqueTutorialMarkerID();
        else
            markerId = 1;
    }
    ensureMarker(markerId, action.position, action.creatureId);
    update();
}

void TWorldMapTutorialMarkerOverlayController::removeAllMarkers()
{
    for (auto it = m_markers.begin(); it != m_markers.end(); ++it) {
        if (it.value().item)
            it.value().item->deleteLater();
    }
    m_markers.clear();
}

}
