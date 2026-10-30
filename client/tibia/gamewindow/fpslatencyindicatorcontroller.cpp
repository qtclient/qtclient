#include "fpslatencyindicatorcontroller.h"

#include "app/clientoptions.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/nextframetimestamppredictor.h"

#include <tibia/world/game.h>

#include <QQuickWindow>

namespace tibia::gamewindow {

namespace {

constexpr auto ShowFpsOption = QLatin1String("showFpsLagIndicator");
constexpr auto StabilityOption = QLatin1String("connectionStabilityOptimizationEnabled");

QString latencyStateFor(const int pingMs)
{
    if (pingMs < 0)
        return QStringLiteral("measuring");
    if (pingMs < 120)
        return QStringLiteral("low");
    if (pingMs < 250)
        return QStringLiteral("medium");
    return QStringLiteral("high");
}

/// Effective FPS from the tick-to-swap average (CIP FUN_1008f4590).
QString frameCountFromRing(const worldmap::TFrameDurationRing* ring)
{
    if (!ring)
        return QStringLiteral("0");
    const qint64 averageNs = ring->averageNs();
    if (averageNs <= 0)
        return QStringLiteral("0");
    return QString::number((1'000'000'000 + averageNs / 2) / averageNs);
}

}

TFPSLatencyIndicatorController::TFPSLatencyIndicatorController(QObject* parent)
    : cip::tibia::gamewindow::TFPSLatencyIndicatorController(parent)
{
    m_currentFrameRate = QStringLiteral("0");
    m_currentFrameCount = QStringLiteral("0");
    m_currentLatencyState = QStringLiteral("measuring");
    m_visible = app::ClientOptions::load().options().value(ShowFpsOption).toBool(false);
    syncOptimizedConnectionStability();

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &TFPSLatencyIndicatorController::tick);

    input::setActionHandler(input::EGameAction::TOGGLE_FPS_LATENCY_INDICATOR, [this](const input::TGameAction&) {
        toggle();
        return true;
    });

    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == ShowFpsOption)
            setVisibleFromOptions(app::ClientOptions::load().options().value(ShowFpsOption).toBool(false));
        else if (name == StabilityOption)
            syncOptimizedConnectionStability();
    });
}

TFPSLatencyIndicatorController::~TFPSLatencyIndicatorController()
{
    input::setActionHandler(input::EGameAction::TOGGLE_FPS_LATENCY_INDICATOR, nullptr);
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
    const int frames = m_frames.exchange(0);
    const QString fps = QString::number(frames);
    if (fps != m_currentFrameRate) {
        m_currentFrameRate = fps;
        Q_EMIT currentFrameRateChanged();
    }
    const QString frameCount = frameCountFromRing(m_frameDurations);
    if (frameCount != m_currentFrameCount) {
        m_currentFrameCount = frameCount;
        Q_EMIT currentFrameCountChanged();
    }
    const QString buildUs = QString::number(m_provider ? m_provider->buildNs() / 1000 : 0);
    if (buildUs != m_worldmapBuildUs) {
        m_worldmapBuildUs = buildUs;
        Q_EMIT worldmapBuildUsChanged();
    }
    syncLatency();
}

void TFPSLatencyIndicatorController::syncLatency()
{
    const int ping = g_game.isOnline() ? g_game.getPing() : -1;
    if (ping != m_currentLatency) {
        m_currentLatency = ping;
        Q_EMIT currentLatencyChanged();
    }
    const QString state = latencyStateFor(ping);
    if (state != m_currentLatencyState) {
        m_currentLatencyState = state;
        Q_EMIT currentLatencyStateChanged();
    }
}

void TFPSLatencyIndicatorController::syncOptimizedConnectionStability()
{
    const bool on = app::ClientOptions::load().options().value(StabilityOption).toBool(true);
    if (on == m_optimizedConnectionStability)
        return;
    m_optimizedConnectionStability = on;
    Q_EMIT optimizedConnectionStabilityChanged();
}

}
