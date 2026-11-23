#include "minimap/minimapmarkercontroller.h"

#include "minimap/minimapmarkerqmlrenderinfo.h"
#include "qmlcomponents/qmlincubatorwithcallbackonready.h"

#include <QQuickItem>
#include <QVariant>

namespace tibia::minimap {

TMinimapMarkerController::TMinimapMarkerController(
    const std::shared_ptr<qmlcomponents::TQmlIncubatorWithCallbackOnReady>& incubator, QQuickItem* parentItem)
    : cip::tibia::minimap::TMinimapMarkerController(nullptr)
    , m_incubator(incubator)
    , m_parentItem(parentItem)
    , m_renderInfo(std::make_shared<TMinimapMarkerQmlRenderInfo>())
{
    if (m_incubator->isReady())
        update();
    else
        m_incubator->m_CallbackOnReady = [this] { update(); };
}

TMinimapMarkerController::~TMinimapMarkerController()
{
    if (m_incubator) {
        m_incubator->clear();
        m_incubator.reset();
    }
    if (m_item) {
        m_item->setVisible(false);
        m_item->deleteLater();
        m_item = nullptr;
    }
}

void TMinimapMarkerController::setMarker(const TMinimapMarker& marker)
{
    m_renderInfo->setMarker(marker);
}

void TMinimapMarkerController::setPosition(const QPoint& position)
{
    m_position = position;
}

void TMinimapMarkerController::update()
{
    if (!m_item) {
        if (m_incubator && m_incubator->isReady()) {
            m_item = qobject_cast<QQuickItem*>(m_incubator->object());
            m_incubator.reset();
            if (m_item)
                setItem(m_item);
        }
        if (!m_item)
            return;
    }
    const QPointF position(m_position.x() - 6, m_position.y() - 6);
    if (m_item->position() != position)
        m_item->setPosition(position);
}

void TMinimapMarkerController::setItem(QQuickItem* item)
{
    m_item = item;
    m_item->setProperty("minimapMarkerInfo", QVariant::fromValue(static_cast<QObject*>(m_renderInfo.get())));
    m_item->setParentItem(m_parentItem);
}

}
