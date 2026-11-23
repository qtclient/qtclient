#pragma once

class QQuickItem;

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <atomic>

class QQuickWindow;

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}
namespace tibia {
namespace worldmap {
class TFrameDurationRing;
}
}

namespace tibia {
namespace gamewindow {

/// FpsLatencyIndicator.qml (CIP TFPSLatencyIndicatorController):
/// currentFrameRate = TFrameDurationRing effective FPS as QString::number(d,'f',2)
/// ; ping from tibia::game::getPing; optimizedConnectionStability
/// from connectionStabilityOptimizationEnabled; visibility from showFpsLagIndicator /
/// TOGGLE_FPS_LATENCY_INDICATOR (Alt+F8).
/// product also exposes worldmapBuildUs for the override HUD.
class TFPSLatencyIndicatorController final : public cip::tibia::gamewindow::TFPSLatencyIndicatorController
{
    Q_OBJECT
    Q_PROPERTY(QString worldmapBuildUs READ worldmapBuildUs NOTIFY worldmapBuildUsChanged FINAL)
public:
    /// CIP: the indicator item shows this controller; a destroyed item lets go of it.
    void setWidgetItem(QQuickItem* item);
    explicit TFPSLatencyIndicatorController(QObject* parent = nullptr);
    ~TFPSLatencyIndicatorController() override;

    void setWindow(QQuickWindow* window);
    /// Optional: game client's tick-to-swap ring for currentFrameCount.
    void setFrameDurations(worldmap::TFrameDurationRing* ring);
    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);
    QString worldmapBuildUs() const { return m_worldmapBuildUs; }
    void toggle();
    void setVisibleFromOptions(bool on);
    /// CIP: the game client's values before a frame renders; each property
    /// signals when it changed (the frame count silently).
    void update(double frameRate, qint64 latencyMs, bool latencyMeasured, bool optimizedConnectionStability, int frameCount);
    QString currentFrameCount() const override;

Q_SIGNALS:
    void worldmapBuildUsChanged();

private:
    /// CIP: the item lets go of this controller.
    void detachWidgetItem();
    /// CIP: the destroyed item is forgotten.
    void onWidgetQuickItemDestroyed();
    QPointer<QQuickItem> m_widgetItem;
    void applyVisible(bool on);
    void tick();
    void syncOptimizedConnectionStability();

    QPointer<QQuickWindow> m_window;
    worldmap::TFrameDurationRing* m_frameDurations = nullptr;
    qmlcomponents::TWorldMapRenderProvider* m_provider = nullptr;
    QMetaObject::Connection m_frameConnection;
    QTimer m_timer;
    std::atomic<int> m_frames { 0 };
    QString m_worldmapBuildUs { QStringLiteral("0") };
    double m_frameRate = 0.0;
    bool m_stabilityOption = false;
    bool m_latencyMeasured = true;
    /// The frame count of the last update.
    int m_frameCount = 0;
};

}
}
