#pragma once

#include "input/gameactionmapper.h"

#include <QObject>
#include <QPointF>
#include <QPointer>

#include <memory>

class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace chat {
class TTextStorage;
}
}
namespace tibia {
namespace worldmap {
class TCreatureHUDController;
class TNumericalEffectStorage;
class TWorldMapGameActionHandler;
class TWorldMapGameActionProvider;
class TWorldMapTutorialMarkerOverlayController;
class TWorldMapViewport;
}
}

namespace tibia {
namespace qmlcomponents {

class TLightmapPaintedItem;
class TNumericalEffectOverlayQuickItem;
class TSpeechBubbleOverlayQuickItem;
class TWorldMapQuickItem;
class TWorldMapRenderProvider;

/// Binds a map pane's items (MapWindowPane's worldmap, lightmap,
/// worldmapCreatureHUDOverlay, speechBubbleOverlay, numericalEffectOverlay and
/// mapWindow, by objectName) to the world provider, and turns the pane's mouse
/// input into game actions.
class TWorldView : public QObject
{
    Q_OBJECT
public:
    /// Owned by `pane`. False from isBound when the pane lacks the world map.
    TWorldView(QQuickItem* pane, TWorldMapRenderProvider* provider, const chat::TTextStorage* texts = nullptr,
               const worldmap::TNumericalEffectStorage* numericalEffects = nullptr);
    ~TWorldView() override;

    bool isBound() const { return m_worldMap != nullptr; }
    /// Unsets the provider everywhere; call before the provider goes away.
    void unbind();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    /// MapWindowPane's mapWindowClicked: mouse in mapWindow pixels (CIP:).
    void onMapWindowClicked(int mouseX, int mouseY, int mouseButton, int keyboardModifiers);
    /// MapWindowPane's mapWindowStartDrag: the first MOVE_OBJECT under the mouse (CIP:).
    void onMapWindowStartDrag(int mouseX, int mouseY, QObject* source);
    /// MapWindowPane's mapWindowDragDropped: the drag moves to the picked field (CIP:).
    void onMapWindowDragDropped(int mouseX, int mouseY);
    /// MapWindowPane's TibiaTargetSelection: publishes the TARGET_SELECTED under the click.
    void onMapWindowTargetSelected(int mouseX, int mouseY);

private:
    QPoint toMapPixel(const QPointF& mapWindowPoint) const;
    /// The TARGET_SELECTED candidate at a mapWindow point, creatures first.
    input::TGameActionPtr targetAt(const QPointF& mapWindowPoint) const;
    /// USE_AT_MOUSECURSOR / SPELL_AT_MOUSE_CURSOR while the mouse is over the
    /// map (CIP: TMapWindowController's handler).
    bool onAtMouseCursor(const input::TGameAction& action);
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

    TWorldMapRenderProvider* m_provider = nullptr;
    std::unique_ptr<worldmap::TWorldMapGameActionProvider> m_actions;
    worldmap::TWorldMapGameActionHandler* m_actionHandler = nullptr;
    worldmap::TWorldMapViewport* m_viewport = nullptr;
    input::TGameActionMapper m_mapper;
    QPointer<QQuickItem> m_mapWindow;
    /// MapWindowPane mouseAreaDrag — FOV grabber (under IdleOT HUD z-order).
    QPointer<QQuickItem> m_mapMouseArea;
    QPointer<QQuickWindow> m_pointerWindow;
    QPointer<TWorldMapQuickItem> m_worldMap;
    QPointer<TLightmapPaintedItem> m_lightMap;
    QPointer<TSpeechBubbleOverlayQuickItem> m_speechBubbles;
    QPointer<TNumericalEffectOverlayQuickItem> m_numericalEffects;
    worldmap::TCreatureHUDController* m_creatureHuds = nullptr;
    worldmap::TWorldMapTutorialMarkerOverlayController* m_tutorialMarkers = nullptr;
    bool m_highlightMouseTarget = true;
    bool m_hovering = false;
    QPointF m_hoverPosition;
    /// CIP +0x140 / +0x150 : frames since an at-mouse action
    /// no handler took; on the second "Invalid target." goes to the status line.
    int m_unhandledAtMouseFrames = -1;

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
