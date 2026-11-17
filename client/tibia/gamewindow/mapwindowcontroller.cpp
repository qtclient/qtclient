#include "mapwindowcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/mousecursorcontroller.h"
#include "renderer/worldmaprenderprovider.h"
#include <QQuickItem>
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/draganddropcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/numericaleffectoverlay.h"
#include "qmlcomponents/speechbubbleoverlay.h"
#include "renderer/lightmappainteditem.h"
#include "renderer/mapscale.h"
#include "renderer/worldmapquickitem.h"
#include "worldmap/creaturehudcontroller.h"
#include "worldmap/worldmapgameactionhandler.h"
#include "worldmap/worldmapgameactionprovider.h"
#include "worldmap/worldmappicker.h"
#include "worldmap/worldmaptutorialmarkeroverlaycontroller.h"
#include "worldmap/worldmapviewport.h"
#include "assets/thingtype.h"
#include <QCursor>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QTouchEvent>
#include <cmath>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using Cursor = TMouseCursorController;
using qmlcomponents::TWorldMapRenderProvider;

}

TMapWindowController::TMapWindowController(QQuickItem* pane, const bool softwareRenderer, QObject* parent)
    : cip::tibia::gamewindow::TMapWindowController(parent)
    , m_pane(pane)
    , m_softwareRenderer(softwareRenderer)
    , m_actionHandler(new worldmap::TWorldMapGameActionHandler(this))
{
    // The pane knows its controller; its items are found by objectName.
    if (pane) {
        pane->setProperty("mapWindowController", QVariant::fromValue<QObject*>(this));
        m_mapWindow = pane->findChild<QQuickItem*>(QStringLiteral("mapWindow"));
        m_worldMap = pane->findChild<qmlcomponents::TWorldMapQuickItem*>(QStringLiteral("worldmap"));
        m_numericalEffects = pane->findChild<qmlcomponents::TNumericalEffectOverlayQuickItem*>(QStringLiteral("numericalEffectOverlay"));
        m_speechBubbles = pane->findChild<qmlcomponents::TSpeechBubbleOverlayQuickItem*>(QStringLiteral("speechBubbleOverlay"));
        m_lightMap = pane->findChild<qmlcomponents::TLightmapPaintedItem*>(QStringLiteral("lightmap"));
    }

    const QJsonObject options = app::ClientOptions::load().options();
    m_highlightMouseTarget = options.value(QLatin1String("mouseHighlightTarget")).toBool(true);
    m_mapper.setControlScheme(options.value(QLatin1String("controlSchemeIndex")).toInt(0));
    // CIP default lootSchemeIndex is 0 (Loot: Right); match clientoptiondefaults.
    m_mapper.setLootScheme(options.value(QLatin1String("lootSchemeIndex")).toInt(0));
    refreshClassicView();
    onMapWindowControllerOptionsChanged();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            &TMapWindowController::onMapWindowControllerOptionsChanged);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        const QJsonObject options = app::ClientOptions::load().options();
        if (name == QLatin1String("modImmersiveGameView") || name == QLatin1String("modClassicGameView"))
            refreshClassicView();
        else if (name == QLatin1String("mouseHighlightTarget"))
            m_highlightMouseTarget = options.value(name).toBool(true);
        else if (name == QLatin1String("controlSchemeIndex"))
            m_mapper.setControlScheme(options.value(name).toInt(0));
        else if (name == QLatin1String("lootSchemeIndex"))
            m_mapper.setLootScheme(options.value(name).toInt(0));
        else if (name == QLatin1String("gameWindowShowAttackAnimation") || name == QLatin1String("gameWindowShowTargetHighlight"))
            applyTargetHighlightOption();
    });

    if (m_mapWindow) {
        connect(m_mapWindow, SIGNAL(mapWindowClicked(int, int, int, int)), this, SLOT(onMapWindowClicked(int, int, int, int)));
        connect(m_mapWindow, SIGNAL(mapWindowStartDrag(int, int, QObject*)), this, SLOT(onMapWindowStartDrag(int, int, QObject*)));
        connect(m_mapWindow, SIGNAL(mapWindowDragDropped(int, int)), this, SLOT(onMapWindowDragDropped(int, int)));
        connect(m_mapWindow, SIGNAL(mapWindowTargetSelected(int, int)), this, SLOT(onMapWindowTargetSelected(int, int)));
        m_mapMouseArea = m_mapWindow->findChild<QQuickItem*>(QStringLiteral("mapWindowMouseArea"));
        if (!m_mapMouseArea) {
            // Upstream MapWindowPane has no objectName on mouseAreaDrag.
            for (QQuickItem* child : m_mapWindow->childItems()) {
                if (child->inherits("QQuickMouseArea")) {
                    m_mapMouseArea = child;
                    break;
                }
            }
        }
        if (m_mapMouseArea)
            m_mapMouseArea->installEventFilter(this);
        // HUD chrome is a sibling of the FOV frame (above mouseAreaDrag). Bridge
        // press/drag/click from the window so tiles under the action bar still work.
        connect(m_mapWindow, &QQuickItem::windowChanged, this, [this](QQuickWindow*) { ensureWindowPointerFilter(); });
        ensureWindowPointerFilter();
        // mapWindow is the inner clip item whose size is the live game FOV pane.
        connect(m_mapWindow, &QQuickItem::widthChanged, this, &TMapWindowController::syncFov);
        connect(m_mapWindow, &QQuickItem::heightChanged, this, &TMapWindowController::syncFov);
    }
    if (pane) {
        connect(pane, SIGNAL(unusedWidthChanged()), this, SLOT(onUnusedWidthChanged()));
        connect(pane, &QQuickItem::widthChanged, this, &TMapWindowController::syncFov);
        connect(pane, &QQuickItem::heightChanged, this, &TMapWindowController::syncFov);
    }
    for (const input::EGameAction type : { input::EGameAction::USE_AT_MOUSECURSOR, input::EGameAction::SPELL_AT_MOUSE_CURSOR })
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleGameAction(action); });
}

