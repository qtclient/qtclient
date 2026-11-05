#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace gamewindow {

/// MapWindowPane's mapWindowController (CIP: TMapWindowController, metacall
/// 0x10082d6f0): the render options the pane reads, the cursor layer while the
/// mouse is over the map, and the pane's unused width for the sidebars.
class TMapWindowController final : public cip::tibia::gamewindow::TMapWindowController
{
    Q_OBJECT
    /// FOV in fields. Always CIP 15×11 (HUD and classic share the same world size).
    Q_PROPERTY(int fovWidth READ fovWidth NOTIFY fovChanged)
    Q_PROPERTY(int fovHeight READ fovHeight NOTIFY fovChanged)
    /// Classic CIP chrome (sidebars from unusedWidth). False = IdleOT HUD chrome;
    /// FOV stays 15×11 either way, with grey gutters filling leftover pane.
    Q_PROPERTY(bool classicView READ classicView NOTIFY classicViewChanged)
    /// CIP map scale: map window height / 352.
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
    /// Keep FOV at CIP 15×11 and refresh viewScale from the map window height.
    void syncFov();
    /// Classic vs IdleOT HUD chrome layout (FOV unchanged).
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
}
