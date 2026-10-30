#include "mapwindowcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/mousecursorcontroller.h"
#include "renderer/worldmaprenderprovider.h"

#include <QQuickItem>

#include <cmath>

namespace tibia::gamewindow {

TMapWindowController::TMapWindowController(QQuickItem* pane, const bool softwareRenderer, QObject* parent)
    : cip::tibia::gamewindow::TMapWindowController(parent)
    , m_pane(pane)
    , m_softwareRenderer(softwareRenderer)
{
    refreshClassicView();
    onMapWindowControllerOptionsChanged();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            &TMapWindowController::onMapWindowControllerOptionsChanged);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            [this](const QString& name) {
                if (name == QLatin1String("modClassicGameView")
                    || name == QLatin1String("modFullscreenGameView"))
                    refreshClassicView();
            });
    if (pane) {
        connect(pane, SIGNAL(unusedWidthChanged()), this, SLOT(onUnusedWidthChanged()));
        // mapWindow is the inner clip item whose size is the live game FOV pane.
        if (QQuickItem* mapWindow = pane->findChild<QQuickItem*>(QStringLiteral("mapWindow"))) {
            connect(mapWindow, &QQuickItem::widthChanged, this, &TMapWindowController::syncFov);
            connect(mapWindow, &QQuickItem::heightChanged, this, &TMapWindowController::syncFov);
        }
        connect(pane, &QQuickItem::widthChanged, this, &TMapWindowController::syncFov);
        connect(pane, &QQuickItem::heightChanged, this, &TMapWindowController::syncFov);
    }
}

void TMapWindowController::setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider)
{
    m_provider = provider;
    syncFov();
}

bool TMapWindowController::classicView() const
{
    return m_classicView;
}

void TMapWindowController::refreshClassicView()
{
    const QJsonObject options = app::ClientOptions::load().options();
    bool classic = false;
    if (options.contains(QLatin1String("modClassicGameView")))
        classic = options.value(QLatin1String("modClassicGameView")).toBool(false);
    else
        classic = !options.value(QLatin1String("modFullscreenGameView")).toBool(true);
    if (classic == m_classicView)
        return;
    m_classicView = classic;
    Q_EMIT classicViewChanged();
    syncFov();
}

namespace {

/// CIP MapWindowPane: scale is the snapped map height over the 15×11 reference (352).
double cipScale(const double paneW, const double paneH, const bool evenMultiples)
{
    constexpr double unscaled = 11 * 32;
    constexpr double aspect = 15.0 / 11.0;
    double maxH = std::floor(paneW / aspect);
    if (evenMultiples) {
        maxH = std::min(paneH, maxH);
        maxH = std::floor(maxH / unscaled) * unscaled;
    }
    maxH = std::max(unscaled / 2, maxH);
    double desired = paneH;
    const double percent = desired / unscaled;
    const double closest = std::round(percent);
    if (std::abs(percent - closest) <= 0.05)
        desired = closest * unscaled;
    const double height = std::max(unscaled / 2, std::min(desired, maxH));
    return height / unscaled;
}

}

void TMapWindowController::syncFov()
{
    if (!m_pane)
        return;
    QQuickItem* mapWindow = m_pane->findChild<QQuickItem*>(QStringLiteral("mapWindow"));
    const qreal paneW = mapWindow ? mapWindow->width() : m_pane->width();
    const qreal paneH = mapWindow ? mapWindow->height() : m_pane->height();
    if (paneW < 32 || paneH < 32)
        return;

    int width = 15;
    int height = 11;
    qreal scale = paneH / (11 * 32.0);
    if (classicView()) {
        // The pane was already sized by CIP's height snap. The world stays 15×11.
        scale = paneH / (11 * 32.0);
    } else {
        scale = cipScale(paneW, paneH, m_mapWindowScaleOnlyByEvenMultiples);
        width = std::max(3, static_cast<int>(std::ceil(paneW / (32.0 * scale))));
        height = std::max(3, static_cast<int>(std::ceil(paneH / (32.0 * scale))));
        // CIP's classic FOV is always odd (15×11). An even field count puts the
        // local player a half-tile off the cover centre, so screen-centred look /
        // status text (and the geometric mid of the pane) drift from the avatar.
        if ((width & 1) == 0)
            ++width;
        if ((height & 1) == 0)
            ++height;
    }
    const bool changed = width != m_fovWidth || height != m_fovHeight || !qFuzzyCompare(scale, m_viewScale);
    m_fovWidth = width;
    m_fovHeight = height;
    m_viewScale = scale;
    if (m_provider) {
        m_provider->setVisibleFields(m_fovWidth, m_fovHeight);
        m_provider->setViewScale(m_viewScale);
    }
    if (changed)
        Q_EMIT fovChanged();
}

void TMapWindowController::onMapWindowControllerOptionsChanged()
{
    const QJsonObject options = app::ClientOptions::load().options();
    const bool showNumericalEffects = options.value(QLatin1String("gameWindowShowTextualEffects")).toBool(true);
    const auto antialiasingMode = static_cast<cip::tibia::qmlcomponents::enums::EAntialiasingMode>(
        options.value(QLatin1String("antialiasingMode")).toInt());
    const bool scaleOnlyByEvenMultiples = options.value(QLatin1String("gameWindowScaleOnlyByEvenMultiples")).toBool();
    const bool useLightMap = options.value(QLatin1String("lightEffectsEnabled")).toBool(true) && !m_softwareRenderer;
    if (showNumericalEffects == m_showNumericalEffects && antialiasingMode == m_antialiasingMode
        && scaleOnlyByEvenMultiples == m_mapWindowScaleOnlyByEvenMultiples && useLightMap == m_useLightMap)
        return;
    m_showNumericalEffects = showNumericalEffects;
    m_antialiasingMode = antialiasingMode;
    m_mapWindowScaleOnlyByEvenMultiples = scaleOnlyByEvenMultiples;
    m_useLightMap = useLightMap;
    Q_EMIT renderOptionsChanged();
    syncFov();
}

void TMapWindowController::onMapWindowEntered()
{
    if (auto* cursor = TMouseCursorController::instance())
        cursor->onMapWindowEntered();
}

void TMapWindowController::onMapWindowExited()
{
    if (auto* cursor = TMouseCursorController::instance())
        cursor->onMapWindowExited();
    if (m_provider)
        m_provider->setMouseTarget(nullptr);
}

void TMapWindowController::onUnusedWidthChanged()
{
    if (m_pane)
        Q_EMIT unusedWidthChanged(m_pane->property("unusedWidth").toInt());
}

}
