#include "draganddropcontroller.h"

#include "game/thingsession_forward.h"
#include "input/gameactioncontext.h"
#include "protocol/objectdata.h"
#include "qmlcomponents/appearanceinstancerenderer.h"
#include "gamewindow/mousecursorcontroller.h"

#include <client/position.h>
#include "protocol/protocolcodes.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

/// TibiaStyle.dragKeyMoveTibiaObject, the drop areas' key.
const QString MoveObjectMimeType = QStringLiteral("application/x-move-tibia-object");

QPointer<TDragAndDropController> s_instance;

/// Whether a drop area's `keys` take one of `keys`; an area without keys
/// takes everything.
bool acceptsKeys(const QQuickItem* area, const QStringList& keys)
{
    const QStringList areaKeys = area->property("keys").toStringList();
    return areaKeys.isEmpty() || std::ranges::any_of(keys, [&areaKeys](const QString& key) { return areaKeys.contains(key); });
}

/// Searches `items` from the highest z down: a visible, enabled item whose
/// clip does not exclude the point is searched below first (a result there
/// counts only when it accepts `keys`), then is itself the result when it
/// contains the point and is a `className`.
QQuickItem* findDropAreaIn(const QList<QQuickItem*>& items, const QPointF& scenePosition, const char* className, const QStringList& keys)
{
    QList<QQuickItem*> sorted = items;
    std::stable_sort(sorted.begin(), sorted.end(), [](const QQuickItem* a, const QQuickItem* b) { return a->z() < b->z(); });
    for (auto it = sorted.crbegin(); it != sorted.crend(); ++it) {
        QQuickItem* item = *it;
        if (!item->isVisible() || !item->isEnabled())
            continue;
        if (item->clip() && !item->boundingRect().contains(item->mapFromScene(scenePosition)))
            continue;
        if (!item->childItems().isEmpty()) {
            if (QQuickItem* found = findDropAreaIn(item->childItems(), scenePosition, className, keys); found && acceptsKeys(found, keys))
                return found;
        }
        if (item->contains(item->mapFromScene(scenePosition)) && item->inherits(className))
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

QQuickItem* TDragAndDropControllerEventFilter::findDropArea(const QPointF& scenePosition, QQuickItem* root, const QStringList& keys)
{
    return root ? findDropAreaIn(root->childItems(), scenePosition, "QQuickDropArea", keys) : nullptr;
}

void TDragAndDropControllerEventFilter::onLastFoundDropAreaDestroyed() { m_lastFoundDropArea = nullptr; }

void TDragAndDropControllerEventFilter::forgetLastFoundDropArea()
{
    if (!m_lastFoundDropArea)
        return;
    disconnect(m_lastFoundDropArea, &QObject::destroyed, this, &TDragAndDropControllerEventFilter::onLastFoundDropAreaDestroyed);
    m_lastFoundDropArea = nullptr;
}

void TDragAndDropControllerEventFilter::leaveLastFoundDropArea()
{
    if (!m_lastFoundDropArea)
        return;
    QCoreApplication::postEvent(m_lastFoundDropArea, new QDragLeaveEvent);
    forgetLastFoundDropArea();
}

bool TDragAndDropControllerEventFilter::eventFilter(QObject* /*watched*/, QEvent* event)
{
    Q_EMIT dragAndDropMouseCursorPositionChanged();
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        break;
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        const bool release = event->type() == QEvent::MouseButtonRelease;
        if (release && mouse->button() != Qt::LeftButton)
            break;
        QQuickItem* area = findDropArea(mouse->scenePosition(), m_root, { MoveObjectMimeType });
        if (m_lastFoundDropArea && m_lastFoundDropArea != area)
            leaveLastFoundDropArea();
        if (area) {
            const QPointF mapped = area->mapFromScene(mouse->scenePosition());
            const QPoint local(int(mapped.x()), int(mapped.y()));
            QEvent* drag = m_lastFoundDropArea == area
                               ? static_cast<QEvent*>(new QDragMoveEvent(local, Qt::MoveAction, m_mimeData, Qt::LeftButton, mouse->modifiers()))
                               : new QDragEnterEvent(local, Qt::MoveAction, m_mimeData, Qt::LeftButton, mouse->modifiers());
            QCoreApplication::postEvent(area, drag);
            m_lastFoundDropArea = area;
            connect(area, &QObject::destroyed, this, &TDragAndDropControllerEventFilter::onLastFoundDropAreaDestroyed);
            if (release) {
                Q_EMIT dragAndDropConfirmed(mouse->modifiers());
                QCoreApplication::postEvent(area, new QDropEvent(QPointF(local), Qt::MoveAction, m_mimeData, Qt::LeftButton, mouse->modifiers()));
                forgetLastFoundDropArea();
            }
        }
        if (!release)
            return false;
        break;
    }
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent*>(event)->key() != Qt::Key_Escape)
            return false;
        break;
    case QEvent::ApplicationStateChange:
        break;
    default:
        return false;
    }
    leaveLastFoundDropArea();
    Q_EMIT dragAndDropEventFilterFinished();
    return false;
}

