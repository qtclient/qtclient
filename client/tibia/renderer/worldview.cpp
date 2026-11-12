#include "worldview.h"

#include "app/clientoptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/draganddropcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
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

#include <tibia/appearances/thing.h>

#include <QCursor>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTouchEvent>

#include <cmath>
#include <tibia/game/gamesession.h>


namespace tibia::qmlcomponents {

namespace {

using Cursor = gamewindow::TMouseCursorController;

}

TWorldView::TWorldView(QQuickItem* pane, TWorldMapRenderProvider* provider, const chat::TTextStorage* texts,
                       const worldmap::TNumericalEffectStorage* numericalEffects)
    : QObject(pane)
    , m_provider(provider)
    , m_actions(std::make_unique<worldmap::TWorldMapGameActionProvider>(*provider))
    , m_actionHandler(new worldmap::TWorldMapGameActionHandler(this))
    , m_viewport(new worldmap::TWorldMapViewport(provider, this))
    , m_mapWindow(pane->findChild<QQuickItem*>(QStringLiteral("mapWindow")))
    , m_worldMap(pane->findChild<TWorldMapQuickItem*>(QStringLiteral("worldmap")))
    , m_lightMap(pane->findChild<TLightmapPaintedItem*>(QStringLiteral("lightmap")))
    , m_speechBubbles(pane->findChild<TSpeechBubbleOverlayQuickItem*>(QStringLiteral("speechBubbleOverlay")))
    , m_numericalEffects(pane->findChild<TNumericalEffectOverlayQuickItem*>(QStringLiteral("numericalEffectOverlay")))
{
    const QJsonObject options = app::ClientOptions::load().options();
    m_highlightMouseTarget = options.value(QLatin1String("mouseHighlightTarget")).toBool(true);
    m_mapper.setControlScheme(options.value(QLatin1String("controlSchemeIndex")).toInt(0));
    // CIP default lootSchemeIndex is 0 (Loot: Right); match clientoptiondefaults.
    m_mapper.setLootScheme(options.value(QLatin1String("lootSchemeIndex")).toInt(0));
    // CIP 0x4c ShowTargetHighlight + 0x4d ShowAttackAnimation both gate the
    // red/green creature tint (RE doc names 0x4c; options.json has both keys).
    const auto attackHighlightOn = [](const QJsonObject& o) {
        return o.value(QLatin1String("gameWindowShowAttackAnimation")).toBool(true)
            || o.value(QLatin1String("gameWindowShowTargetHighlight")).toBool(true);
    };
    provider->setTargetHighlightEnabled(attackHighlightOn(options));
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this, attackHighlightOn](const QString& name) {
        const QJsonObject options = app::ClientOptions::load().options();
        if (name == QLatin1String("mouseHighlightTarget"))
            m_highlightMouseTarget = options.value(name).toBool(true);
        else if (name == QLatin1String("controlSchemeIndex"))
            m_mapper.setControlScheme(options.value(name).toInt(0));
        else if (name == QLatin1String("lootSchemeIndex"))
            m_mapper.setLootScheme(options.value(name).toInt(0));
        else if (name == QLatin1String("gameWindowShowAttackAnimation")
                 || name == QLatin1String("gameWindowShowTargetHighlight"))
            m_provider->setTargetHighlightEnabled(attackHighlightOn(options));
    });

    if (m_worldMap)
        m_worldMap->setRenderProvider(provider);
    if (m_lightMap)
        m_lightMap->setRenderProvider(provider);
    if (auto* overlay = pane->findChild<QQuickItem*>(QStringLiteral("worldmapCreatureHUDOverlay")))
        m_creatureHuds = new worldmap::TCreatureHUDController(overlay, provider, this);
    if (auto* tutorialOverlay = pane->findChild<QQuickItem*>(QStringLiteral("tutorialMarkerOverlay"))) {
        m_tutorialMarkers = new worldmap::TWorldMapTutorialMarkerOverlayController(tutorialOverlay, this);
        m_tutorialMarkers->setRenderProvider(provider);
    }
    if (m_speechBubbles && texts)
        m_speechBubbles->setSources(provider, texts);
    if (m_numericalEffects && numericalEffects)
        m_numericalEffects->setSources(provider, numericalEffects);

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
    }
    // The world moves under a resting mouse.
    connect(provider, &TWorldMapRenderProvider::overlaysAdvanced, this, &TWorldView::updateMouseTarget);
    connect(provider, &TWorldMapRenderProvider::overlaysAdvanced, this, [this] {
        if (m_unhandledAtMouseFrames < 0 || ++m_unhandledAtMouseFrames < 2)
            return;
        m_unhandledAtMouseFrames = -1;
        auto status = input::mutableClone(*input::makeAction(input::EGameAction::SET_STATUS_LINE));
        status->text = qtTrId("actionbar_action_no_target");
        input::publish(status);
    });
    for (const input::EGameAction type : { input::EGameAction::USE_AT_MOUSECURSOR, input::EGameAction::SPELL_AT_MOUSE_CURSOR })
        input::setActionHandler(type, [this](const input::TGameAction& action) { return onAtMouseCursor(action); });
}

