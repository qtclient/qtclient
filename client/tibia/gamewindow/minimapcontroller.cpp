#include "minimapcontroller.h"

#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"
#include "minimap/minimaprenderprovider.h"
#include "minimap/minimaptilestorage.h"
#include "qmlcomponents/minimapquickitem.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/worldmapgameactionprovider.h"

#include <QGuiApplication>
#include <QQuickItem>

#include <cmath>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Cursor = TMouseCursorController;

/// The context menu of a minimap field is "create mark", that of a marker
/// "edit mark" and "delete mark". The fields' actions here already carry the
/// marker's entries when the field has one (there is no marker overlay to
/// click), so one order serves both.
/// CIP: a click on a field offers a new marker, a click on a marker edits
/// or deletes it.
constexpr A FieldContextMenuOrder[] = { A::SHOW_DIALOG_CREATE_MINIMAP_MARKER };
constexpr A MarkerContextMenuOrder[] = { A::SHOW_DIALOG_EDIT_MINIMAP_MARKER, A::DELETE_MINIMAP_MARKER };

/// Rounds half away from zero, as std::lround does.
int roundToInt(const float v)
{
    return static_cast<int>(std::round(v));
}

}

TMinimapController::TMinimapController(QObject* parent)
    : cip::tibia::gamewindow::TMinimapController(parent)
{
    // CIP: Thais temple until the player is known.
    m_area.setCenter(Position(32369, 32215, 7));

    m_positionHook = client::gamecore::Hook(tibia::game::kNotifyScope, "onPositionChange", [this](GameObject*, const std::vector<std::any>& args) {
        if (!args.empty())
            if (const auto* position = std::any_cast<Position>(&args[0]))
                onPlayerPositionChanged(*position);
        return std::any{};
    });
    input::setMinimapActionHandler([this](const input::IGameAction& action) {
        handleGameAction(action);
    });
}

TMinimapController::~TMinimapController()
{
    input::setMinimapActionHandler(nullptr);
    if (m_minimap)
        m_minimap->setRenderProvider(nullptr);
}

void TMinimapController::setTileStorage(minimap::TMinimapTileStorage* storage)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_storage = storage;
    m_renderProvider = storage ? std::make_unique<minimap::TMinimapRenderProvider>(storage, &m_area) : nullptr;
    if (m_minimap)
        m_minimap->setRenderProvider(m_renderProvider.get());
    if (storage) {
        connect(storage, &minimap::TMinimapTileStorage::minimapTileChanged, this, &TMinimapController::onMinimapDataChanged);
        if (tibia::game::hasLocalPlayer()) {
            const Position pos = tibia::game::localPlayerPosition();
            if (pos.isValid())
                onPlayerPositionChanged(pos);
        }
    }
    refresh();
}

void TMinimapController::setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight)
{
    if (m_ambientLight)
        disconnect(m_ambientLight, nullptr, this, nullptr);
    m_ambientLight = ambientLight;
    if (ambientLight) {
        connect(ambientLight, &worldmap::TAmbientLightStorage::normalizedTibiaTimeOfDayWasChanged, this, &TMinimapController::updateTimeDisplay);
        connect(ambientLight, &worldmap::TAmbientLightStorage::interpolatedTibiaTimeOfDayChanged, this, &TMinimapController::updateTimeDisplay);
    }
    updateTimeDisplay();
}

void TMinimapController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_area.setOptionsSource(options);
    refresh();
}

void TMinimapController::setPanelItem(QQuickItem* panel)
{
    if (m_minimap) {
        disconnect(m_minimap, nullptr, this, nullptr);
        m_minimap->setRenderProvider(nullptr);
    }
    m_panel = panel;
    m_minimap = panel ? panel->findChild<qmlcomponents::TMinimapQuickItem*>(QStringLiteral("minimap")) : nullptr;
    m_timeDisplay = panel ? panel->findChild<QQuickItem*>(QStringLiteral("timedisplay")) : nullptr;
    if (m_minimap) {
        connect(m_minimap, &QQuickItem::widthChanged, this, &TMinimapController::updateMinimapVisibleAreaFromQuickitemSize);
        connect(m_minimap, &QQuickItem::heightChanged, this, &TMinimapController::updateMinimapVisibleAreaFromQuickitemSize);
        m_minimap->setRenderProvider(m_renderProvider.get());
    }
    updateMinimapVisibleAreaFromQuickitemSize();
    updateTimeDisplay();
}

int TMinimapController::currentLayer() const
{
    return m_area.z();
}

// CIP .
void TMinimapController::setCurrentLayer(const int layer)
{
    if (m_area.z() == layer)
        return;
    m_area.setZ(layer);
    refresh();
    Q_EMIT currentLayerChanged();
}

