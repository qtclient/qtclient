#include "draganddropcontroller.h"

#include "gamewindow/mousecursorcontroller.h"

#include <client/item.h>
#include <tibia/world/thing.h>

#include <QCoreApplication>
#include <QCursor>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

/// TibiaStyle.dragKeyMoveTibiaObject, the drop areas' key.
const QString MoveObjectMimeType = QStringLiteral("application/x-move-tibia-object");

/// The icon sits below and right of the mouse.
constexpr QPointF IconOffset(10, 10);

QPointer<TDragAndDropController> s_instance;

bool acceptsMoveObject(const QQuickItem* area)
{
    const QStringList keys = area->property("keys").toStringList();
    return keys.isEmpty() || keys.contains(MoveObjectMimeType);
}

/// CIP 0x1010a7be0: the topmost visible, enabled DropArea at the point, taking
/// higher z first and not looking outside clipping items.
QQuickItem* dropAreaAt(const QList<QQuickItem*>& items, const QPointF& scenePosition)
{
    QList<QQuickItem*> sorted = items;
    std::stable_sort(sorted.begin(), sorted.end(), [](const QQuickItem* a, const QQuickItem* b) { return a->z() < b->z(); });
    for (auto it = sorted.crbegin(); it != sorted.crend(); ++it) {
        QQuickItem* item = *it;
        if (!item->isVisible() || !item->isEnabled())
            continue;
        const QPointF local = item->mapFromScene(scenePosition);
        if (item->clip() && !item->boundingRect().contains(local))
            continue;
        if (const QList<QQuickItem*> children = item->childItems(); !children.isEmpty()) {
            if (QQuickItem* found = dropAreaAt(children, scenePosition); found && acceptsMoveObject(found))
                return found;
        }
        if (item->contains(local) && item->inherits("QQuickDropArea") && acceptsMoveObject(item))
            return item;
    }
    return nullptr;
}

}

TDragAndDropControllerEventFilter::TDragAndDropControllerEventFilter(QQuickItem* root, QObject* parent)
    : cip::tibia::gamewindow::TDragAndDropControllerEventFilter(parent)
    , m_root(root)
    , m_mimeData(new QMimeData)
{
    m_mimeData->setData(MoveObjectMimeType, {});
}

TDragAndDropControllerEventFilter::~TDragAndDropControllerEventFilter()
{
    delete m_mimeData;
}

void TDragAndDropControllerEventFilter::leaveTarget()
{
    if (m_target)
        QCoreApplication::postEvent(m_target, new QDragLeaveEvent);
    m_target = nullptr;
}

void TDragAndDropControllerEventFilter::moveTo(const QPointF& scenePosition, const Qt::KeyboardModifiers modifiers, const bool drop)
{
    QQuickItem* area = m_root ? dropAreaAt(m_root->childItems(), scenePosition) : nullptr;
    if (m_target && m_target != area)
        leaveTarget();
    if (!area)
        return;

    const QPoint local = area->mapFromScene(scenePosition).toPoint();
    if (m_target == area)
        QCoreApplication::postEvent(area, new QDragMoveEvent(local, Qt::MoveAction, m_mimeData, Qt::LeftButton, modifiers));
    else
        QCoreApplication::postEvent(area, new QDragEnterEvent(local, Qt::MoveAction, m_mimeData, Qt::LeftButton, modifiers));
    m_target = area;

    if (drop) {
        Q_EMIT dragAndDropConfirmed(modifiers);
        QCoreApplication::postEvent(area, new QDropEvent(QPointF(local), Qt::MoveAction, m_mimeData, Qt::LeftButton, modifiers));
        m_target = nullptr;
    }
}