TMapWindowController::~TMapWindowController()
{
    for (const input::EGameAction type : { input::EGameAction::USE_AT_MOUSECURSOR, input::EGameAction::SPELL_AT_MOUSE_CURSOR })
        input::setActionHandler(type, nullptr);
    unbind();
}

void TMapWindowController::applyTargetHighlightOption()
{
    if (!m_provider)
        return;
    // CIP 0x4c ShowTargetHighlight + 0x4d ShowAttackAnimation both gate the
    // red/green creature tint (options.json has both keys).
    const QJsonObject options = app::ClientOptions::load().options();
    m_provider->setTargetHighlightEnabled(options.value(QLatin1String("gameWindowShowAttackAnimation")).toBool(true)
                                          || options.value(QLatin1String("gameWindowShowTargetHighlight")).toBool(true));
}

void TMapWindowController::bindSession(const chat::TTextStorage* texts, const effects::TEffectStorage* effects)
{
    if (!m_provider)
        return;
    if (!m_bound) {
        m_bound = true;
        // The session's view the map handler centres; a provider without one gets a view of its own.
        m_viewport = m_provider->viewport() ? m_provider->viewport() : new worldmap::TWorldMapViewport(this);
        m_actions = std::make_unique<worldmap::TWorldMapGameActionProvider>(*m_provider, *m_viewport);
        applyTargetHighlightOption();
        // The drawn volume is sized for the visible fields.
        const auto sizeViewport = [this] {
            if (m_viewport && m_provider)
                m_viewport->setSize(m_provider->visibleWidth() * TWorldMapRenderProvider::FieldSize,
                                    m_provider->visibleHeight() * TWorldMapRenderProvider::FieldSize);
        };
        sizeViewport();
        connect(m_provider, &TWorldMapRenderProvider::visibleFieldsChanged, this, sizeViewport);
        if (m_worldMap)
            m_worldMap->setRenderProvider(m_provider);
        if (m_lightMap)
            m_lightMap->setRenderProvider(m_provider);
        if (m_pane) {
            if (auto* overlay = m_pane->findChild<QQuickItem*>(QStringLiteral("worldmapCreatureHUDOverlay")))
                m_creatureHuds = new worldmap::TCreatureHUDController(overlay, m_provider, this);
            if (auto* tutorialOverlay = m_pane->findChild<QQuickItem*>(QStringLiteral("tutorialMarkerOverlay"))) {
                m_tutorialMarkers = new worldmap::TWorldMapTutorialMarkerOverlayController(tutorialOverlay, this);
                m_tutorialMarkers->setRenderProvider(m_provider);
            }
        }
        // The world moves under a resting mouse.
        connect(m_provider, &TWorldMapRenderProvider::overlaysAdvanced, this, &TMapWindowController::updateMouseTarget);
        connect(m_provider, &TWorldMapRenderProvider::overlaysAdvanced, this, &TMapWindowController::updateTrackedUseAtMouseCursor);
    }
    if (worldmap::TWorldMapViewport* viewport = m_provider->viewport(); viewport && viewport != m_viewport) {
        m_viewport = viewport;
        m_actions = std::make_unique<worldmap::TWorldMapGameActionProvider>(*m_provider, *m_viewport);
        m_viewport->setSize(m_provider->visibleWidth() * TWorldMapRenderProvider::FieldSize,
                            m_provider->visibleHeight() * TWorldMapRenderProvider::FieldSize);
    }
    if (m_speechBubbles)
        m_speechBubbles->setSources(texts ? m_provider : nullptr, texts);
    if (m_numericalEffects)
        m_numericalEffects->setSources(effects ? m_provider : nullptr, effects);
}

