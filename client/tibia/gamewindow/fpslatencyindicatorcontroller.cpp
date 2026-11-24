#include "fpslatencyindicatorcontroller.h"
#include "game/currentgamesession.h"
#include "network/networkqualityprotocolmessagehandler.h"
#include "game/gameservergamesession.h"

#include <QQuickItem>

#include "config/clientoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/nextframetimestamppredictor.h"


#include <QQuickWindow>

namespace tibia::gamewindow {

namespace {

/// The latency band QML colours the ping by: "measuring" until a ping
/// is known, then low below 200 ms, medium below 500 ms, else high.
QString latencyStateFor(const qint64 latencyMs, const bool measured)
{
    if (!measured)
        return QStringLiteral("measuring");
    if (latencyMs < 200)
        return QStringLiteral("low");
    if (latencyMs < 500)
        return QStringLiteral("medium");
    return QStringLiteral("high");
}

/// Effective FPS from the tick-to-swap average (CipSoft).
double effectiveFpsFromRing(const worldmap::TFrameDurationRing* ring)
{
    if (!ring)
        return 0.0;
    const qint64 averageNs = ring->averageNs();
    if (averageNs <= 0)
        return 0.0;
    return 1'000'000'000.0 / static_cast<double>(averageNs);
}

/// CipSoft TFPSLatencyIndicatorController getter : QString::number(d, 'f', 2).
QString formatFps(const double fps)
{
    return QString::number(fps, 'f', 2);
}

}

TFPSLatencyIndicatorController::TFPSLatencyIndicatorController(config::TClientOptions* ClientOptions, QObject* parent)
    : cip::tibia::gamewindow::TFPSLatencyIndicatorController(parent)
    , m_pClientOptions(ClientOptions)
{
    m_currentFrameRate = QStringLiteral("0");
    m_currentLatencyState = QStringLiteral("low");
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &TFPSLatencyIndicatorController::tick);
    connect(this, &TFPSLatencyIndicatorController::visibilityChanged, this, &TFPSLatencyIndicatorController::onVisibilityChanged);
    if (m_pClientOptions)
        connect(m_pClientOptions, &config::TClientOptions::fpsLagIndicatorOptionsChanged, this,
                &TFPSLatencyIndicatorController::visibilityChanged);
    Q_EMIT visibilityChanged();
}

TFPSLatencyIndicatorController::~TFPSLatencyIndicatorController()
{
    disconnect(m_frameConnection);
}

void TFPSLatencyIndicatorController::setWindow(QQuickWindow* window)
{
    disconnect(m_frameConnection);
    m_window = window;
    if (window)
        m_frameConnection = connect(window, &QQuickWindow::frameSwapped, this, [this] { ++m_frames; }, Qt::DirectConnection);
    onVisibilityChanged();
}

void TFPSLatencyIndicatorController::setFrameDurations(worldmap::TFrameDurationRing* ring)
{
    m_frameDurations = ring;
}

void TFPSLatencyIndicatorController::setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider)
{
    m_provider = provider;
}

bool TFPSLatencyIndicatorController::visible() const
{
    return m_pClientOptions && m_pClientOptions->boolOption(config::EClientOption::showFpsLagIndicator);
}

void TFPSLatencyIndicatorController::onVisibilityChanged()
{
    if (!visible()) {
        m_timer.stop();
        return;
    }
    if (m_timer.isActive())
        return;
    m_frames = 0;
    m_timer.start();
    tick();
}

void TFPSLatencyIndicatorController::tick()
{
    // The game client's frame values; the rate is the ring-derived double.
    (void)m_frames.exchange(0);
    const double fps = effectiveFpsFromRing(m_frameDurations);
    // The live game server session's round trip; -1 without one (a replay
    // measures nothing).
    const auto* session = dynamic_cast<const game::TGameserverGameSession*>(tibia::game::currentGameSession());
    const int ping = session && session->isOnline() ? int(session->latency().milliseconds) : -1;
    const bool optimizedConnectionStability =
        m_pClientOptions && m_pClientOptions->boolOption(config::EClientOption::connectionStabilityOptimizationEnabled);
    update(fps, ping, ping >= 0, optimizedConnectionStability, static_cast<int>(fps + 0.5));
    const QString buildUs = QString::number(m_provider ? m_provider->buildNs() / 1000 : 0);
    if (buildUs != m_worldmapBuildUs) {
        m_worldmapBuildUs = buildUs;
        Q_EMIT worldmapBuildUsChanged();
    }
}

void TFPSLatencyIndicatorController::update(const double frameRate, const qint64 latencyMs, const bool latencyMeasured,
                                            const bool optimizedConnectionStability, const int frameCount)
{
    m_frameCount = frameCount;
    if (m_frameRate != frameRate) {
        m_frameRate = frameRate;
        m_currentFrameRate = formatFps(frameRate);
        Q_EMIT currentFrameRateChanged();
    }
    if (m_currentLatency != latencyMs) {
        m_currentLatency = static_cast<int>(latencyMs);
        Q_EMIT currentLatencyChanged();
    }
    m_latencyMeasured = latencyMeasured;
    const QString state = latencyStateFor(latencyMs, latencyMeasured);
    if (state != m_currentLatencyState) {
        m_currentLatencyState = state;
        Q_EMIT currentLatencyStateChanged();
    }
    if (m_optimizedConnectionStability != optimizedConnectionStability) {
        m_optimizedConnectionStability = optimizedConnectionStability;
        Q_EMIT optimizedConnectionStabilityChanged();
    }
}

/// The frame count is formatted when it is read.
QString TFPSLatencyIndicatorController::currentFrameCount() const { return QString::number(m_frameCount); }

void TFPSLatencyIndicatorController::setWidgetItem(QQuickItem* item)
{
    detachWidgetItem();
    m_widgetItem = item;
    if (!item)
        return;
    item->setProperty("controller", QVariant::fromValue<QObject*>(this));
    connect(item, &QObject::destroyed, this, &TFPSLatencyIndicatorController::onWidgetQuickItemDestroyed);
}

void TFPSLatencyIndicatorController::detachWidgetItem()
{
    if (!m_widgetItem)
        return;
    disconnect(m_widgetItem, &QObject::destroyed, this, &TFPSLatencyIndicatorController::onWidgetQuickItemDestroyed);
    m_widgetItem->setProperty("controller", QVariant::fromValue<QObject*>(nullptr));
    m_widgetItem = nullptr;
}

void TFPSLatencyIndicatorController::onWidgetQuickItemDestroyed()
{
    m_widgetItem = nullptr;
}

}
