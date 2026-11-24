#include "mapwindowcontroller.h"
#include "game/gamesessionbase.h"

#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "gamewindow/mousecursorcontroller.h"
#include "renderer/worldmaprenderprovider.h"
#include <QQuickItem>
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/draganddropcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/lenshelpmanager.h"
#include "qmlcomponents/numericaleffectoverlay.h"
#include "qmlcomponents/speechbubbleoverlay.h"
#include "renderer/lightmappainteditem.h"
#include "renderer/mapscale.h"
#include "renderer/worldmapquickitem.h"
#include "worldmap/creaturehudoverlaycontroller.h"
#include "worldmap/creaturehudrenderprovider.h"
#include "worldmap/worldmapgameactionprovider.h"
#include "worldmap/worldmappicker.h"
#include "worldmap/worldmaptutorialmarkeroverlaycontroller.h"
#include "worldmap/worldmapviewport.h"
#include <tibia/appearances/appearancetype.h>
#include <QCursor>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QTouchEvent>
#include <cmath>
#include <tibia/game/gamesession.h>
#include <utility>

namespace tibia::gamewindow {

namespace {

using Cursor = TMouseCursorController;
using qmlcomponents::TWorldMapRenderProvider;
using A = input::EGameAction;

constexpr A Separator = A::CONTEXT_MENU_SEPARATOR;

/// The map window's context menu: the entries in this order.
constexpr A ContextMenuOrder[] = {
    A::LOOK,
    A::INSPECT_OBJECT,
    A::OPEN_CYCLOPEDIA,
    A::OPEN_WEAPON_PROFICIENCY_DIALOG,
    A::USE,
    A::USE_WITH,
    A::OPEN_IN_NEW_WINDOW,
    A::WRAP,
    A::UNWRAP,
    A::ROTATE_OBJECT,
    A::CONFIGURE_SHOW_OFF_SOCKET,
    A::QUICK_LOOT,
    A::GATHER_ALL,
    A::BROWSE_FIELD,
    Separator,
    A::SHOW_DIALOG_CREATE_MINIMAP_MARKER,
    A::SHOW_DIALOG_EDIT_MINIMAP_MARKER,
    A::DELETE_MINIMAP_MARKER,
    Separator,
    A::TRADE_ITEM,
    Separator,
    A::LOOT_ADD_TO_BLACKWHITELIST,
    A::LOOT_REMOVE_FROM_BLACKWHITELIST,
    Separator,
    A::START_TALKING_TO_NPC,
    A::ATTACK_CREATURE,
    A::STOP_ATTACKING_CREATURE,
    A::FOLLOW_CREATURE,
    A::STOP_FOLLOWING_CREATURE,
    Separator,
    A::OPEN_MESSAGE_TO_CHAT_CHANNEL,
    A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL,
    A::ADD_NEW_VIP,
    A::IGNORE_NAME,
    A::UNIGNORE_NAME,
    A::RECENTLY_TRADED,
    A::INVITE_TO_PARTY,
    A::JOIN_PARTY,
    A::REVOKE_PARTY_INVITATION,
    A::PASS_PARTY_LEADERSHIP,
    A::JOIN_AGGRESSION,
    A::REQUEST_OUTFITS_FROM_SERVER,
    A::BUY_HIRELING_NAME_OR_SEX_CHANGE,
    A::MOUNT_PLAYER,
    A::UNMOUNT_PLAYER,
    A::OPEN_PREY_DIALOG,
    A::SET_SHARED_EXPERIENCE,
    A::LEAVE_PARTY,
    A::INSPECT_PLAYER,
    A::ASK_TO_INSPECT_PLAYER,
    A::INVITE_TO_INSPECT_PLAYER,
    A::ALLOW_INSPECT_PLAYER,
    A::REVOKE_INSPECT_PLAYER,
    Separator,
    A::REPORT_BUG_MAP,
    A::REPORT_NAME,
    A::REPORT_BOT,
    A::REPORT_HIRELING_NAME,
    A::GET_TEXT_FOR_REPORT,
    Separator,
    A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD,
};

}

TMapWindowController::TMapWindowController(QQmlEngine* engine, QQuickItem* pane, config::TClientOptions* clientOptions,
                                           std::shared_ptr<input::TGameActionMapper> GameActionMapper,
                                           qmlcomponents::TLenshelpManager* lenshelpManager, const bool softwareRenderer,
                                           QObject* parent)
    : cip::tibia::gamewindow::TMapWindowController(parent)
    , m_pEngine(engine)
    , m_pane(pane)
    , m_pClientOptions(clientOptions)
    , m_pGameActionMapper(std::move(GameActionMapper))
    , m_pLenshelpManager(lenshelpManager)
    , m_softwareRenderer(softwareRenderer)
{
    // The pane knows its controller; its items are found by objectName.
    if (pane) {
        pane->setProperty("mapWindowController", QVariant::fromValue<QObject*>(this));
        m_mapWindow = pane->findChild<QQuickItem*>(QStringLiteral("mapWindow"));
        m_worldMap = pane->findChild<qmlcomponents::TWorldMapQuickItem*>(QStringLiteral("worldmap"));
        m_numericalEffects = pane->findChild<qmlcomponents::TNumericalEffectOverlayQuickItem*>(QStringLiteral("numericalEffectOverlay"));
        m_speechBubbles = pane->findChild<qmlcomponents::TSpeechBubbleOverlayQuickItem*>(QStringLiteral("speechBubbleOverlay"));
        m_lightMap = pane->findChild<qmlcomponents::TLightmapPaintedItem*>(QStringLiteral("lightmap"));
        // The overlays follow the game session the controller is connected to.
        if (auto* overlay = pane->findChild<QQuickItem*>(QStringLiteral("worldmapCreatureHUDOverlay")))
            m_creatureHuds = new worldmap::TCreatureHUDOverlayController(overlay, this);
        if (auto* tutorialOverlay = pane->findChild<QQuickItem*>(QStringLiteral("tutorialMarkerOverlay")))
            m_tutorialMarkers = new worldmap::TWorldMapTutorialMarkerOverlayController(tutorialOverlay, this);
    }

    refreshClassicView();
    onMapWindowControllerOptionsChanged();
    if (m_pClientOptions)
        connect(m_pClientOptions, &config::TClientOptions::mapWindowControllerOptionsChanged, this,
                &TMapWindowController::onMapWindowControllerOptionsChanged);
    // Host: the pack's classic / immersive view and the target highlight options.
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("modImmersiveGameView") || name == QLatin1String("modClassicGameView"))
            refreshClassicView();
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
}

