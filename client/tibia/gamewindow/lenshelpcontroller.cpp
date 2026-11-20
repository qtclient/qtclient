#include "lenshelpcontroller.h"

#include <QDebug>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>

namespace tibia::gamewindow {

namespace {

/// The space kept free along the parent's edges.
constexpr qreal BoundsMargin = 5;

/// `rect` lies inside `bounds`.
bool contains(const QRectF& bounds, const QRectF& rect)
{
    return (bounds & rect) == rect;
}

}

TLenshelpController::TLenshelpController(QQmlEngine* engine, QQuickItem* parentItem, QObject* parent)
    : cip::tibia::gamewindow::TLenshelpController(parent)
    , m_engine(engine)
    , m_parentItem(parentItem)
{}

TLenshelpController::~TLenshelpController()
{
    hide();
    destroyPopup();
}

void TLenshelpController::createPopup()
{
    if (m_popup || !m_engine)
        return;
    QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/LenshelpTemplate.qml")));
    auto* item = qobject_cast<QQuickItem*>(component.create());
    if (!item) {
        qCritical() << "Component creation error for " << component.url().url() << "\n" << component.errors();
        return;
    }
    QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
    item->setParentItem(m_parentItem);
    item->setProperty("controller", QVariant::fromValue<QObject*>(this));
    m_popup = item;
    connect(item, &QQuickItem::heightChanged, this, &TLenshelpController::updatePosition);
    connect(item, &QQuickItem::widthChanged, this, &TLenshelpController::updatePosition);
}

void TLenshelpController::destroyPopup()
{
    if (!m_popup)
        return;
    m_popup->deleteLater();
    m_popup = nullptr;
}

void TLenshelpController::show(const QString& caption, const QString& content, const QRectF& triggerRect)
{
    m_caption = caption;
    m_content = content;
    Q_EMIT contentChanged();
    if (!m_lenshelpVisible) {
        m_lenshelpVisible = true;
        Q_EMIT lenshelpVisibleChanged();
    }
    m_triggerRect = triggerRect;
    updatePosition();
}

void TLenshelpController::hide()
{
    if (m_lenshelpVisible) {
        m_lenshelpVisible = false;
        Q_EMIT lenshelpVisibleChanged();
    }
    m_caption.clear();
    m_content.clear();
    Q_EMIT contentChanged();
}

void TLenshelpController::updatePosition()
{
    if (!m_popup || !m_parentItem)
        return;
    QRectF bounds = m_parentItem->mapRectToScene(m_parentItem->boundingRect());
    bounds.adjust(BoundsMargin, BoundsMargin, -BoundsMargin, -BoundsMargin);
    const QPointF position = popupPosition(m_popup->boundingRect().size(), bounds, m_triggerRect);
    if (m_popup->position() == position)
        return;
    m_popup->setPosition(m_parentItem->mapFromScene(position));
}

QRectF TLenshelpController::clampedInto(const QRectF& bounds, const QRectF& rect)
{
    if ((rect & bounds) == rect)
        return rect;
    qreal x = rect.x();
    qreal width = rect.width();
    const qreal left = bounds.x();
    if (left > x) {
        width -= left - x;
        x = left;
    }
    if (x + width < left)
        width = left - x;
    const qreal right = left + bounds.width();
    if (right < x) {
        const qreal shift = right - x;
        x += shift;
        width -= shift;
    }
    if (right < x + width)
        width = right - x;

    qreal y = rect.y();
    qreal height = rect.height();
    const qreal top = bounds.y();
    if (top > y) {
        height -= top - y;
        y = top;
    }
    if (y + height < top)
        height = top - y;
    const qreal bottom = top + bounds.height();
    if (bottom < y) {
        const qreal shift = bottom - y;
        y += shift;
        height -= shift;
    }
    if (bottom < y + height)
        height = bottom - y;
    return { x, y, width, height };
}

QPointF TLenshelpController::popupPosition(const QSizeF& size, const QRectF& bounds, const QRectF& triggerRect)
{
    const QRectF trigger = clampedInto(bounds, triggerRect);
    const qreal width = size.width();
    const qreal height = size.height();
    const qreal left = trigger.x();
    const qreal right = trigger.x() + trigger.width();
    const qreal top = trigger.y();
    const qreal bottom = trigger.y() + trigger.height();
    const auto fits = [&](const QPointF& at) { return contains(bounds, QRectF(at, size)); };

    const QPointF candidates[] = {
        { right, top },          { right, bottom - height },        // right of the trigger
        { left - width, top },   { left - width, bottom - height }, // left of it
        { left, top - height },  { right - width, top - height },   // above
        { left, bottom },        { right - width, bottom },         // below
    };
    for (const QPointF& at : candidates) {
        if (fits(at))
            return at.toPoint();
    }
    // Along the bounds' edges.
    if (width < bounds.x() + bounds.width() - right) {
        if (const QPointF at(right, bounds.y()); fits(at))
            return at.toPoint();
    }
    if (width < left - bounds.x()) {
        if (const QPointF at(left - width, bounds.y()); fits(at))
            return at.toPoint();
    }
    if (height < bounds.y() + bounds.height() - bottom) {
        if (const QPointF at(bounds.x() + bounds.width() - width, bottom); fits(at))
            return at.toPoint();
    }
    return bounds.topLeft().toPoint();
}

}