void TMapWindowController::updateTrackedUseAtMouseCursor()
{
    // A use at the mouse no handler took: on the second frame the status line says so.
    if (m_unhandledAtMouseFrames < 0 || ++m_unhandledAtMouseFrames < 2)
        return;
    clearTrackedUseAtMouseCursorGameAction();
    auto status = input::mutableClone(*input::makeAction(input::EGameAction::SET_STATUS_LINE));
    status->text = qtTrId("actionbar_action_no_target");
    input::publish(status);
}

void TMapWindowController::trackUseAtMouseCursorGameAction()
{
    m_unhandledAtMouseFrames = 0;
}

void TMapWindowController::clearTrackedUseAtMouseCursorGameAction()
{
    m_unhandledAtMouseFrames = -1;
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
    bool immersive = true;
    if (options.contains(QLatin1String("modImmersiveGameView")))
        immersive = options.value(QLatin1String("modImmersiveGameView")).toBool(true);
    else if (options.contains(QLatin1String("modClassicGameView")))
        immersive = !options.value(QLatin1String("modClassicGameView")).toBool(false);
    const bool classic = !immersive;
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

void TMapWindowController::unbind()
{
    if (m_pointerWindow) {
        m_pointerWindow->removeEventFilter(this);
        m_pointerWindow.clear();
    }
    m_mapPressActive = false;
    m_mapDragStarted = false;
    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
        m_provider->setMouseTargetObject(0, {}, -1);
        m_provider->setMouseHighlightField({});
    }
    m_provider = nullptr;
    m_actions.reset();
    if (m_worldMap)
        m_worldMap->setRenderProvider(nullptr);
    if (m_lightMap)
        m_lightMap->setRenderProvider(nullptr);
    if (m_creatureHuds)
        m_creatureHuds->setRenderProvider(nullptr);
    if (m_speechBubbles)
        m_speechBubbles->setSources(nullptr, nullptr);
    if (m_numericalEffects)
        m_numericalEffects->setSources(nullptr, nullptr);
}

void TMapWindowController::ensureWindowPointerFilter()
{
    QQuickWindow* window = m_mapWindow ? m_mapWindow->window() : nullptr;
    if (m_pointerWindow == window)
        return;
    if (m_pointerWindow)
        m_pointerWindow->removeEventFilter(this);
    m_pointerWindow = window;
    if (m_pointerWindow)
        m_pointerWindow->installEventFilter(this);
}