TMapWindowController::~TMapWindowController()
{
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

void TMapWindowController::updateTrackedUseAtMouseCursor()
{
    // A use at the mouse no window took: on the second frame the status line says so.
    if (!m_trackedUseAtMouseCursor || m_trackedUseAtMouseCursorFrames++ <= 0)
        return;
    if (!m_trackedUseAtMouseCursor->isHandled()) {
        auto status = std::make_shared<input::TGameActionSetStatusLine>(qtTrId("actionbar_action_no_target"));
        Q_EMIT publishGameAction(status);
    }
    clearTrackedUseAtMouseCursorGameAction();
}

void TMapWindowController::trackUseAtMouseCursorGameAction(const std::shared_ptr<const input::TGameActionUseAtMouseCursor>& action)
{
    clearTrackedUseAtMouseCursorGameAction();
    m_trackedUseAtMouseCursor = action;
}

void TMapWindowController::clearTrackedUseAtMouseCursorGameAction()
{
    m_trackedUseAtMouseCursor.reset();
    m_trackedUseAtMouseCursorFrames = 0;
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
    if (!m_pClientOptions)
        return;
    using config::EClientOption;
    const bool showNumericalEffects = m_pClientOptions->boolOption(EClientOption::gameWindowShowTextualEffects);
    const auto antialiasingMode =
        static_cast<cip::tibia::qmlcomponents::enums::EAntialiasingMode>(m_pClientOptions->antialiasingMode());
    const bool scaleOnlyByEvenMultiples = m_pClientOptions->boolOption(EClientOption::gameWindowScaleOnlyByEvenMultiples);
    // The light map needs a hardware renderer.
    const bool useLightMap = m_pClientOptions->boolOption(EClientOption::lightEffectsEnabled) && !m_softwareRenderer;
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
    connectToGameSession(nullptr);
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
            m_provider->clearMouseHighlightField();
        return;
    }
    Cursor* cursor = Cursor::instance();
    // CIP : no hover candidates while a modal dialog is open.
    if (TDialogStackController* dialogs = TDialogStackController::instance();
        dialogs && !dialogs->isEmpty()) {
        if (cursor)
            cursor->setLayerShape(Cursor::MapWindow, Cursor::Default);
        m_provider->setMouseTargetObject(0, {}, -1);
        m_provider->clearMouseHighlightField();
        return;
    }

    const QPoint mapPixel = toMapPixel(m_hoverPosition);
    const bool highlightMouseTarget = m_pClientOptions && m_pClientOptions->boolOption(config::EClientOption::mouseHighlightTarget);
    // CIP v21 baseplate: field under the cursor whenever Highlight Mouse Target is on.
    m_provider->setMouseHighlightEnabled(highlightMouseTarget);
    if (highlightMouseTarget && m_provider->camera().isValid())
        m_provider->setMouseHighlightField(m_provider->fieldAt(mapPixel, m_provider->camera().z));
    else
        m_provider->clearMouseHighlightField();

    const input::TGameActionList candidates = m_actions->actionsAt(mapPixel);
    // CIP : with mouseHighlightTarget on, the creature under the
    // mouse (none off creatures) is the temporarily selected one, like a
    // hovered battle-list entry.
    if (highlightMouseTarget) {
        for (const input::IGameActionPtr& candidate : candidates) {
            if (candidate->gameActionType() == input::EGameAction::TEMPORARY_SELECT_CREATURE) {
                Q_EMIT publishGameAction(candidate);
                break;
            }
        }
    }
    // CIP : the action a left click with the held modifiers would run.
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::IGameActionPtr action = m_pGameActionMapper->map(candidates, input::TGameActionMapper::WorldMapContext, Qt::LeftButton, modifiers);
    if (!action) {
        if (cursor)
            cursor->setLayerShape(Cursor::MapWindow, Cursor::Default);
        m_provider->setMouseTargetObject(0, {}, -1);
        // Keep the baseplate field — empty tiles still show the square.
    } else {
        // CipSoft map hover sets the layer shape from the mapped left-click action
        // (cursorShapeOf → Crosshair for MOVE_OBJECT). mouseAnimatedCursor is owned
        // by TMouseCursorController (static vs animated cursor frames), not here.
        if (cursor)
            cursor->setLayerShape(Cursor::MapWindow, worldmap::TWorldMapGameActionProvider::cursorShapeOf(action));
        const auto target = worldmap::TWorldMapGameActionProvider::mouseTargetOf(action);
        if (target.creatureId)
            m_provider->setMouseTargetCreatureId(target.creatureId);
        else if (target)
            m_provider->setMouseTargetObject(target.typeId, target.field, target.stackPos);
        else
            m_provider->setMouseTargetObject(0, {}, -1);
    }
    updateLenshelpAtMouseCursor(candidates);
}

