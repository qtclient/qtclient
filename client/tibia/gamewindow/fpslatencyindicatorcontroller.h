#pragma once

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <atomic>

class QQuickWindow;

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}
namespace tibia::worldmap {
class TFrameDurationRing;
}

namespace tibia::gamewindow {

/// FpsLatencyIndicator.qml (CIP TFPSLatencyIndicatorController): FPS from
/// frameSwapped, effective FPS from TFrameDurationRing (currentFrameCount),
/// ping from g_game, optimizedConnectionStability from
/// connectionStabilityOptimizationEnabled, visibility from showFpsLagIndicator /
/// TOGGLE_FPS_LATENCY_INDICATOR (Alt+F8).
/// product also exposes worldmapBuildUs for the override HUD.
class TFPSLatencyIndicatorController final : public cip::tibia::gamewindow::TFPSLatencyIndicatorController
{
    Q_OBJECT
    Q_PROPERTY(QString worldmapBuildUs READ worldmapBuildUs NOTIFY worldmapBuildUsChanged FINAL)
public:
    explicit TFPSLatencyIndicatorController(QObject* parent = nullptr);
    ~TFPSLatencyIndicatorController() override;

    void setWindow(QQuickWindow* window);
    /// Optional: game client's tick-to-swap ring for currentFrameCount.
    void setFrameDurations(worldmap::TFrameDurationRing* ring);
    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);
    QString worldmapBuildUs() const { return m_worldmapBuildUs; }
    void toggle();
    void setVisibleFromOptions(bool on);

Q_SIGNALS:
    void worldmapBuildUsChanged();

private:
    void applyVisible(bool on);
    void tick();
    void syncLatency();
    void syncOptimizedConnectionStability();

    QPointer<QQuickWindow> m_window;
    worldmap::TFrameDurationRing* m_frameDurations = nullptr;
    qmlcomponents::TWorldMapRenderProvider* m_provider = nullptr;
    QMetaObject::Connection m_frameConnection;
    QTimer m_timer;
    std::atomic<int> m_frames { 0 };
    QString m_worldmapBuildUs { QStringLiteral("0") };
};

}