QQuickItem* TMapWindowController::leafItemAtScenePos(const QPointF& scenePos) const
{
    if (!m_pointerWindow || !m_pointerWindow->contentItem())
        return nullptr;
    const auto walk = [&](auto&& self, QQuickItem* root) -> QQuickItem* {
        if (!root || !root->isVisible() || !root->isEnabled())
            return nullptr;
        const QPointF local = root->mapFromScene(scenePos);
        if (!root->contains(local))
            return nullptr;
        const QList<QQuickItem*> kids = root->childItems();
        for (auto it = kids.crbegin(); it != kids.crend(); ++it) {
            if (QQuickItem* hit = self(self, *it))
                return hit;
        }
        // Full-bleed pack roots (IdleotHud) set passMapPointer so empty space
        // falls through to the FOV MouseArea instead of owning the leaf.
        if (root->property("passMapPointer").toBool()
            && root->acceptedMouseButtons() == Qt::NoButton
            && !root->inherits("QQuickMouseArea")
            && !root->inherits("QQuickAbstractButton"))
            return nullptr;
        return root;
    };
    return walk(walk, m_pointerWindow->contentItem());
}

bool TMapWindowController::isInteractiveOverlay(const QQuickItem* leaf) const
{
    if (!leaf)
        return false;
    // FOV MouseArea subtree is real map input — never treat as overlay chrome.
    for (const QQuickItem* p = leaf; p; p = p->parentItem()) {
        if (p == m_mapMouseArea)
            return false;
    }
    // Pack HUD (TeamHud, AutomationBar, TopBar, …) is a sibling of the FOV
    // frame under mapWindow. Treating every mapWindow child as non-interactive
    // used to synthesize a walk on HUD button release. Walk ancestors for a
    // real control; absorbMapClicks marks opaque chrome (HudClickSink hosts).
    bool underMapWindow = false;
    for (const QQuickItem* p = leaf; p; p = p->parentItem()) {
        if (p == m_mapWindow) {
            underMapWindow = true;
            break;
        }
        if (p->property("absorbMapClicks").toBool() || p->property("absorbClicks").toBool())
            return true;
        if (p->acceptedMouseButtons() != Qt::NoButton)
            return true;
        if (p->inherits("QQuickMouseArea") || p->inherits("QQuickAbstractButton"))
            return true;
    }
    if (underMapWindow)
        return false;
    // Outside the FOV frame: any hit that survived passMapPointer is chrome
    // (or pane chrome) — never synthesize a walk through it.
    return true;
}

void TMapWindowController::ungrabOverlayPointer(QQuickItem* from) const
{
    for (QQuickItem* p = from; p; p = p->parentItem()) {
        if (p == m_mapWindow)
            break;
        p->ungrabMouse();
    }
}