void TMapWindowController::updateLenshelpAtMouseCursor(const input::TGameActionList& candidates)
{
    if (!m_gameSession)
        return;
    std::shared_ptr<const input::TGameActionShowLenshelp> show;
    if (m_pLenshelpManager && m_pLenshelpManager->isEnabled()) {
        for (const input::IGameActionPtr& candidate : candidates) {
            if (candidate && candidate->gameActionType() == input::EGameAction::LENSHELP_SHOW) {
                show = std::dynamic_pointer_cast<const input::TGameActionShowLenshelp>(candidate);
                if (show)
                    break;
            }
        }
    }
    const bool shown = show != nullptr;
    if (shown) {
        // The field's top-left in the map window, in scene coordinates, framed
        // by 2 pixels on each side.
        const qmlcomponents::TWorldMapRenderProvider::Frame& frame = m_provider->frame();
        const double mapW = frame.visibleWidth * qmlcomponents::TWorldMapRenderProvider::FieldSize;
        const double mapH = frame.visibleHeight * qmlcomponents::TWorldMapRenderProvider::FieldSize;
        const qmlcomponents::MapCoverScale cover =
            qmlcomponents::MapCoverScale::placed(m_mapWindow->width(), m_mapWindow->height(), mapW, mapH, frame.viewScale);
        const QPoint pixel = m_provider->mapPixelOf(show->coordinate()) - frame.camera;
        const QPointF scene = m_mapWindow->mapToScene(cover.toPaneRounded(pixel.x(), pixel.y()));
        const double size = cover.scale * qmlcomponents::TWorldMapRenderProvider::FieldSize + 4.0;
        const QRectF rect(scene.x() - 2.0, scene.y() - 2.0, size, size);
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowLenshelp>(show->lensHelpId(), show->coordinate(), rect));
    } else if (m_lenshelpShown) {
        Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionSimple>(input::EGameAction::LENSHELP_HIDE));
    }
    m_lenshelpShown = shown;
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
    const input::IGameActionPtr action =
