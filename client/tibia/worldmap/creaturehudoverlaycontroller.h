#pragma once

#include <cip_gen.h>

#include <QMetaObject>
#include <QPointer>

#include <cstdint>
#include <memory>
#include <unordered_map>

class QQmlComponent;
class QQuickItem;

namespace tibia {
namespace game {
class IGameSession;
}
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
namespace renderer {
class TWorldMapCamera;
}
}

namespace tibia {
namespace worldmap {

class TCreatureHUDController;
class TCreatureHUDRenderProvider;
class TWorldMapViewport;

/// Keeps one TCreatureHUDController per creature the HUD render provider
/// lists, and drops the controllers of creatures it no longer lists (CIP:
/// TCreatureHUDOverlayController).
class TCreatureHUDOverlayController final : public cip::tibia::worldmap::TCreatureHUDOverlayController
{
    Q_OBJECT
public:
    /// `parentItem` is MapWindowPane's worldmapCreatureHUDOverlay: it covers
    /// the scaled map, in unscaled pixels.
    explicit TCreatureHUDOverlayController(QQuickItem* parentItem, QObject* parent = nullptr);
    ~TCreatureHUDOverlayController() override;

    /// Takes the session's HUD render provider, viewport and camera and makes
    /// the CreatureHUD.qml component once; a null session drops them and the
    /// HUDs. Refreshes on every frame of the world map.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    void refreshCreatureHUDs() override;
    void update() override;

private:
    std::shared_ptr<TCreatureHUDRenderProvider> m_renderProvider;
    std::shared_ptr<TWorldMapViewport> m_viewport;
    std::shared_ptr<renderer::TWorldMapCamera> m_camera;
    /// Host frame source: refreshCreatureHUDs maps the HUD anchors with the
    /// world map frame's camera and scale, not m_viewport / m_camera.
    QPointer<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    QPointer<QQuickItem> m_parentItem;
    std::unordered_map<uint32_t, std::shared_ptr<TCreatureHUDController>> m_controllers;
    QQmlComponent* m_component = nullptr;
    QMetaObject::Connection m_frameConnection;
};

}
}
