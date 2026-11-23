#include "minimap/minimapmarkeroverlaycontroller.h"

#include "minimap/minimapmarkercontroller.h"
#include "minimap/minimapmarkerrenderprovider.h"
#include "minimap/minimapvisiblearea.h"
#include "qmlcomponents/qmlincubatorwithcallbackonready.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>

#include <cstdint>
#include <stdexcept>

namespace tibia::minimap {

std::size_t TMinimapMarkerCoordinateHash::operator()(const worldmap::TWorldMapCoordinate& coordinate) const
{
    return static_cast<std::size_t>(static_cast<std::int64_t>(coordinate.z) + static_cast<std::int64_t>(coordinate.y) * 997
                                    + static_cast<std::int64_t>(coordinate.x) * 999983);
}

TMinimapMarkerOverlayController::TMinimapMarkerOverlayController(
    QQuickItem* parentItem, std::shared_ptr<TMinimapMarkerRenderProvider> renderProvider, const TMinimapVisibleArea* area)
    : cip::tibia::minimap::TMinimapMarkerOverlayController(nullptr)
    , m_parentItem(parentItem)
    , m_renderProvider(std::move(renderProvider))
    , m_area(area)
{
    // CipSoft names the file relative to the engine's base URL; qtclient's
    // QML module lives under qrc:/qt/qml/qmlcomponents/qml/.
    m_component = std::make_shared<QQmlComponent>(
        qmlEngine(m_parentItem), QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/MinimapMarker.qml")));
    if (m_component->isError())
        throw std::runtime_error(m_component->errorString().toStdString());
}

TMinimapMarkerOverlayController::~TMinimapMarkerOverlayController() = default;

void TMinimapMarkerOverlayController::refreshMinimapMarkers()
{
    if (!m_renderProvider || !m_area) {
        if (!m_controllers.empty())
            m_controllers.clear();
        return;
    }

    const std::vector<TMinimapMarkerPtr> markers = m_renderProvider->renderInfos();
    auto unlisted = m_controllers;
    for (const TMinimapMarkerPtr& marker : markers) {
        std::shared_ptr<TMinimapMarkerController> controller;
        if (m_controllers.find(marker->coordinate) == m_controllers.end()) {
            auto incubator = std::make_shared<qmlcomponents::TQmlIncubatorWithCallbackOnReady>();
            m_component->create(*incubator);
            controller = std::make_shared<TMinimapMarkerController>(incubator, m_parentItem);
            m_controllers.emplace(marker->coordinate, controller);
        } else {
            controller = m_controllers.at(marker->coordinate);
            unlisted.erase(marker->coordinate);
        }
        controller->setMarker(*marker);
        controller->setPosition(m_area->worldToItem(marker->coordinate.x, marker->coordinate.y));
        controller->update();
    }
    for (const auto& [coordinate, controller] : unlisted)
        m_controllers.erase(coordinate);
}

void TMinimapMarkerOverlayController::update()
{
    refreshMinimapMarkers();
}

}