bool TMapWindowController::filterWindowPointer(QEvent* event)
{
    if (!m_mapWindow || !m_provider || !m_actions)
        return false;
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton)
            return false;
        const QPointF local = m_mapWindow->mapFromScene(mouse->scenePosition());
        if (local.x() < 0 || local.y() < 0 || local.x() >= m_mapWindow->width()
            || local.y() >= m_mapWindow->height()) {
            m_mapPressActive = false;
            return false;
        }
        m_mapPressActive = true;
        m_mapDragStarted = false;
        m_mapOwnedPress = false;
        m_mapPressButton = mouse->button();
        m_mapPressModifiers = mouse->modifiers();
        m_mapPressPos = local;
        m_hovering = true;
        m_hoverPosition = local;
        // After delivery: did the FOV MouseArea get the grab, or did HUD chrome?
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_mapPressActive)
                    return;
                m_mapOwnedPress = m_mapMouseArea && m_mapMouseArea->property("pressed").toBool();
            },
            Qt::QueuedConnection);
        return false;
    }
    case QEvent::MouseMove: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        const QPointF local = m_mapWindow->mapFromScene(mouse->scenePosition());
        if (local.x() >= 0 && local.y() >= 0 && local.x() < m_mapWindow->width()
            && local.y() < m_mapWindow->height()) {
            m_hovering = true;
            m_hoverPosition = local;
            updateMouseTarget();
        }
        if (!m_mapPressActive || m_mapDragStarted || !(mouse->buttons() & Qt::LeftButton))
            return false;
        if (TDragAndDropController::instance()
            && TDragAndDropController::instance()->isDragging()) {
            m_mapDragStarted = true;
            return false;
        }
        const QPointF delta = local - m_mapPressPos;
        if (std::hypot(delta.x(), delta.y()) < MapDragThresholdPx)
            return false;
        // Live check — queued press ownership may not have landed yet.
        const bool mapOwns = m_mapOwnedPress
            || (m_mapMouseArea && m_mapMouseArea->property("pressed").toBool());
        m_mapOwnedPress = mapOwns;
        // A press grabbed by an item outside the map window belongs to a dialog
        // drawn over the FOV (the Cyclopedia map pans this way): that item's own
        // drag, not a map drag. Ungrabbing it would end the dialog's drag and pick
        // up an object on the field hidden under the dialog.
        if (!mapOwns && !mouse->points().isEmpty()) {
            if (const auto* grabber = qobject_cast<const QQuickItem*>(mouse->exclusiveGrabber(mouse->points().first()))) {
                bool underMapWindow = false;
                for (const QQuickItem* p = grabber; p && !underMapWindow; p = p->parentItem())
                    underMapWindow = p == m_mapWindow;
                if (!underMapWindow) {
                    m_mapPressActive = false;
                    return false;
                }
            }
        }
        // HUD button / empty overlay owned the press — take the drag for the map.
        if (!mapOwns) {
            if (QQuickItem* leaf = leafItemAtScenePos(mouse->scenePosition()))
                ungrabOverlayPointer(leaf);
            QObject* source = m_mapMouseArea ? static_cast<QObject*>(m_mapMouseArea.data())
                                             : static_cast<QObject*>(m_mapWindow.data());
            onMapWindowStartDrag(static_cast<int>(std::lround(m_mapPressPos.x())),
                                 static_cast<int>(std::lround(m_mapPressPos.y())), source);
        }
        m_mapDragStarted = true;
        return false;
    }
    case QEvent::MouseButtonRelease: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (!m_mapPressActive || mouse->button() != m_mapPressButton)
            return false;
        const bool dragged = m_mapDragStarted
            || (TDragAndDropController::instance()
                && TDragAndDropController::instance()->isDragging());
        const QPointF local = m_mapWindow->mapFromScene(mouse->scenePosition());
        const bool inMap = local.x() >= 0 && local.y() >= 0 && local.x() < m_mapWindow->width()
            && local.y() < m_mapWindow->height();
        m_mapPressActive = false;
        m_mapDragStarted = false;
        if (dragged || m_mapOwnedPress || !inMap)
            return false;
        // Press landed on non-interactive chrome above the FOV (or a full-bleed
        // overlay root). MouseArea never saw it — deliver the map click here.
        // Pass the real button/modifiers (not LeftButton): Classic Controls open
        // / Loot: Right are right-click chords on TGameActionMapper.
        if (isInteractiveOverlay(leafItemAtScenePos(mouse->scenePosition())))
            return false;
        onMapWindowClicked(static_cast<int>(std::lround(m_mapPressPos.x())),
                           static_cast<int>(std::lround(m_mapPressPos.y())),
                           static_cast<int>(m_mapPressButton),
                           static_cast<int>(m_mapPressModifiers));
        return false;
    }
    default:
        return false;
    }
}

