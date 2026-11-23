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
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace worldmap {

class TCreatureHUDController;
class TCreatureHUDRenderProvider;

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

    /// The session's HUD records, and the world map frame that places them
    /// on the overlay; refreshes on every frame of the world map.
    void setRenderProviders(std::shared_ptr<TCreatureHUDRenderProvider> renderProvider,
                            qmlcomponents::TWorldMapRenderProvider* worldMapRenderProvider);

public Q_SLOTS:
    void refreshCreatureHUDs() override;
    void update() override;

private:
    std::shared_ptr<TCreatureHUDRenderProvider> m_renderProvider;
    /// Stands in for CipSoft's camera viewport: the frame's camera and scale
    /// map the HUD anchors onto the overlay.
    QPointer<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    QPointer<QQuickItem> m_parentItem;
    std::unordered_map<uint32_t, std::shared_ptr<TCreatureHUDController>> m_controllers;
    QQmlComponent* m_component = nullptr;
    QMetaObject::Connection m_frameConnection;
};

}
}
