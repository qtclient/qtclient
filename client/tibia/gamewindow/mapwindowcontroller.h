#pragma once

#include "input/gameactiondispatch.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QPointer>
#include <QPointF>

#include <memory>

class QQuickItem;
class QQuickWindow;

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace worldmap {
class TCreatureHUDOverlayController;
class TWorldMapGameActionProvider;
class TWorldMapTutorialMarkerOverlayController;
class TWorldMapViewport;
}
namespace qmlcomponents {
class TLightmapPaintedItem;
class TNumericalEffectOverlayQuickItem;
class TSpeechBubbleOverlayQuickItem;
class TWorldMapQuickItem;
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace gamewindow {

/// MapWindowPane's mapWindowController (CIP: TMapWindowController): binds the
/// pane's items (worldmap, lightmap, worldmapCreatureHUDOverlay,
/// tutorialMarkerOverlay, speechBubbleOverlay, numericalEffectOverlay and
/// mapWindow, by objectName) to the game session's world, turns the pane's mouse
/// input into game actions, keeps the hovered thing and cursor shape per frame,
/// and holds the render options and the pane's unused width.
class TMapWindowController final : public cip::tibia::gamewindow::TMapWindowController
{
    Q_OBJECT
    /// FOV in fields. Always CIP 15×11 (HUD and classic share the same world size).
    Q_PROPERTY(int fovWidth READ fovWidth NOTIFY fovChanged)
    Q_PROPERTY(int fovHeight READ fovHeight NOTIFY fovChanged)
    /// Classic vs product HUD chrome (chat/status). FOV stays 15×11 either way;
    /// grey gutters / unusedWidth feed CIP expandable sidebars in both modes.
    Q_PROPERTY(bool classicView READ classicView NOTIFY classicViewChanged)
    /// CIP map scale: map window height / 352.
    Q_PROPERTY(qreal viewScale READ viewScale NOTIFY fovChanged)
public:
    /// `pane` is the MapWindowPane item; the controller is its mapWindowController.
    TMapWindowController(QQuickItem* pane, bool softwareRenderer, QObject* parent = nullptr);
    ~TMapWindowController() override;

    /// Drops the window's pointer filter and the game session.
    void unbind();
    /// CIP connector: the pane's items draw `GameSession`'s world, texts and
    /// numerical effects, and the mouse picks in its view; null unsets them.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// False when the pane lacks the world map.
    bool isBound() const { return m_worldMap != nullptr; }

    int fovWidth() const { return m_fovWidth; }
    int fovHeight() const { return m_fovHeight; }
    bool classicView() const;
    qreal viewScale() const { return m_viewScale; }

    /// The render options: textual effects, antialiasing mode, even-multiple
    /// scaling and the light map (which needs a GPU renderer).
    void onMapWindowControllerOptionsChanged() override;
    void onMapWindowEntered() override;
    /// Also drops the provider's hover object.
    void onMapWindowExited() override;
    /// Re-emits the pane's unusedWidth.
    void onUnusedWidthChanged() override;
    void clearTrackedUseAtMouseCursorGameAction() override;
    /// Keep FOV at CIP 15×11 and refresh viewScale from the map window height.
    void syncFov();
    /// Classic vs product pack HUD chrome layout (FOV unchanged).
    void refreshClassicView();

public Q_SLOTS:
    /// MapWindowPane's mapWindowClicked: mouse in mapWindow pixels.
    void onMapWindowClicked(int mouseX, int mouseY, int mouseButton, int keyboardModifiers);
    /// MapWindowPane's mapWindowStartDrag: the first MOVE_OBJECT under the mouse.
    void onMapWindowStartDrag(int mouseX, int mouseY, QObject* source);
    /// MapWindowPane's mapWindowDragDropped: the drag moves to the picked field.
    void onMapWindowDragDropped(int mouseX, int mouseY);
    /// MapWindowPane's TibiaTargetSelection: publishes the TARGET_SELECTED under the click.
    void onMapWindowTargetSelected(int mouseX, int mouseY);

Q_SIGNALS:
    void fovChanged();
    void classicViewChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// The use goes to the TARGET_SELECTED under the mouse; it is tracked
    /// until the next frames tell whether a window took it.
    void handleGameActionAtMouseCursor(const std::shared_ptr<const input::TGameActionUseAtMouseCursor>& action);
    /// The spell's talk aimed at the field under the mouse.
    void handleGameActionCastSpellAtMouseCursor(const input::TGameActionSpellAtMouseCursor& action);
    /// A use at the mouse waits a frame for a handler.
    void trackUseAtMouseCursorGameAction(const std::shared_ptr<const input::TGameActionUseAtMouseCursor>& action);
    /// Per frame: an untaken use at the mouse reports "no target".
    void updateTrackedUseAtMouseCursor();
    /// The mouse cursor in mapWindow pixels; `onMap` when it is over the map and no dialog is up.
    QPointF mouseCursorPosition(bool& onMap) const;
    void applyTargetHighlightOption();

    QPoint toMapPixel(const QPointF& mapWindowPoint) const;
    /// The TARGET_SELECTED candidate at a mapWindow point, creatures first.
    input::TTargetSelectedPtr targetAt(const QPointF& mapWindowPoint) const;
    /// Refresh m_hovering / m_hoverPosition from the live cursor inside mapWindow.
    /// IdleOT HUD chrome (action bar, etc.) sits above the FOV and steals the
    /// map MouseArea's hover; the blue baseplate must still track map tiles under it.
    bool syncHoverFromCursor();
    /// The hover's cursor shape and highlighted thing, once per frame.
    void updateMouseTarget();
    /// Keep a filter on the QQuickWindow so overlay HUD cannot eat map drag/click.
    void ensureWindowPointerFilter();
    /// Window-level press/move/release: start map drag (and optionally click) when
    /// HUD chrome above the FOV owns the MouseArea grab.
    bool filterWindowPointer(QEvent* event);
    /// Top-most QQuickItem under a scene position (front-to-back walk).
    QQuickItem* leafItemAtScenePos(const QPointF& scenePos) const;
    /// True when the leaf is pack/HUD chrome that must not synthesize a map
    /// click. FOV MouseArea stays false. Items with passMapPointer are skipped
    /// in leafItemAtScenePos; absorbMapClicks / MouseArea / AbstractButton
    /// ancestors count as interactive.
    bool isInteractiveOverlay(const QQuickItem* leaf) const;
    void ungrabOverlayPointer(QQuickItem* from) const;

    QPointer<QQuickItem> m_pane;
    QPointer<QQuickItem> m_mapWindow;
    QPointer<qmlcomponents::TWorldMapQuickItem> m_worldMap;
    QPointer<qmlcomponents::TNumericalEffectOverlayQuickItem> m_numericalEffects;
    QPointer<qmlcomponents::TSpeechBubbleOverlayQuickItem> m_speechBubbles;
    QPointer<qmlcomponents::TLightmapPaintedItem> m_lightMap;
    worldmap::TCreatureHUDOverlayController* m_creatureHuds = nullptr;
    worldmap::TWorldMapTutorialMarkerOverlayController* m_tutorialMarkers = nullptr;
    /// The game session the map shows; its pieces below.
    std::shared_ptr<game::IGameSession> m_gameSession;
    qmlcomponents::TWorldMapRenderProvider* m_provider = nullptr;
    /// The session's view: the picker reads the player's floor and visible
    /// area from it.
    worldmap::TWorldMapViewport* m_viewport = nullptr;
    std::shared_ptr<worldmap::TWorldMapGameActionProvider> m_actions;
    input::TGameActionMapper m_mapper;
    bool m_softwareRenderer = false;
    int m_fovWidth = 15;
    int m_fovHeight = 11;
    qreal m_viewScale = 1;
    bool m_classicView = false;
    bool m_highlightMouseTarget = true;
    bool m_hovering = false;
    QPointF m_hoverPosition;
    /// CIP +0x140: the tracked use at the mouse cursor, and the frames since (+0x150).
    std::shared_ptr<const input::TGameActionUseAtMouseCursor> m_trackedUseAtMouseCursor;
    int m_trackedUseAtMouseCursorFrames = 0;

    /// MapWindowPane mouseAreaDrag — FOV grabber (under IdleOT HUD z-order).
    QPointer<QQuickItem> m_mapMouseArea;
    QPointer<QQuickWindow> m_pointerWindow;
    /// Window pointer bridge (HUD-over-FOV). Matches TibiaStyle.dragThreshold.
    static constexpr qreal MapDragThresholdPx = 10.;
    bool m_mapPressActive = false;
    bool m_mapDragStarted = false;
    bool m_mapOwnedPress = false;
    Qt::MouseButton m_mapPressButton = Qt::NoButton;
    Qt::KeyboardModifiers m_mapPressModifiers = Qt::NoModifier;
    QPointF m_mapPressPos;

public Q_SLOTS:
    /// CIP: USE_AT_MOUSECURSOR and SPELL_AT_MOUSE_CURSOR from the router
    /// (connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
};

}
}
