#pragma once

#include "client/gamecore.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"
#include "minimap/minimapgameactionprovider.h"
#include "minimap/minimapvisiblearea.h"

#include <cip_gen.h>

#include <QPointF>
#include <QPointer>

#include <memory>

class QQuickItem;

namespace tibia::config {
class TSidebarOptions;
}
namespace tibia::minimap {
class TMinimapRenderProvider;
class TMinimapTileStorage;
}
namespace tibia::qmlcomponents {
class TMinimapQuickItem;
}
namespace tibia::worldmap {
class TAmbientLightStorage;
}

namespace tibia::gamewindow {

/// MinimapPanel's controller (CIP: TMinimapController, 0x100b92750): the
/// visible area follows the player; buttons, the rose, dragging, the wheel and
/// the minimap hotkeys move, zoom and change its floor; clicks map to game
/// actions in rule context 6. Modal hover gate (0x100c15470), Soft context
/// menu / marks, and markers are ported. Not ported yet: emulateClickOnAutomapRose.
class TMinimapController final : public cip::tibia::gamewindow::TMinimapController
{
    Q_OBJECT
public:
    explicit TMinimapController(QObject* parent = nullptr);
    ~TMinimapController() override;

    /// CIP 0x100b93540: the minimap data to show; null shows nothing.
    void setTileStorage(minimap::TMinimapTileStorage* storage);
    void setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight);
    /// The character's options the zoom lives in.
    void setSidebarOptions(config::TSidebarOptions* options);
    /// CIP v14: the MinimapPanel instance (its "minimap" and "timedisplay" items).
    void setPanelItem(QQuickItem* panel);
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }

    /// The minimap hotkeys (CIP 0x100b94cc0, actions 93-101).
    void handleGameAction(const input::TGameAction& action);

    int currentLayer() const override;
    void setCurrentLayer(int layer) override;

    /// The shown area and its item (overlays; currentLayerChanged fires on
    /// every refresh).
    const minimap::TMinimapVisibleArea& visibleArea() const { return m_area; }
    qmlcomponents::TMinimapQuickItem* minimapItem() const;
    minimap::TMinimapTileStorage* tileStorage() const;

public Q_SLOTS:
    void updateMinimapVisibleAreaFromQuickitemSize() override;
    void onMinimapDataChanged() override;
    void onZoomOutButtonClicked() override;
    void onZoomInButtonClicked() override;
    void onMoveUpButtonClicked() override;
    void onMoveDownButtonClicked() override;
    void onCenterButtonClicked() override;
    void onMinimapRoseButtonClicked(int offsetX, int offsetY) override;
    void onMinimapDragged(float deltaX, float deltaY) override;
    void onMinimapClick(int x, int y, int triggeringMouseButton, Qt::KeyboardModifiers modifiers) override;
    void onMinimapMarkerClick(QObject* pMinimapMarkerQmlRenderInfo, int TriggeringMouseButton,
                              Qt::KeyboardModifiers Modifiers) override;
    void onMinimapMouseMoved(int x, int y) override;
    void updateMouseCursor() override;
    void updateTimeDisplay() override;
    /// CIP 0x100b93c80: OPEN_CYCLOPEDIA, Map tab (4), at the visible area's
    /// centre.
    void onCyclopediaMapButtonClicked() override;

private:
    /// CIP 0x100b92d80: area to the storage, crosshair, repaint, cursor.
    void refresh();
    void onPlayerPositionChanged(const Position& position);
    void centerOnPlayer();
    /// Fields one rose click or scroll hotkey moves: round(12 / scale).
    int scrollStep() const;
    /// CIP 0x100b94f00: the player's field is light (blue 255) on the minimap.
    bool crosshairIsDark() const;
    input::TGameActionList actionsAtMouse() const;

    minimap::TMinimapVisibleArea m_area;
    minimap::TMinimapGameActionProvider m_actions;
    input::TGameActionMapper m_mapper;
    QPointer<minimap::TMinimapTileStorage> m_storage;
    std::unique_ptr<minimap::TMinimapRenderProvider> m_renderProvider;
    QPointer<worldmap::TAmbientLightStorage> m_ambientLight;
    QPointer<QQuickItem> m_panel;
    QPointer<qmlcomponents::TMinimapQuickItem> m_minimap;
    QPointer<QQuickItem> m_timeDisplay;
    Position m_playerPosition;
    QPoint m_mouse;
    QPointF m_dragRemainder;
    client::gamecore::Hook m_positionHook;
};

}