bool TMapWindowController::eventFilter(QObject* watched, QEvent* event)
{
    if (m_pointerWindow && watched == m_pointerWindow)
        return filterWindowPointer(event);

    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
        m_hovering = true;
        m_hoverPosition = static_cast<QHoverEvent*>(event)->position();
        updateMouseTarget();
        break;
    case QEvent::MouseMove:
        m_hovering = true;
        m_hoverPosition = static_cast<QMouseEvent*>(event)->position();
        updateMouseTarget();
        break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate: {
        // Mobile / wasm: drive the same hover-target path as mouse (Qt also
        // synthesizes clicks into MapWindowPane's MouseArea).
        const auto* touch = static_cast<const QTouchEvent*>(event);
        if (!touch->points().isEmpty()) {
            m_hovering = true;
            m_hoverPosition = touch->points().first().position();
        }
        break;
    }
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        m_hovering = false;
        break;
    case QEvent::HoverLeave:
        // Do not clear here: IdleOT HUD over the FOV steals MouseArea hover while
        // the cursor is still on map tiles. updateMouseTarget / overlaysAdvanced
        // re-sync from QCursor; only then clear when outside mapWindow.
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QPoint TMapWindowController::toMapPixel(const QPointF& mapWindowPoint) const
{
    // Same placement as creature names and speech: the logical map drawn at
    // viewScale, centered in the map window. Sprites are already logical pixels
    // (HD textures are divided back down), so do not scale the click again.
    const qmlcomponents::TWorldMapRenderProvider::Frame& frame = m_provider->frame();
    const qreal width = m_mapWindow->width();
    const qreal height = m_mapWindow->height();
    if (width <= 0 || height <= 0)
        return frame.camera;
    const double mapW = frame.visibleWidth * qmlcomponents::TWorldMapRenderProvider::FieldSize;
    const double mapH = frame.visibleHeight * qmlcomponents::TWorldMapRenderProvider::FieldSize;
    const qmlcomponents::MapCoverScale cover = qmlcomponents::MapCoverScale::placed(width, height, mapW, mapH, frame.viewScale);
    const QPointF view = cover.toMap(mapWindowPoint.x(), mapWindowPoint.y());
    return QPoint(int(std::lround(view.x())), int(std::lround(view.y()))) + frame.camera;
}

bool TMapWindowController::syncHoverFromCursor()
{
    if (!m_mapWindow || !m_mapWindow->window()) {
        m_hovering = false;
        return false;
    }
    const QPointF local = m_mapWindow->mapFromGlobal(QCursor::pos(m_mapWindow->window()->screen()));
    if (local.x() >= 0 && local.y() >= 0 && local.x() < m_mapWindow->width()
        && local.y() < m_mapWindow->height()) {
        m_hovering = true;
        m_hoverPosition = local;
        return true;
    }
    m_hovering = false;
    return false;
}

void TMapWindowController::updateMouseTarget()
{
    // Prefer live cursor over the map MouseArea: HUD chrome above the FOV
    // (AutomationBar / ServicesBar) triggers HoverLeave while still on tiles.
    syncHoverFromCursor();
    if (!m_provider || !m_actions || !m_hovering || !m_mapWindow || !m_worldMap) {
        if (m_provider)
            m_provider->setMouseHighlightField({});
        return;
    }
    Cursor* cursor = Cursor::instance();
    // CIP : no hover candidates while a modal dialog is open.
    if (TDialogStackController* dialogs = TDialogStackController::instance();
        dialogs && !dialogs->isEmpty()) {
        if (cursor)
            cursor->setLayerShape(Cursor::MapWindow, Cursor::Default);
        m_provider->setMouseTargetObject(0, {}, -1);
        m_provider->setMouseHighlightField({});
        return;
    }

    const QPoint mapPixel = toMapPixel(m_hoverPosition);
    // CIP v21 baseplate: field under the cursor whenever Highlight Mouse Target is on.
    m_provider->setMouseHighlightEnabled(m_highlightMouseTarget);
    if (m_highlightMouseTarget && m_provider->camera().isValid())
        m_provider->setMouseHighlightField(m_provider->fieldAt(mapPixel, m_provider->camera().z));
    else
        m_provider->setMouseHighlightField({});

    const input::TGameActionList candidates = m_actions->actionsAt(mapPixel);
    // CIP : with mouseHighlightTarget on, the creature under the
    // mouse (none off creatures) is the temporarily selected one, like a
    // hovered battle-list entry.
    if (m_highlightMouseTarget) {
        for (const input::TGameActionPtr& candidate : candidates) {
            if (candidate->type == input::EGameAction::TEMPORARY_SELECT_CREATURE) {
                m_actionHandler->handleGameAction(candidate);
                break;
            }
        }
    }
    // CIP : the action a left click with the held modifiers would run.
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::TGameActionPtr action = m_mapper.map(candidates, input::TGameActionMapper::WorldMapContext, Qt::LeftButton, modifiers);
    if (!action) {
        if (cursor)
            cursor->setLayerShape(Cursor::MapWindow, Cursor::Default);
        m_provider->setMouseTargetObject(0, {}, -1);
        // Keep the baseplate field — empty tiles still show the square.
        return;
    }
    // CipSoft map hover sets the layer shape from the mapped left-click action
    // (cursorShapeOf → Crosshair for MOVE_OBJECT). mouseAnimatedCursor is owned
    // by TMouseCursorController (static vs animated cursor frames), not here.
    if (cursor)
        cursor->setLayerShape(Cursor::MapWindow, worldmap::TWorldMapGameActionProvider::cursorShapeOf(action));
    if (const uint32_t creatureId = worldmap::TWorldMapGameActionProvider::mouseTargetCreatureIdOf(action))
        m_provider->setMouseTargetCreatureId(creatureId);
    else if (const auto object = worldmap::TWorldMapGameActionProvider::mouseTargetObjectOf(action))
        m_provider->setMouseTargetObject(object.typeId, object.field, object.stackPos);
    else
        m_provider->setMouseTargetObject(0, {}, -1);
}

void TMapWindowController::onMapWindowClicked(const int mouseX, const int mouseY, const int mouseButton, const int keyboardModifiers)
{
    if (!m_provider || !m_actions || !m_mapWindow || !m_worldMap || !tibia::game::isOnline())
        return;
    if (input::isSelectingTarget()) {
        input::finishTargetSelection(mouseButton == Qt::LeftButton ? targetAt(QPointF(mouseX, mouseY)) : nullptr);
        return;
    }
    const input::TGameActionList candidates = m_actions->actionsAt(toMapPixel(QPointF(mouseX, mouseY)));
    const input::TGameActionPtr action = m_mapper.map(candidates, input::TGameActionMapper::WorldMapContext,
                                                      static_cast<Qt::MouseButton>(mouseButton), keyboardModifiers);
    if (!action)
        return;
    if (action->type != input::EGameAction::CONTEXT_MENU) {
        m_actionHandler->handleGameAction(action);
        // A click on the map closes the open multi action buttons.
        input::publish(std::make_shared<input::TGameActionCloseAllMultiActionButtons>());
        return;
    }
    TContextMenuController* menu = TContextMenuController::instance();
    if (const input::TGameActionList entries = input::contextMenuActions(candidates); menu && !entries.empty())
        menu->showGameActions(entries, input::TGameActionMapper::WorldMapContext, m_mapper);
}

void TMapWindowController::onMapWindowStartDrag(const int mouseX, const int mouseY, QObject* source)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd || !m_provider || !m_actions || !m_mapWindow || !m_worldMap || !tibia::game::isOnline())
        return;
    for (const input::TGameActionPtr& action : m_actions->actionsAt(toMapPixel(QPointF(mouseX, mouseY)))) {
        if (action->type == input::EGameAction::MOVE_OBJECT) {
            dnd->startDrag(action, source);
            return;
        }
    }
}