TWorldView::~TWorldView()
{
    for (const input::EGameAction type : { input::EGameAction::USE_AT_MOUSECURSOR, input::EGameAction::SPELL_AT_MOUSE_CURSOR })
        input::setActionHandler(type, nullptr);
    unbind();
}

void TWorldView::unbind()
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

void TWorldView::ensureWindowPointerFilter()
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

QQuickItem* TWorldView::leafItemAtScenePos(const QPointF& scenePos) const
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
        return root;
    };
    return walk(walk, m_pointerWindow->contentItem());
}

bool TWorldView::isMapSubtree(const QQuickItem* item) const
{
    for (const QQuickItem* p = item; p; p = p->parentItem()) {
        if (p == m_mapWindow || p == m_mapMouseArea)
            return true;
    }
    return false;
}

bool TWorldView::isInteractiveOverlay(const QQuickItem* leaf) const
{
    if (!leaf || isMapSubtree(leaf))
        return false;
    for (const QQuickItem* p = leaf; p; p = p->parentItem()) {
        if (p == m_mapWindow || p == m_mapMouseArea)
            return false;
        if (p->acceptedMouseButtons() != Qt::NoButton)
            return true;
        if (p->inherits("QQuickMouseArea") || p->inherits("QQuickAbstractButton"))
            return true;
    }
    return false;
}

void TWorldView::ungrabOverlayPointer(QQuickItem* from) const
{
    for (QQuickItem* p = from; p; p = p->parentItem()) {
        if (p == m_mapWindow)
            break;
        p->ungrabMouse();
    }
}

bool TWorldView::filterWindowPointer(QEvent* event)
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
        if (gamewindow::TDragAndDropController::instance()
            && gamewindow::TDragAndDropController::instance()->isDragging()) {
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
            || (gamewindow::TDragAndDropController::instance()
                && gamewindow::TDragAndDropController::instance()->isDragging());
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

bool TWorldView::eventFilter(QObject* watched, QEvent* event)
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

QPoint TWorldView::toMapPixel(const QPointF& mapWindowPoint) const
{
    // Same placement as creature names and speech: the logical map drawn at
    // viewScale, centered in the map window. Sprites are already logical pixels
    // (HD textures are divided back down), so do not scale the click again.
    const TWorldMapRenderProvider::Frame& frame = m_provider->frame();
    const qreal width = m_mapWindow->width();
    const qreal height = m_mapWindow->height();
    if (width <= 0 || height <= 0)
        return frame.camera;
    const double mapW = frame.visibleWidth * TWorldMapRenderProvider::FieldSize;
    const double mapH = frame.visibleHeight * TWorldMapRenderProvider::FieldSize;
    const MapCoverScale cover = MapCoverScale::placed(width, height, mapW, mapH, frame.viewScale);
    const QPointF view = cover.toMap(mapWindowPoint.x(), mapWindowPoint.y());
    return QPoint(int(std::lround(view.x())), int(std::lround(view.y()))) + frame.camera;
}

bool TWorldView::syncHoverFromCursor()
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

void TWorldView::updateMouseTarget()
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
    if (gamewindow::TDialogStackController* dialogs = gamewindow::TDialogStackController::instance();
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

void TWorldView::onMapWindowClicked(const int mouseX, const int mouseY, const int mouseButton, const int keyboardModifiers)
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
        return;
    }
    gamewindow::TContextMenuController* menu = gamewindow::TContextMenuController::instance();
    if (const input::TGameActionList entries = input::contextMenuActions(candidates); menu && !entries.empty())
        menu->showGameActions(entries, input::TGameActionMapper::WorldMapContext, m_mapper);
}

