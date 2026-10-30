#pragma once

#include "input/gameactionmapper.h"

#include <QObject>
#include <QPointF>
#include <QPointer>

#include <memory>

class QQuickItem;

namespace tibia::chat {
class TTextStorage;
}
namespace tibia::worldmap {
class TCreatureHUDController;
class TNumericalEffectStorage;
class TWorldMapGameActionHandler;
class TWorldMapGameActionProvider;
class TWorldMapTutorialMarkerOverlayController;
class TWorldMapViewport;
}

namespace tibia::qmlcomponents {

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
    /// Owned by `pane`. False from isBound() when the pane lacks the world map.
    TWorldView(QQuickItem* pane, TWorldMapRenderProvider* provider, const chat::TTextStorage* texts = nullptr,
               const worldmap::TNumericalEffectStorage* numericalEffects = nullptr);
    ~TWorldView() override;

    bool isBound() const { return m_worldMap != nullptr; }
    /// Unsets the provider everywhere; call before the provider goes away.
    void unbind();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    /// MapWindowPane's mapWindowClicked: mouse in mapWindow pixels (CIP: FUN_100b6dc70).
    void onMapWindowClicked(int mouseX, int mouseY, int mouseButton, int keyboardModifiers);
    /// MapWindowPane's mapWindowStartDrag: the first MOVE_OBJECT under the mouse (CIP: FUN_100b6e020).
    void onMapWindowStartDrag(int mouseX, int mouseY, QObject* source);
    /// MapWindowPane's mapWindowDragDropped: the drag moves to the picked field (CIP: FUN_100b6e3e0).
    void onMapWindowDragDropped(int mouseX, int mouseY);
    /// MapWindowPane's TibiaTargetSelection: publishes the TARGET_SELECTED under the click.
    void onMapWindowTargetSelected(int mouseX, int mouseY);

private:
    QPoint toMapPixel(const QPointF& mapWindowPoint) const;
    /// The TARGET_SELECTED candidate at a mapWindow point, creatures first.
    input::TGameActionPtr targetAt(const QPointF& mapWindowPoint) const;
    /// USE_AT_MOUSECURSOR / SPELL_AT_MOUSE_CURSOR while the mouse is over the
    /// map (CIP: TMapWindowController's handler FUN_100b6b8b0).
    bool onAtMouseCursor(const input::TGameAction& action);
    /// The hover's cursor shape and highlighted thing, once per frame.
    void updateMouseTarget();

    TWorldMapRenderProvider* m_provider = nullptr;
    std::unique_ptr<worldmap::TWorldMapGameActionProvider> m_actions;
    worldmap::TWorldMapGameActionHandler* m_actionHandler = nullptr;
    worldmap::TWorldMapViewport* m_viewport = nullptr;
    input::TGameActionMapper m_mapper;
    QPointer<QQuickItem> m_mapWindow;
    QPointer<TWorldMapQuickItem> m_worldMap;
    QPointer<TLightmapPaintedItem> m_lightMap;
    QPointer<TSpeechBubbleOverlayQuickItem> m_speechBubbles;
    QPointer<TNumericalEffectOverlayQuickItem> m_numericalEffects;
    worldmap::TCreatureHUDController* m_creatureHuds = nullptr;
    worldmap::TWorldMapTutorialMarkerOverlayController* m_tutorialMarkers = nullptr;
    bool m_highlightMouseTarget = true;
    bool m_animatedCursor = true;
    bool m_hovering = false;
    QPointF m_hoverPosition;
    /// CIP +0x140 / +0x150 (FUN_100b6cfd0): frames since an at-mouse action
    /// no handler took; on the second "Invalid target." goes to the status line.
    int m_unhandledAtMouseFrames = -1;
};

}