void TMapWindowController::onMapWindowDragDropped(const int mouseX, const int mouseY)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd || !m_provider || !m_mapWindow || !m_worldMap)
        return;
    const input::TGameActionPtr move = dnd->takeAction();
    if (!move || !tibia::game::isOnline())
        return;
    const worldmap::TWorldMapPick pick = worldmap::TWorldMapPicker::pick(*m_provider, *m_viewport, toMapPixel(QPointF(mouseX, mouseY)));
    if (!pick.isValid())
        return;
    const bool moveAll = app::ClientOptions::load().options().value(QLatin1String("dragAndDropDefaultActionIsMoveAll")).toBool(false);
    m_actionHandler->handleGameAction(
        input::finalizeDrop(move, pick.position, static_cast<int>(dnd->modifiers()), moveAll));
}

input::TGameActionPtr TMapWindowController::targetAt(const QPointF& mapWindowPoint) const
{
    if (!m_provider || !m_actions || !m_mapWindow || !m_worldMap)
        return nullptr;
    input::TGameActionPtr target;
    for (const input::TGameActionPtr& candidate : m_actions->actionsAt(toMapPixel(mapWindowPoint))) {
        if (candidate->type == input::EGameAction::TARGET_SELECTED && (!target || candidate->creatureId))
            target = candidate;
    }
    return target;
}