void TWorldView::onMapWindowStartDrag(const int mouseX, const int mouseY, QObject* source)
{
    gamewindow::TDragAndDropController* dnd = gamewindow::TDragAndDropController::instance();
    if (!dnd || !m_provider || !m_actions || !m_mapWindow || !m_worldMap || !tibia::game::isOnline())
        return;
    for (const input::TGameActionPtr& action : m_actions->actionsAt(toMapPixel(QPointF(mouseX, mouseY)))) {
        if (action->type == input::EGameAction::MOVE_OBJECT) {
            dnd->startDrag(action, source);
            return;
        }
    }
}

void TWorldView::onMapWindowDragDropped(const int mouseX, const int mouseY)
{
    gamewindow::TDragAndDropController* dnd = gamewindow::TDragAndDropController::instance();
    if (!dnd || !m_provider || !m_mapWindow || !m_worldMap)
        return;
    const input::TGameActionPtr move = dnd->takeAction();
    if (!move || !tibia::game::isOnline())
        return;
    const worldmap::TWorldMapPick pick = worldmap::TWorldMapPicker::pick(*m_provider, toMapPixel(QPointF(mouseX, mouseY)));
    if (!pick.isValid())
        return;
    const bool moveAll = app::ClientOptions::load().options().value(QLatin1String("dragAndDropDefaultActionIsMoveAll")).toBool(false);
    m_actionHandler->handleGameAction(
        input::finalizeDrop(move, pick.position, static_cast<int>(dnd->modifiers()), moveAll));
}

input::TGameActionPtr TWorldView::targetAt(const QPointF& mapWindowPoint) const
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

void TWorldView::onMapWindowTargetSelected(const int mouseX, const int mouseY)
{
    if (!tibia::game::isOnline())
        return;
    if (const input::TGameActionPtr target = targetAt(QPointF(mouseX, mouseY)))
        m_actionHandler->handleGameAction(target);
}

bool TWorldView::onAtMouseCursor(const input::TGameAction& action)
{
    // CIP : off the map a use waits for another handler (the battle
    // lists take theirs first); / : no dialog up.
    Cursor* cursor = Cursor::instance();
    if (!m_provider || !m_mapWindow || !m_mapWindow->window() || !cursor || cursor->topRegion() != Cursor::MapWindow) {
        if (action.type == input::EGameAction::USE_AT_MOUSECURSOR)
            m_unhandledAtMouseFrames = 0;
        return true;
    }
    gamewindow::TDialogStackController* dialogs = gamewindow::TDialogStackController::instance();
    if (dialogs && !dialogs->isEmpty())
        return true;
    const QPointF point = m_mapWindow->mapFromScene(m_mapWindow->window()->mapFromGlobal(QCursor::pos()).toPointF());
    if (!m_mapWindow->contains(point))
        return true;
    if (action.type == input::EGameAction::USE_AT_MOUSECURSOR) {
        // CIP : use with the TARGET_SELECTED under the mouse.
        const input::TGameActionPtr target = targetAt(point);
        if (!target)
            return true;
        auto use = std::make_shared<input::TGameActionUseAtMouseCursor>(action);
        use->target = target;
        m_actionHandler->handleGameAction(use);
        return true;
    }
    // CIP : the spell's Talk aimed (2) at the field under the mouse.
    const worldmap::TWorldMapPick pick = worldmap::TWorldMapPicker::pick(*m_provider, toMapPixel(point));
    if (!pick.isValid() || action.text.isEmpty())
        return true;
    tibia::game::setNextTalkAim(pick.position);
    tibia::game::setNextTalkAimMode(2);
    tibia::game::talkChannel(static_cast<tibia::client::MessageMode>(action.talkMode), 0, action.text.toStdString());
    return true;
}

}
