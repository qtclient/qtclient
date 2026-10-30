#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::gamewindow {

/// MapWindowPane's mapWindowController (CIP: TMapWindowController, metacall
/// 0x10082d6f0): the render options the pane reads, the cursor layer while the
/// mouse is over the map, and the pane's unused width for the sidebars.
class TMapWindowController final : public cip::tibia::gamewindow::TMapWindowController
{
    Q_OBJECT
    /// FOV in fields. Stock CIP MapWindowPane is always 15×11; packs that set
    /// `packExpandsFov` on the pane (IdleOT HUD) get a pane-derived FOV.
    Q_PROPERTY(int fovWidth READ fovWidth NOTIFY fovChanged)
    Q_PROPERTY(int fovHeight READ fovHeight NOTIFY fovChanged)
    /// CIP 15×11 pane (gutters). False is the option for a fuller map window;
    /// stock CIP QML still keeps the 15×11 world — only packs with packExpandsFov
    /// grow the field count. Not tied to fovChanged (resize loop).
    Q_PROPERTY(bool classicView READ classicView NOTIFY classicViewChanged)
    /// CIP map scale: map window height / 352 (pack HUD may use cipScale instead).
    Q_PROPERTY(qreal viewScale READ viewScale NOTIFY fovChanged)
public:
    /// `pane` is the MapWindowPane item.
    TMapWindowController(QQuickItem* pane, bool softwareRenderer, QObject* parent = nullptr);

    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);

    int fovWidth() const { return m_fovWidth; }
    int fovHeight() const { return m_fovHeight; }
    bool classicView() const;
    qreal viewScale() const { return m_viewScale; }

    /// CIP 0x100b6a890: options 0x4e, 0x59, 0x3d and 0x35 (light needs a GPU renderer).
    void onMapWindowControllerOptionsChanged() override;
    /// CIP 0x100b6b7b0.
    void onMapWindowEntered() override;
    /// CIP 0x100b6b7d0: also drops the provider's hover object (v17).
    void onMapWindowExited() override;
    /// CIP 0x100b6b820: re-emits the pane's unusedWidth.
    void onUnusedWidthChanged() override;
    /// Recompute FOV from the map pane size (OTUI MapViewport / visible-sqm).
    void syncFov();
    /// product classic game view (CIP 15×11). Independent of the engine extended-view flag.
    void refreshClassicView();

Q_SIGNALS:
    void fovChanged();
    void classicViewChanged();

private:
    QPointer<QQuickItem> m_pane;
    qmlcomponents::TWorldMapRenderProvider* m_provider = nullptr;
    bool m_softwareRenderer = false;
    int m_fovWidth = 15;
    int m_fovHeight = 11;
    qreal m_viewScale = 1;
    bool m_classicView = false;
};

}