void TMapWindowController::onMapWindowTargetSelected(const int mouseX, const int mouseY)
{
    if (!tibia::game::isOnline())
        return;
    if (const input::TGameActionPtr target = targetAt(QPointF(mouseX, mouseY)))
        m_actionHandler->handleGameAction(target);
}

bool TMapWindowController::handleGameAction(const input::TGameAction& action)
{
    if (action.type == input::EGameAction::USE_AT_MOUSECURSOR)
        handleGameActionAtMouseCursor(action);
    else if (action.type == input::EGameAction::SPELL_AT_MOUSE_CURSOR)
        handleGameActionCastSpellAtMouseCursor(action);
    return true;
}

QPointF TMapWindowController::mouseCursorPosition(bool& onMap) const
{
    // Off the map, or under a dialog, nothing is at the mouse cursor.
    onMap = false;
    TDialogStackController* dialogs = TDialogStackController::instance();
    if (!m_provider || !m_viewport || !m_mapWindow || !m_mapWindow->window() || (dialogs && !dialogs->isEmpty()))
        return {};
    const QPointF point = m_mapWindow->mapFromScene(m_mapWindow->window()->mapFromGlobal(QCursor::pos()).toPointF());
    onMap = m_mapWindow->contains(point);
    return point;
}

void TMapWindowController::handleGameActionAtMouseCursor(const input::TGameAction& action)
{
    // The use waits a frame for a handler; off the map the battle lists may take it.
    trackUseAtMouseCursorGameAction();
    Cursor* cursor = Cursor::instance();
    if (!cursor || cursor->topRegion() != Cursor::MapWindow)
        return;
    clearTrackedUseAtMouseCursorGameAction();
    bool onMap = false;
    const QPointF point = mouseCursorPosition(onMap);
    if (!onMap)
        return;
    // The use goes to the TARGET_SELECTED under the mouse.
    const input::TGameActionPtr target = targetAt(point);
    if (!target)
        return;
    auto use = std::make_shared<input::TGameActionUseAtMouseCursor>(action);
    use->target = target;
    m_actionHandler->handleGameAction(use);
}

void TMapWindowController::handleGameActionCastSpellAtMouseCursor(const input::TGameAction& action)
{
    Cursor* cursor = Cursor::instance();
    if (!cursor || cursor->topRegion() != Cursor::MapWindow)
        return;
    bool onMap = false;
    const QPointF point = mouseCursorPosition(onMap);
    if (!onMap)
        return;
    // The spell's talk aimed (2) at the field under the mouse.
    const worldmap::TWorldMapPick pick = worldmap::TWorldMapPicker::pick(*m_provider, *m_viewport, toMapPixel(point));
    if (!pick.isValid() || action.text.isEmpty())
        return;
    tibia::game::setNextTalkAim(pick.position);
    tibia::game::setNextTalkAimMode(2);
    tibia::game::talkChannel(static_cast<tibia::client::MessageMode>(action.talkMode), 0, action.text.toStdString());
}

}
