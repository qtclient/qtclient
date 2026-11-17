#include "lenshelpquickitem.h"

#include "qmlcomponents/lenshelpmanager.h"

namespace tibia::qmlcomponents {

TLenshelpQuickItem::TLenshelpQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TLenshelpQuickItem(parent)
{
    setAcceptHoverEvents(true);
    // In lenshelp mode the overlay takes every click.
    setAcceptedMouseButtons(Qt::AllButtons);
    setEnabled(TLenshelpManager::isEnabled());
    setVisible(TLenshelpManager::isEnabled());
    TLenshelpManager::registerItem(this);
}

TLenshelpQuickItem::~TLenshelpQuickItem()
{
    TLenshelpManager::unregisterItem(this);
}

void TLenshelpQuickItem::hoverEnterEvent(QHoverEvent* event)
{
    QQuickItem::hoverEnterEvent(event);
    TLenshelpManager* manager = TLenshelpManager::instance();
    if (!manager)
        return;
    QRectF rect = boundingRect();
    if (m_triggerRect.width() > 0 && m_triggerRect.height() > 0)
        rect = m_triggerRect;
    manager->show(m_caption, m_content, mapRectToScene(rect), this);
}

void TLenshelpQuickItem::hoverLeaveEvent(QHoverEvent* event)
{
    QQuickItem::hoverLeaveEvent(event);
    if (TLenshelpManager* manager = TLenshelpManager::instance())
        manager->hide(this);
}

}