void TMinimapController::updateMinimapVisibleAreaFromQuickitemSize()
{
    if (m_minimap)
        m_area.setViewportSizePx(static_cast<int>(m_minimap->width()), static_cast<int>(m_minimap->height()));
    refresh();
}

void TMinimapController::onMinimapDataChanged()
{
    refresh();
}

void TMinimapController::onZoomOutButtonClicked()
{
    m_area.zoomOut();
    refresh();
}

void TMinimapController::onZoomInButtonClicked()
{
    m_area.zoomIn();
    refresh();
}

void TMinimapController::onMoveUpButtonClicked()
{
    m_area.moveZ(-1);
    refresh();
}

void TMinimapController::onMoveDownButtonClicked()
{
    m_area.moveZ(1);
    refresh();
}

void TMinimapController::onCenterButtonClicked()
{
    centerOnPlayer();
}

qmlcomponents::TMinimapQuickItem* TMinimapController::minimapItem() const { return m_minimap; }
minimap::TMinimapTileStorage* TMinimapController::tileStorage() const { return m_storage; }

void TMinimapController::onCyclopediaMapButtonClicked()
{
    // The cyclopedia's map tab, at the player's field.
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(4);
    if (tibia::game::hasLocalPlayer()) {
        m_playerPosition = tibia::game::localPlayerPosition();
        action->setCoordinate(m_playerPosition);
    }
    input::publish(action);
}

// The rose's centre recentres on the player, its arms scroll, from the last
// mouse position on the rose (normalised to 0..1; unset until known, and
// then nothing scrolls).
void TMinimapController::emulateClickOnAutomapRose()
{
    const double nx = m_roseMouseNormalized.x();
    const double ny = m_roseMouseNormalized.y();
    const double sx = nx + nx - 1.0;
    const double sy = ny + ny - 1.0;
    if (std::sqrt(sx * sx + sy * sy) < double(0.33f)) {
        centerOnPlayer();
        refresh();
        return;
    }
    const int step = scrollStep();
    if (nx >= double(0.66f))
        m_area.moveX(step);
    else if (nx <= double(0.33f))
        m_area.moveX(-step);
    if (ny >= double(0.66f))
        m_area.moveY(step);
    else if (ny <= double(0.33f))
        m_area.moveY(-step);
    refresh();
}

// The rose's centre recentres on the player, its arms scroll.
void TMinimapController::onMinimapRoseButtonClicked(const int offsetX, const int offsetY)
{
    if (offsetX == 0 && offsetY == 0) {
        centerOnPlayer();
    } else {
        const int step = scrollStep();
        if (offsetX != 0)
            m_area.moveX(offsetX * step);
        if (offsetY != 0)
            m_area.moveY(step * offsetY);
    }
    refresh();
}

// Item pixels to fields, carrying the fraction to the next drag.
void TMinimapController::onMinimapDragged(const float deltaX, const float deltaY)
{
    const float scale = m_area.scale();
    const float x = m_dragRemainderX + deltaX / scale;
    const float y = m_dragRemainderY + deltaY / scale;
    const float fieldsX = std::round(x);
    const float fieldsY = std::round(y);
    m_dragRemainderX = x - fieldsX;
    m_dragRemainderY = y - fieldsY;
    m_area.moveX(static_cast<int>(fieldsX));
    m_area.moveY(static_cast<int>(fieldsY));
    refresh();
}

void TMinimapController::dispatchClick(const input::TGameActionList& actions, const int mouseButton, const int modifiers,
                                       const std::span<const input::EGameAction> contextMenuOrder)
{
    const input::IGameActionPtr action = m_mapper.map(actions, input::TGameActionMapper::MinimapContext, static_cast<Qt::MouseButton>(mouseButton), modifiers);
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        input::publish(action);
        return;
    }
    const input::TGameActionList entries = input::contextMenuActions(actions, contextMenuOrder);
    if (entries.empty())
        return;
    if (TContextMenuController* menu = TContextMenuController::instance())
        menu->showGameActions(entries, input::TGameActionMapper::MinimapContext, m_mapper);
}

void TMinimapController::onMinimapClick(const int x, const int y, const int triggeringMouseButton, const Qt::KeyboardModifiers modifiers)
{
    m_mouse = QPoint(x, y);
    dispatchClick(m_actions.actionsAt(m_area.itemToWorld(m_mouse)), triggeringMouseButton, static_cast<int>(modifiers),
                  FieldContextMenuOrder);
}

