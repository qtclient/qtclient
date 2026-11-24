#include "worldmap/creaturehudoverlaycontroller.h"

#include "game/gamesessionbase.h"
#include "renderer/mapscale.h"
#include "renderer/worldmapcamera.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmapviewport.h"
#include "worldmap/creaturehudcontroller.h"
#include "worldmap/creaturehudrenderprovider.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlIncubator>
#include <QQuickItem>

#include <stdexcept>

namespace tibia::worldmap {

namespace {

using Provider = qmlcomponents::TWorldMapRenderProvider;

}

TCreatureHUDOverlayController::TCreatureHUDOverlayController(QQuickItem* parentItem, QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDOverlayController(parent)
    , m_parentItem(parentItem)
{
}

TCreatureHUDOverlayController::~TCreatureHUDOverlayController()
{
    disconnect(m_frameConnection);
    m_controllers.clear();
}

void TCreatureHUDOverlayController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    disconnect(m_frameConnection);
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    if (!session) {
        m_renderProvider.reset();
        m_viewport.reset();
        m_camera.reset();
        m_worldMapRenderProvider = nullptr;
        refreshCreatureHUDs();
        return;
    }
    m_renderProvider = session->creatureHUDRenderProvider();
    m_viewport = session->worldMapViewport();
    m_camera = session->worldMapCamera();
    // Host: the HUDs follow the world map frame (see m_worldMapRenderProvider).
    m_worldMapRenderProvider = session->worldMapRenderProvider().get();
    if (m_worldMapRenderProvider) {
        m_frameConnection = connect(m_worldMapRenderProvider.data(), &Provider::overlaysAdvanced, this,
                                    &TCreatureHUDOverlayController::update);
    }
    if (m_component)
        return;
    // Host: an item outside a QML engine (tests) has no component to make.
    QQmlEngine* engine = m_parentItem ? qmlEngine(m_parentItem.data()) : nullptr;
    if (!engine)
        return;
    m_component = new QQmlComponent(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/CreatureHUD.qml")),
                                    QQmlComponent::PreferSynchronous, this);
    if (m_component->isError())
        throw std::runtime_error(m_component->errorString().toStdString());
}

void TCreatureHUDOverlayController::update()
{
    refreshCreatureHUDs();
}

void TCreatureHUDOverlayController::refreshCreatureHUDs()
{
    if (!m_renderProvider || !m_worldMapRenderProvider || !m_parentItem || !m_component || !m_component->isReady()) {
        m_controllers.clear();
        return;
    }
    const Provider::Frame& frame = m_worldMapRenderProvider->frame();
    if (!frame.valid) {
        m_controllers.clear();
        return;
    }

    const std::vector<TCreatureHUDRenderInfo> infos = m_renderProvider->creatureHUDRenderInfos();
    // The controllers of creatures no longer listed are dropped at the end.
    std::unordered_map<uint32_t, std::shared_ptr<TCreatureHUDController>> stale = m_controllers;

    // The overlay covers the map at the pane's scale (uniform, square pixels).
    const int mapWidth = frame.visibleWidth * Provider::FieldSize;
    const int mapHeight = frame.visibleHeight * Provider::FieldSize;
    const qmlcomponents::MapCoverScale cover =
        qmlcomponents::MapCoverScale::placed(m_parentItem->width(), m_parentItem->height(), mapWidth, mapHeight, frame.viewScale);
    const auto scale = static_cast<float>(cover.scale);
    const double fieldSize = static_cast<double>(scale) * Provider::FieldSize;

    for (const TCreatureHUDRenderInfo& info : infos) {
        std::shared_ptr<TCreatureHUDController> controller;
        if (const auto found = m_controllers.find(info.creatureId); found != m_controllers.end()) {
            controller = found->second;
            stale.erase(info.creatureId);
        } else {
            auto incubator = std::make_shared<QQmlIncubator>();
            m_component->create(*incubator, qmlContext(m_parentItem));
            controller = std::make_shared<TCreatureHUDController>(incubator, m_parentItem.data());
            m_controllers.emplace(info.creatureId, controller);
        }
        const QPoint anchor = info.anchor.toMapPixel();
        const QPointF mapped = cover.toPane(anchor.x() - frame.camera.x(), anchor.y() - frame.camera.y());
        controller->setRenderInfo(info);
        controller->setGeometry(QRectF(mapped.x(), mapped.y(), fieldSize, fieldSize));
        controller->setScaleFactor(scale);
        controller->update();
    }

    for (const auto& [creatureId, controller] : stale)
        m_controllers.erase(creatureId);
}

}
