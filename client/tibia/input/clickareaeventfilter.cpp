#include "clickareaeventfilter.h"

#include <QQuickItem>
#include <QSinglePointEvent>

namespace tibia::input {

TClickAreaEventFilter::TClickAreaEventFilter(QObject* parent)
    : cip::tibia::input::TClickAreaEventFilter(parent)
{}

void TClickAreaEventFilter::setClickArea(QQuickItem* item)
{
    m_area = item;
}

bool TClickAreaEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
        break;
    default:
        return QObject::eventFilter(watched, event);
    }
    if (!m_area)
        return false;
    const QRectF area(m_area->mapToScene(QPointF()), m_area->boundingRect().size());
    if (area.contains(static_cast<const QSinglePointEvent*>(event)->scenePosition()))
        Q_EMIT clickedInside();
    else
        Q_EMIT clickedOutside();
    return false;
}

}
