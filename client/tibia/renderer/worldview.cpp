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

#include <tibia/world/game.h>
#include <tibia/world/thing.h>

#include <QCursor>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTouchEvent>

#include <cmath>


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
    m_animatedCursor = options.value(QLatin1String("mouseAnimatedCursor")).toBool(true);
    m_mapper.setControlScheme(options.value(QLatin1String("controlSchemeIndex")).toInt(0));
    m_mapper.setLootScheme(options.value(QLatin1String("lootSchemeIndex")).toInt(1));
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
        else if (name == QLatin1String("mouseAnimatedCursor"))
            m_animatedCursor = options.value(name).toBool(true);
        else if (name == QLatin1String("controlSchemeIndex"))
            m_mapper.setControlScheme(options.value(name).toInt(0));
        else if (name == QLatin1String("lootSchemeIndex"))
            m_mapper.setLootScheme(options.value(name).toInt(1));
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
    if (auto* tutorialOverlay = pane->findChild<QQuickItem*>(QStringLiteral("tutorialMarkerOverlay")))
        m_tutorialMarkers = new worldmap::TWorldMapTutorialMarkerOverlayController(tutorialOverlay, this);
    if (m_speechBubbles && texts)
        m_speechBubbles->setSources(provider, texts);
    if (m_numericalEffects && numericalEffects)
        m_numericalEffects->setSources(provider, numericalEffects);

    if (m_mapWindow) {
        connect(m_mapWindow, SIGNAL(mapWindowClicked(int, int, int, int)), this, SLOT(onMapWindowClicked(int, int, int, int)));
        connect(m_mapWindow, SIGNAL(mapWindowStartDrag(int, int, QObject*)), this, SLOT(onMapWindowStartDrag(int, int, QObject*)));
        connect(m_mapWindow, SIGNAL(mapWindowDragDropped(int, int)), this, SLOT(onMapWindowDragDropped(int, int)));
        connect(m_mapWindow, SIGNAL(mapWindowTargetSelected(int, int)), this, SLOT(onMapWindowTargetSelected(int, int)));
        // MapWindowPane's hover MouseArea (mouseAreaDrag) has no objectName.
        for (QQuickItem* child : m_mapWindow->childItems()) {
            if (child->inherits("QQuickMouseArea")) {
                child->installEventFilter(this);
                break;
            }
        }
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
    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
        m_provider->setMouseTarget(nullptr);
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

bool TWorldView::eventFilter(QObject* watched, QEvent* event)
{
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
        // The cursor layer and the hover object go with mapWindowController.onMapWindowExited.
        m_hovering = false;
        if (m_provider)
            m_provider->setMouseHighlightField({});
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

void TWorldView::updateMouseTarget()
{
    if (!m_provider || !m_actions || !m_hovering || !m_mapWindow || !m_worldMap) {
        if (m_provider)
            m_provider->setMouseHighlightField({});
        return;
    }
    Cursor* cursor = Cursor::instance();
    if (cursor && cursor->topRegion() != Cursor::MapWindow) {
        m_provider->setMouseHighlightField({});
        return;
    }
    // CIP FUN_100c15470: no hover candidates while a modal dialog is open.
    if (gamewindow::TDialogStackController* dialogs = gamewindow::TDialogStackController::instance();
        dialogs && !dialogs->isEmpty()) {
        if (cursor)
            cursor->setLayerShape(Cursor::MapWindow, Cursor::Default);
        m_provider->setMouseTarget(nullptr);
        m_provider->setMouseHighlightField({});
        return;
    }

    const QPoint mapPixel = toMapPixel(m_hoverPosition);
    // CIP v21 baseplate: field under the cursor whenever Highlight Mouse Target is on.
    if (m_highlightMouseTarget && m_provider->camera().isValid())
        m_provider->setMouseHighlightField(m_provider->fieldAt(mapPixel, m_provider->camera().z));
    else
        m_provider->setMouseHighlightField({});

    const input::TGameActionList candidates = m_actions->actionsAt(mapPixel);
    // CIP FUN_100b6d160: with mouseHighlightTarget on, the creature under the
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
    // CIP (FUN_100b6d430): the action a left click with the held modifiers would run.
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::TGameActionPtr action = m_mapper.map(candidates, input::TGameActionMapper::WorldMapContext, Qt::LeftButton, modifiers);
    if (!action) {
        if (cursor)
            cursor->setLayerShape(Cursor::MapWindow, Cursor::Default);
        m_provider->setMouseTarget(nullptr);
        // Keep the baseplate field — empty tiles still show the square.
        return;
    }
    if (cursor && m_animatedCursor)
        cursor->setLayerShape(Cursor::MapWindow, worldmap::TWorldMapGameActionProvider::cursorShapeOf(action));
    m_provider->setMouseHighlightEnabled(m_highlightMouseTarget);
    m_provider->setMouseTarget(worldmap::TWorldMapGameActionProvider::mouseTargetOf(action));
}

void TWorldView::onMapWindowClicked(const int mouseX, const int mouseY, const int mouseButton, const int keyboardModifiers)
{
    if (!m_provider || !m_actions || !m_mapWindow || !m_worldMap || !g_game.isOnline())
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
    if (!dnd || !m_provider || !m_actions || !m_mapWindow || !m_worldMap || !g_game.isOnline())
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
    if (!move || !g_game.isOnline())
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
    if (!g_game.isOnline())
        return;
    if (const input::TGameActionPtr target = targetAt(QPointF(mouseX, mouseY)))
        m_actionHandler->handleGameAction(target);
}

bool TWorldView::onAtMouseCursor(const input::TGameAction& action)
{
    // CIP FUN_100b6ba10: off the map a use waits for another handler (the battle
    // lists take theirs first); FUN_100b6c710 / FUN_100b6c880: no dialog up.
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
        // CIP FUN_100b6ba10: use with the TARGET_SELECTED under the mouse.
        const input::TGameActionPtr target = targetAt(point);
        if (!target)
            return true;
        auto use = std::make_shared<input::TGameActionUseAtMouseCursor>(action);
        use->target = target;
        m_actionHandler->handleGameAction(use);
        return true;
    }
    // CIP FUN_100b6c2c0: the spell's Talk aimed (2) at the field under the mouse.
    const worldmap::TWorldMapPick pick = worldmap::TWorldMapPicker::pick(*m_provider, toMapPixel(point));
    if (!pick.isValid() || action.text.isEmpty())
        return true;
    g_game.setNextTalkAim(pick.position);
    g_game.setNextTalkAimMode(2);
    g_game.talkChannel(static_cast<Otc::MessageMode>(action.talkMode), 0, action.text.toStdString());
    return true;
}

}