TDragAndDropController::TDragAndDropController(QQuickItem* root, QQuickItem* icon, QObject* parent)
    : cip::tibia::gamewindow::TDragAndDropController(parent)
    , m_root(root)
    , m_icon(icon)
    , m_filter(new TDragAndDropControllerEventFilter(root, this))
{
    s_instance = this;
    connect(m_filter, &TDragAndDropControllerEventFilter::dragAndDropEventFilterFinished, this,
            &TDragAndDropController::onDragAndDropEventFilterFinished);
    connect(m_filter, &TDragAndDropControllerEventFilter::dragAndDropConfirmed, this, &TDragAndDropController::onDragAndDropConfirmed);
    connect(m_filter, &TDragAndDropControllerEventFilter::dragAndDropMouseCursorPositionChanged, this,
            &TDragAndDropController::onDragAndDropMousePositionChanged);
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

void TDragAndDropController::registerContextProperty(QQmlContext* context)
{
    context->setContextProperty(QStringLiteral("tibiaDragAndDropController"), this);
}

void TDragAndDropController::startDrag(const input::TMoveObjectPtr& action, QObject* source)
{
    if (!action)
        return;
    m_action = action;
    m_source = qobject_cast<QQuickItem*>(source);
    const input::TObjectGameActionContext& object = action->context();
    const uint32_t creatureId = input::creatureOfObject(object);
    updateIcon();
    // CipSoft dragApperanceInstanceIcon is a SingleObject item renderer — creatures
    // have no item art under the cursor (looktype-as-typeid would show a random item).
    const bool creatureDrag = creatureId != 0;
    if (m_icon)
        m_icon->setVisible(!creatureDrag);
    onDragAndDropMousePositionChanged();
    QCoreApplication::instance()->installEventFilter(m_filter);
    m_filterInstalled = true;
    // A drag shows the target-selection crosshair until it finishes.
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setTargetSelection(true);
}

input::TMoveObjectPtr TDragAndDropController::takeAction()
{
    input::TMoveObjectPtr action = std::move(m_action);
    m_action.reset();
    if (m_source) {
        m_source->ungrabMouse();
        m_source = nullptr;
    }
    return action;
}

void TDragAndDropController::onDragAndDropConfirmed(const Qt::KeyboardModifiers KeyboardModifiers) { m_modifiers = KeyboardModifiers; }

void TDragAndDropController::onDragAndDropEventFilterFinished()
{
    QCoreApplication::instance()->removeEventFilter(m_filter);
    m_filterInstalled = false;
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setTargetSelection(false);
    if (m_source) {
        m_source->ungrabMouse();
        m_source = nullptr;
    }
    clearIcon();
}

void TDragAndDropController::onDragAndDropMousePositionChanged()
{
    if (!m_icon || !m_icon->window())
        return;
    m_icon->setPosition(m_icon->window()->mapFromGlobal(QCursor::pos()));
}

void TDragAndDropController::updateIcon()
{
    using input::EObjectApperanceTypeContextType;
    using protocol::TObjectData;
    m_draggedObjectInstance.reset();
    const auto* object = m_action ? dynamic_cast<const input::TObjectApperanceTypeGameActionContext*>(m_action->object().get()) : nullptr;
    if (object) {
        const uint16_t typeId = object->appearanceTypeId();
        TObjectData data;
        switch (object->objectApperanceTypeContextType()) {
        case EObjectApperanceTypeContextType::LIQUID:
            data = TObjectData::liquid(typeId, static_cast<const input::TLiquidObjectApperanceTypeGameActionContext*>(object)->liquidType());
            break;
        case EObjectApperanceTypeContextType::UPGRADEABLE:
            data = TObjectData::upgradeable(typeId, static_cast<const input::TUpgradeableObjectApperanceTypeGameActionContext*>(object)->tier());
            break;
        default: {
            const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
            if (type && type->isStackable()) {
                data = TObjectData::cumulative(typeId, 1);
            } else if (type && type->isDecoKit()) {
                const auto* decoItemKit = dynamic_cast<const input::TDecoItemKitObjectApperanceTypeGameActionContext*>(object);
                data = TObjectData::decoItemKit(typeId, decoItemKit ? decoItemKit->decoItemId() : 0);
            } else {
                data = TObjectData::plain(typeId);
            }
            break;
        }
        }
        m_draggedObjectInstance = appearances::TAppearanceInstanceFactory::createObject(data);
    }
    applyIconProperties();
    if (m_icon)
        m_icon->setVisible(true);
}

void TDragAndDropController::applyIconProperties()
{
    if (!m_icon)
        return;
    using qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
    m_icon->setProperty("typeid", TObjectAppearanceInstanceInfoQmlType::getQmlTypeId(m_draggedObjectInstance));
    m_icon->setProperty("cumulativeCount", TObjectAppearanceInstanceInfoQmlType::getQmlCumulativeCount(m_draggedObjectInstance));
    m_icon->setProperty("liquidType", QVariant::fromValue(TObjectAppearanceInstanceInfoQmlType::getQmlLiquidType(m_draggedObjectInstance)));
    m_icon->setProperty("hookDirection",
                        QVariant::fromValue(TObjectAppearanceInstanceInfoQmlType::getQmlHookDirection(m_draggedObjectInstance)));
}

void TDragAndDropController::clearIcon()
{
    m_draggedObjectInstance.reset();
    applyIconProperties();
    if (m_icon)
        m_icon->setVisible(false);
}

}