m_pGameActionMapper->map(candidates, input::TGameActionMapper::WorldMapContext,
                                                      static_cast<Qt::MouseButton>(mouseButton), keyboardModifiers);
    if (!action)
        return;
    if (action->gameActionType() != input::EGameAction::CONTEXT_MENU) {
        Q_EMIT publishGameAction(action);
        // A click on the map closes the open multi action buttons.
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionCloseAllMultiActionButtons>());
        return;
    }
    TContextMenuController* menu = TContextMenuController::instance();
    if (const input::TGameActionList entries = input::contextMenuActions(candidates, ContextMenuOrder); menu && !entries.empty())
        menu->showGameActions(entries, input::TGameActionMapper::WorldMapContext, *m_pGameActionMapper);
}

void TMapWindowController::onMapWindowStartDrag(const int mouseX, const int mouseY, QObject* source)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd || !m_provider || !m_actions || !m_mapWindow || !m_worldMap || !tibia::game::isOnline())
        return;
    for (const input::IGameActionPtr& action : m_actions->actionsAt(toMapPixel(QPointF(mouseX, mouseY)))) {
        if (action->gameActionType() == input::EGameAction::MOVE_OBJECT) {
            dnd->startDrag(std::dynamic_pointer_cast<const input::TGameActionMoveObject>(action), source);
            return;
        }
    }
}

void TMapWindowController::onMapWindowDragDropped(const int mouseX, const int mouseY)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd || !m_provider || !m_mapWindow || !m_worldMap)
        return;
    const input::TMoveObjectPtr move = dnd->takeAction();
    if (!move || !tibia::game::isOnline())
        return;
    const worldmap::TWorldMapPick pick = worldmap::TWorldMapPicker::pick(*m_provider, *m_viewport, toMapPixel(QPointF(mouseX, mouseY)));
    if (!pick.isValid())
        return;
    const bool moveAll = app::ClientOptions::load().options().value(QLatin1String("dragAndDropDefaultActionIsMoveAll")).toBool(false);
    Q_EMIT publishGameAction(
        input::finalizeDrop(move, pick.position, static_cast<int>(dnd->modifiers()), moveAll));
}

input::TTargetSelectedPtr TMapWindowController::targetAt(const QPointF& mapWindowPoint) const
{
    if (!m_provider || !m_actions || !m_mapWindow || !m_worldMap)
        return nullptr;
    input::TTargetSelectedPtr target;
    for (const input::IGameActionPtr& candidate : m_actions->actionsAt(toMapPixel(mapWindowPoint))) {
        auto selected = std::dynamic_pointer_cast<const input::TGameActionTargetSelected>(candidate);
        if (selected && (!target || selected->creatureId()))
            target = std::move(selected);
    }
    return target;
}

void TMapWindowController::onMapWindowTargetSelected(const int mouseX, const int mouseY)
{
    if (!tibia::game::isOnline())
        return;
    if (const input::TTargetSelectedPtr target = targetAt(QPointF(mouseX, mouseY)))
        Q_EMIT publishGameAction(target);
}

