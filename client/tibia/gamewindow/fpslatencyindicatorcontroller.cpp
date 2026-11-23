#include "fpslatencyindicatorcontroller.h"

#include <QQuickItem>

#include "app/clientoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/nextframetimestamppredictor.h"


#include <QQuickWindow>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

constexpr auto ShowFpsOption = QLatin1String("showFpsLagIndicator");
constexpr auto StabilityOption = QLatin1String("connectionStabilityOptimizationEnabled");

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

TFPSLatencyIndicatorController::TFPSLatencyIndicatorController(QObject* parent)
    : cip::tibia::gamewindow::TFPSLatencyIndicatorController(parent)
{
    m_currentFrameRate = QStringLiteral("0");
    m_currentLatencyState = QStringLiteral("low");
    m_visible = app::ClientOptions::load().options().value(ShowFpsOption).toBool(false);
    syncOptimizedConnectionStability();

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &TFPSLatencyIndicatorController::tick);


    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == ShowFpsOption)
            setVisibleFromOptions(app::ClientOptions::load().options().value(ShowFpsOption).toBool(false));
        else if (name == StabilityOption)
            syncOptimizedConnectionStability();
    });
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
    if (m_visible)
        m_timer.start();
}

void TFPSLatencyIndicatorController::setFrameDurations(worldmap::TFrameDurationRing* ring)
{
    m_frameDurations = ring;
}

void TFPSLatencyIndicatorController::setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider)
{
    m_provider = provider;
}

void TFPSLatencyIndicatorController::toggle()
{
    applyVisible(!m_visible);
    app::ClientOptions::store(ShowFpsOption, m_visible);
}

void TFPSLatencyIndicatorController::setVisibleFromOptions(const bool on)
{
    applyVisible(on);
}

void TFPSLatencyIndicatorController::applyVisible(const bool on)
{
    if (on == m_visible)
        return;
    m_visible = on;
    if (on) {
        m_frames = 0;
        m_timer.start();
        tick();
    } else {
        m_timer.stop();
    }
    Q_EMIT visibilityChanged();
}

void TFPSLatencyIndicatorController::tick()
{
    // The game client's frame values; the rate is the ring-derived double.
    (void)m_frames.exchange(0);
    const double fps = effectiveFpsFromRing(m_frameDurations);
    const int ping = tibia::game::isOnline() ? tibia::game::getPing() : -1;
    update(fps, ping, ping >= 0, m_stabilityOption, static_cast<int>(fps + 0.5));
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

void TFPSLatencyIndicatorController::syncOptimizedConnectionStability()
{
    m_stabilityOption = app::ClientOptions::load().options().value(StabilityOption).toBool(true);
    update(m_frameRate, m_currentLatency, m_latencyMeasured, m_stabilityOption, m_frameCount);
}

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