void TMinimapController::onMinimapMarkerClick(QObject* /*pMinimapMarkerQmlRenderInfo*/, const int TriggeringMouseButton,
                                              const Qt::KeyboardModifiers Modifiers)
{
    // There is no marker overlay that reports a hovered marker, so this
    // takes the field under the mouse.
    dispatchClick(actionsAtMouse(), TriggeringMouseButton, static_cast<int>(Modifiers), MarkerContextMenuOrder);
}

void TMinimapController::onMinimapMouseMoved(const int x, const int y)
{
    m_mouse = QPoint(x, y);
    updateMouseCursor();
}

// CIP : the shape a left click would show, on the minimap layer.
void TMinimapController::updateMouseCursor()
{
    // CIP : stop hover candidates while a modal dialog is open.
    if (TDialogStackController* dialogs = TDialogStackController::instance(); dialogs && !dialogs->isEmpty()) {
        if (auto* cursor = Cursor::instance())
            cursor->setLayerShape(Cursor::Minimap, Cursor::Default);
        return;
    }
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::IGameActionPtr action = m_mapper.map(actionsAtMouse(), input::TGameActionMapper::MinimapContext, Qt::LeftButton, modifiers);
    const int shape = action ? worldmap::TWorldMapGameActionProvider::cursorShapeOf(action) : Cursor::Default;
    if (auto* cursor = Cursor::instance())
        cursor->setLayerShape(Cursor::Minimap, shape);
}

// CIP .
void TMinimapController::updateTimeDisplay()
{
    if (!m_timeDisplay || !m_ambientLight)
        return;
    m_timeOfDayNormalized = m_ambientLight->normalizedTibiaTimeOfDay();
    Q_EMIT timeOfDayChanged();
}

void TMinimapController::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case A::MINIMAP_ONE_FLOOR_UP:
        m_area.moveZ(-1);
        break;
    case A::MINIMAP_ONE_FLOOR_DOWN:
        m_area.moveZ(1);
        break;
    case A::MINIMAP_ZOOM_IN:
        m_area.zoomIn();
        break;
    case A::MINIMAP_ZOOM_OUT:
        m_area.zoomOut();
        break;
    case A::MINIMAP_SCROLL_NORTH:
        m_area.moveY(-scrollStep());
        break;
    case A::MINIMAP_SCROLL_EAST:
        m_area.moveX(scrollStep());
        break;
    case A::MINIMAP_SCROLL_SOUTH:
        m_area.moveY(scrollStep());
        break;
    case A::MINIMAP_SCROLL_WEST:
        m_area.moveX(-scrollStep());
        break;
    case A::MINIMAP_CENTER:
        centerOnPlayer();
        return;
    default:
        return;
    }
    refresh();
}

void TMinimapController::refresh()
{
    if (m_storage)
        m_storage->setVisibleArea(m_area.topLeft(), m_area.size());

    bool crosshairVisible = false;
    if (tibia::game::hasLocalPlayer() && m_minimap) {
        m_playerPosition = tibia::game::localPlayerPosition();
        if (m_playerPosition.z == m_area.topLeft().z) {
            const QPoint crosshair = m_area.worldToItem(m_playerPosition.x, m_playerPosition.y);
            m_minimap->setProperty("crosshairX", crosshair.x());
            m_minimap->setProperty("crosshairY", crosshair.y());
            m_minimap->setProperty("crosshairIsDark", crosshairIsDark());
            crosshairVisible = true;
        }
    }
    if (m_minimap) {
        m_minimap->setProperty("crosshairVisible", crosshairVisible);
        m_minimap->update();
    }
    updateMouseCursor();
    Q_EMIT currentLayerChanged();
}

// CIP : the area follows the player.
void TMinimapController::onPlayerPositionChanged(const Position& position)
{
    if (m_area.center() == position)
        return;
    m_playerPosition = position;
    m_area.setCenter(position);
    refresh();
}

// CIP .
void TMinimapController::centerOnPlayer()
{
    if (!tibia::game::hasLocalPlayer())
        return;
    m_playerPosition = tibia::game::localPlayerPosition();
    if (m_area.center() != m_playerPosition)
        m_area.setCenter(m_playerPosition);
    refresh();
}

int TMinimapController::scrollStep() const
{
    return roundToInt(12.0f / m_area.scale());
}

bool TMinimapController::crosshairIsDark() const
{
    if (!m_storage)
        return false;
    const minimap::TMinimapTilePtr tile = m_storage->tileAt(m_playerPosition);
    return tile && tile->colorAt(m_playerPosition).blue() == 0xff;
}

input::TGameActionList TMinimapController::actionsAtMouse() const
{
    return m_actions.actionsAt(m_area.itemToWorld(m_mouse));
}

}
