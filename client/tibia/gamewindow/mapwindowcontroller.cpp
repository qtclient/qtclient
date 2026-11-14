#include "mapwindowcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/mousecursorcontroller.h"
#include "renderer/worldmaprenderprovider.h"

#include <QQuickItem>

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
                if (name == QLatin1String("modClassicGameView"))
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
    const bool classic = options.value(QLatin1String("modClassicGameView")).toBool(false);
    if (classic == m_classicView)
        return;
    m_classicView = classic;
    Q_EMIT classicViewChanged();
    syncFov();
}

void TMapWindowController::syncFov()
{
    if (!m_pane)
        return;
    QQuickItem* mapWindow = m_pane->findChild<QQuickItem*>(QStringLiteral("mapWindow"));
    const qreal paneH = mapWindow ? mapWindow->height() : m_pane->height();
    if (paneH < 32)
        return;

    // CIP MapWindowPane is always a 15×11 world (480×352) scaled into the map
    // window; unusedWidth beside the 15:11 frame feeds sidebars / grey gutters.
    constexpr int cipFieldsW = 15;
    constexpr int cipFieldsH = 11;
    // Matches CIP MapWindowPane.mapWindowScaleFactor (height / 352).
    const qreal scale = paneH / (cipFieldsH * 32.0);
    const bool changed = cipFieldsW != m_fovWidth || cipFieldsH != m_fovHeight
        || !qFuzzyCompare(scale, m_viewScale);
    m_fovWidth = cipFieldsW;
    m_fovHeight = cipFieldsH;
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
        m_provider->setMouseTargetObject(0, {}, -1);
}

void TMapWindowController::onUnusedWidthChanged()
{
    if (m_pane)
        Q_EMIT unusedWidthChanged(m_pane->property("unusedWidth").toInt());
}

}
