#pragma once

#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QPointer>
#include <QPointF>

#include <memory>

class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace chat {
class TTextStorage;
}
namespace worldmap {
class TCreatureHUDController;
class TNumericalEffectStorage;
class TWorldMapGameActionHandler;
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
/// mapWindow, by objectName) to the world provider, turns the pane's mouse
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

    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);
    /// The game session's texts, numerical effects and view; the first call
    /// binds the pane's items to the provider.
    void bindSession(const chat::TTextStorage* texts, const worldmap::TNumericalEffectStorage* numericalEffects);
    /// Unsets the provider everywhere; call before the provider goes away.
    void unbind();
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
    /// USE_AT_MOUSECURSOR and SPELL_AT_MOUSE_CURSOR.
    bool handleGameAction(const input::IGameAction& action);
    /// The use goes to the TARGET_SELECTED under the mouse.
    void handleGameActionAtMouseCursor(const input::TGameActionUseAtMouseCursor& action);
    /// The spell's talk aimed at the field under the mouse.
    void handleGameActionCastSpellAtMouseCursor(const input::TGameActionSpellAtMouseCursor& action);
    /// A use at the mouse waits a frame for a handler.
    void trackUseAtMouseCursorGameAction();
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
    worldmap::TCreatureHUDController* m_creatureHuds = nullptr;
    worldmap::TWorldMapTutorialMarkerOverlayController* m_tutorialMarkers = nullptr;
    qmlcomponents::TWorldMapRenderProvider* m_provider = nullptr;
    /// The session's view (the provider's): the picker reads the player's
    /// floor and visible area from it.
    worldmap::TWorldMapViewport* m_viewport = nullptr;
    std::unique_ptr<worldmap::TWorldMapGameActionProvider> m_actions;
    worldmap::TWorldMapGameActionHandler* m_actionHandler = nullptr;
    input::TGameActionMapper m_mapper;
    bool m_bound = false;
    bool m_softwareRenderer = false;
    int m_fovWidth = 15;
    int m_fovHeight = 11;
    qreal m_viewScale = 1;
    bool m_classicView = false;
    bool m_highlightMouseTarget = true;
    bool m_hovering = false;
    QPointF m_hoverPosition;
    /// Frames since an at-mouse use no handler took; -1 when none is tracked.
    int m_unhandledAtMouseFrames = -1;

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
};

}
}