bool TDragAndDropControllerEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    // Mouse and key events reach the window first, then its items; only the
    // window's copy counts.
    if (event->type() == QEvent::ApplicationStateChange) {
        leaveTarget();
        Q_EMIT dragAndDropEventFilterFinished();
        return false;
    }
    if (!qobject_cast<QQuickWindow*>(watched))
        return false;

    Q_EMIT dragAndDropMouseCursorPositionChanged();
    switch (event->type()) {
    case QEvent::MouseMove: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        moveTo(mouse->scenePosition(), mouse->modifiers(), false);
        break;
    }
    case QEvent::MouseButtonRelease: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton)
            moveTo(mouse->scenePosition(), mouse->modifiers(), true);
        leaveTarget();
        Q_EMIT dragAndDropEventFilterFinished();
        break;
    }
    case QEvent::MouseButtonPress:
        leaveTarget();
        Q_EMIT dragAndDropEventFilterFinished();
        break;
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            leaveTarget();
            Q_EMIT dragAndDropEventFilterFinished();
        }
        break;
    default:
        break;
    }
    return false;
}

TDragAndDropController::TDragAndDropController(QQuickItem* root, QQuickItem* icon, QObject* parent)
    : cip::tibia::gamewindow::TDragAndDropController(parent)
    , m_root(root)
    , m_icon(icon)
    , m_filter(new TDragAndDropControllerEventFilter(root, this))
{
    s_instance = this;
    connect(m_filter, &TDragAndDropControllerEventFilter::dragAndDropEventFilterFinished, this, &TDragAndDropController::finish);
    connect(m_filter, &TDragAndDropControllerEventFilter::dragAndDropConfirmed, this,
            [this](const Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; });
    connect(m_filter, &TDragAndDropControllerEventFilter::dragAndDropMouseCursorPositionChanged, this, &TDragAndDropController::moveIcon);
    if (m_icon)
        m_icon->setVisible(false);
}

TDragAndDropController::~TDragAndDropController()
{
    if (m_filterInstalled)
        QCoreApplication::instance()->removeEventFilter(m_filter);
    if (s_instance == this)
        s_instance = nullptr;
}

TDragAndDropController* TDragAndDropController::instance()
{
    return s_instance;
}

void TDragAndDropController::startDrag(const input::TGameActionPtr& action, QObject* source)
{
    if (!action)
        return;
    m_action = action;
    m_source = qobject_cast<QQuickItem*>(source);
    updateIcon(action->object);
    if (m_icon)
        m_icon->setVisible(true);
    moveIcon();
    QCoreApplication::instance()->installEventFilter(m_filter);
    m_filterInstalled = true;
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setDragging(true);
}

input::TGameActionPtr TDragAndDropController::takeAction()
{
    input::TGameActionPtr action = std::move(m_action);
    m_action.reset();
    if (m_source) {
        m_source->ungrabMouse();
        m_source = nullptr;
    }
    return action;
}

void TDragAndDropController::finish()
{
    QCoreApplication::instance()->removeEventFilter(m_filter);
    m_filterInstalled = false;
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setDragging(false);
    if (m_source) {
        m_source->ungrabMouse();
        m_source = nullptr;
    }
    updateIcon(nullptr);
    if (m_icon)
        m_icon->setVisible(false);
}

void TDragAndDropController::moveIcon()
{
    if (!m_icon || !m_icon->window())
        return;
    const QPointF scene = QPointF(m_icon->window()->mapFromGlobal(QCursor::pos())) + IconOffset;
    QQuickItem* parent = m_icon->parentItem();
    m_icon->setPosition(parent ? parent->mapFromScene(scene) : scene);
}

void TDragAndDropController::updateIcon(const ThingPtr& object)
{
    if (!m_icon)
        return;
    const ItemPtr item = object && object->isItem() ? object->static_self_cast<Item>() : nullptr;
    const bool liquid = item && (item->isFluidContainer() || item->isSplash());
    m_icon->setProperty("typeid", item ? static_cast<int>(item->getId()) : 0);
    m_icon->setProperty("cumulativeCount", item && item->isStackable() ? static_cast<int>(item->getCountOrSubType()) : 0);
    m_icon->setProperty("liquidType", QVariant::fromValue(static_cast<Info::ELiquidType>(liquid ? item->getCountOrSubType() : 0)));
    m_icon->setProperty("hookDirection", QVariant::fromValue(Info::EHookDirection::NONE));
}

}
