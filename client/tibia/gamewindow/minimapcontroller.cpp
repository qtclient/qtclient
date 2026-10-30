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

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QGuiApplication>
#include <QQuickItem>

#include <cmath>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Cursor = TMouseCursorController;

/// Soft minimap field menu order (walk / marks). GOD_TELEPORT is Shift+Ctrl only.
constexpr A SoftMinimapOrder[] = {
    A::SET_PLAYER_MOVEMENT_INTENT,
    A::CONTEXT_MENU_SEPARATOR,
    A::SHOW_DIALOG_CREATE_MINIMAP_MARKER,
    A::SHOW_DIALOG_EDIT_MINIMAP_MARKER,
    A::DELETE_MINIMAP_MARKER,
};

/// CIP's float rounding: half away from zero.
int roundAway(const float v)
{
    return static_cast<int>(std::trunc(v + std::copysign(0.5f, v)));
}

void showMinimapContextMenu(const input::TGameActionList& candidates, const input::TGameActionMapper& mapper)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    if (const input::TGameActionList entries = input::contextMenuActions(candidates, SoftMinimapOrder); !entries.empty())
        menu->showGameActions(entries, input::TGameActionMapper::MinimapContext, mapper);
}

}

TMinimapController::TMinimapController(QObject* parent)
    : cip::tibia::gamewindow::TMinimapController(parent)
{
    // CIP: Thais temple until the player is known.
    m_area.setCenter(Position(32369, 32215, 7));

    m_positionHook = client::gamecore::Hook("object", "onPositionChange", [this](LuaObject* self, const std::vector<std::any>& args) {
        const LocalPlayerPtr& player = g_game.getLocalPlayer();
        if (player && self == player.get() && !args.empty())
            if (const auto* position = std::any_cast<Position>(&args[0]))
                onPlayerPositionChanged(*position);
        return std::any{};
    });
    input::setMinimapActionHandler([this](const input::TGameAction& action) { handleGameAction(action); });
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
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && player->getPosition().isValid())
            onPlayerPositionChanged(player->getPosition());
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

// CIP 0x100b94ff0.
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
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_CYCLOPEDIA));
    action->parameter = 4;
    action->position = m_area.center();
    input::publish(action);
}

// CIP 0x100b94060: the rose's centre recentres, its arms scroll.
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

// CIP 0x100b94170: item pixels to fields, carrying the fraction to the next drag.
void TMinimapController::onMinimapDragged(const float deltaX, const float deltaY)
{
    const float scale = m_area.scale();
    const float x = static_cast<float>(m_dragRemainder.x()) + deltaX / scale;
    const float y = static_cast<float>(m_dragRemainder.y()) + deltaY / scale;
    const int fieldsX = roundAway(x);
    const int fieldsY = roundAway(y);
    m_dragRemainder = QPointF(x - fieldsX, y - fieldsY);
    m_area.moveX(fieldsX);
    m_area.moveY(fieldsY);
    refresh();
}

// CIP 0x100b94510 / 0x100b94660.
void TMinimapController::onMinimapClick(const int x, const int y, const int triggeringMouseButton, const Qt::KeyboardModifiers modifiers)
{
    m_mouse = QPoint(x, y);
    const input::TGameActionList candidates = actionsAtMouse();
    const input::TGameActionPtr action = m_mapper.map(candidates, input::TGameActionMapper::MinimapContext,
                                                      static_cast<Qt::MouseButton>(triggeringMouseButton), static_cast<int>(modifiers));
    if (!action)
        return;
    if (action->type == A::CONTEXT_MENU) {
        showMinimapContextMenu(candidates, m_mapper);
        return;
    }
    input::publish(action);
}

void TMinimapController::onMinimapMarkerClick(QObject* /*pMinimapMarkerQmlRenderInfo*/, const int TriggeringMouseButton,
                                              const Qt::KeyboardModifiers Modifiers)
{
    // Soft: marker overlay clicks use the same Soft field candidates as the tile under the mouse.
    const input::TGameActionList candidates = actionsAtMouse();
    const input::TGameActionPtr action = m_mapper.map(candidates, input::TGameActionMapper::MinimapContext,
                                                      static_cast<Qt::MouseButton>(TriggeringMouseButton), static_cast<int>(Modifiers));
    if (!action)
        return;
    if (action->type == A::CONTEXT_MENU) {
        showMinimapContextMenu(candidates, m_mapper);
        return;
    }
    input::publish(action);
}

void TMinimapController::onMinimapMouseMoved(const int x, const int y)
{
    m_mouse = QPoint(x, y);
    updateMouseCursor();
}

// CIP 0x100b94250: the shape a left click would show, on the minimap layer.
void TMinimapController::updateMouseCursor()
{
    // CIP FUN_100c15470: stop hover candidates while a modal dialog is open.
    if (TDialogStackController* dialogs = TDialogStackController::instance(); dialogs && !dialogs->isEmpty()) {
        if (auto* cursor = Cursor::instance())
            cursor->setLayerShape(Cursor::Minimap, Cursor::Default);
        return;
    }
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::TGameActionPtr action = m_mapper.map(actionsAtMouse(), input::TGameActionMapper::MinimapContext, Qt::LeftButton, modifiers);
    const int shape = action ? worldmap::TWorldMapGameActionProvider::cursorShapeOf(action) : Cursor::Default;
    if (auto* cursor = Cursor::instance())
        cursor->setLayerShape(Cursor::Minimap, shape);
}

// CIP 0x100b93410.
void TMinimapController::updateTimeDisplay()
{
    if (!m_timeDisplay || !m_ambientLight)
        return;
    m_timeOfDayNormalized = m_ambientLight->normalizedTibiaTimeOfDay();
    Q_EMIT timeOfDayChanged();
}

void TMinimapController::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
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
    if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && m_minimap) {
        m_playerPosition = player->getPosition();
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

// CIP 0x100b93ba0: the area follows the player.
void TMinimapController::onPlayerPositionChanged(const Position& position)
{
    if (m_area.center() == position)
        return;
    m_playerPosition = position;
    m_area.setCenter(position);
    refresh();
}

// CIP 0x100b93e40.
void TMinimapController::centerOnPlayer()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player)
        return;
    m_playerPosition = player->getPosition();
    if (m_area.center() != m_playerPosition)
        m_area.setCenter(m_playerPosition);
    refresh();
}

int TMinimapController::scrollStep() const
{
    return roundAway(12.0f / m_area.scale());
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