void TMapWindowController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    if (pGameAction->gameActionType() == input::EGameAction::USE_AT_MOUSECURSOR) {
        if (auto use = std::dynamic_pointer_cast<const input::TGameActionUseAtMouseCursor>(pGameAction))
            handleGameActionAtMouseCursor(use);
    } else if (const auto* spell = input::gameActionAs<input::TGameActionSpellAtMouseCursor>(pGameAction)) {
        handleGameActionCastSpellAtMouseCursor(*spell);
    }
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

void TMapWindowController::handleGameActionAtMouseCursor(const std::shared_ptr<const input::TGameActionUseAtMouseCursor>& action)
{
    // The use waits a frame for a window to take it; off the map the battle lists may.
    trackUseAtMouseCursorGameAction(action);
    Cursor* cursor = Cursor::instance();
    if (!cursor || cursor->topRegion() != Cursor::MapWindow)
        return;
    // Over the map the map window takes it, with or without a target there.
    bool onMap = false;
    const QPointF point = mouseCursorPosition(onMap);
    const input::TTargetSelectedPtr target = onMap ? targetAt(point) : nullptr;
    if (!target) {
        action->setHandled();
        return;
    }
    // The use goes to the TARGET_SELECTED under the mouse: on a creature, or
    // with the object there.
    if (target->creatureId())
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionUseOnCreature>(action->context(), target->creatureId()));
    else
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionUseWith>(action->context(), target->context()));
    action->setHandled();
}

void TMapWindowController::handleGameActionCastSpellAtMouseCursor(const input::TGameActionSpellAtMouseCursor& action)
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
    if (!pick.isValid() || action.words().isEmpty())
        return;
    tibia::game::setNextTalkAim(pick.position);
    tibia::game::setNextTalkAimMode(2);
    tibia::game::talkChannel(static_cast<tibia::client::MessageMode>(action.talkMode()), 0, action.words().toStdString());
}

void TMapWindowController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    // The previous session lives until its pieces are unset everywhere.
    const std::shared_ptr<game::IGameSession> previous = std::move(m_gameSession);
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    TWorldMapRenderProvider* provider = session ? session->worldMapRenderProvider().get() : nullptr;
    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
        m_provider->setMouseTargetObject(0, {}, -1);
        m_provider->clearMouseHighlightField();
    }
    // The pane's items draw the session's world, texts and numerical effects.
    if (m_worldMap)
        m_worldMap->setRenderProvider(provider);
    if (m_lightMap)
        m_lightMap->setRenderProvider(provider);
    if (m_creatureHuds)
        m_creatureHuds->setRenderProviders(session ? session->creatureHUDRenderProvider() : nullptr, provider);
    if (m_speechBubbles)
        m_speechBubbles->setSources(provider, session ? session->textOverlayRenderProvider() : nullptr);
    if (m_numericalEffects)
        m_numericalEffects->setSources(provider, session ? session->numericalEffectRenderProvider() : nullptr,
                                       session ? session->renderVolumeProvider() : nullptr);
    if (m_tutorialMarkers)
        m_tutorialMarkers->setSession(GameSession);
    if (!session) {
        m_viewport = nullptr;
        m_actions.reset();
        m_provider = nullptr;
        return;
    }
    m_viewport = session->worldMapViewport().get();
    m_actions = session->worldMapGameActionProvider();
    m_provider = provider;
    // The session's frame is the map window's size and scale; its view covers
    // the visible fields.
    syncFov();
    m_provider->setVisibleFields(m_fovWidth, m_fovHeight);
    m_provider->setViewScale(m_viewScale);
    const auto sizeViewport = [this] {
        if (m_viewport && m_provider)
            m_viewport->setSize(m_provider->visibleWidth() * TWorldMapRenderProvider::FieldSize,
                                m_provider->visibleHeight() * TWorldMapRenderProvider::FieldSize);
    };
    sizeViewport();
    connect(m_provider, &TWorldMapRenderProvider::visibleFieldsChanged, this, sizeViewport);
    applyTargetHighlightOption();
    // The world moves under a resting mouse.
    connect(m_provider, &TWorldMapRenderProvider::overlaysAdvanced, this, &TMapWindowController::updateMouseTarget);
    connect(m_provider, &TWorldMapRenderProvider::overlaysAdvanced, this, &TMapWindowController::updateTrackedUseAtMouseCursor);
}

}
