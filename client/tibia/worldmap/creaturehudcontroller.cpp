#include "worldmap/creaturehudcontroller.h"

#include "worldmap/creaturehudqmlrenderinfo.h"

#include <QQmlIncubator>
#include <QQuickItem>
#include <QVariant>

#include <cmath>

namespace tibia::worldmap {

TCreatureHUDController::TCreatureHUDController(std::shared_ptr<QQmlIncubator> incubator, QQuickItem* parentItem,
                                               QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDController(parent)
    , m_incubator(std::move(incubator))
    , m_parentItem(parentItem)
    , m_info(std::make_shared<TCreatureHUDQmlRenderInfo>())
{
    if (m_incubator && m_incubator->isReady())
        update();
}

TCreatureHUDController::~TCreatureHUDController()
{
    if (m_incubator) {
        m_incubator->clear();
        m_incubator.reset();
    }
    if (m_item) {
        // The item's bindings read the render info until the item is gone.
        QObject::connect(m_item, &QObject::destroyed, [info = m_info] {});
        m_item->setVisible(false);
        m_item->deleteLater();
        m_item = nullptr;
    }
}

void TCreatureHUDController::setRenderInfo(const TCreatureHUDRenderInfo& renderInfo)
{
    m_renderInfo = renderInfo;
}

void TCreatureHUDController::setItem(QQuickItem* item)
{
    m_item = item;
    if (!m_item)
        return;
    m_item->setProperty("creatureHUDInfo", QVariant::fromValue<QObject*>(m_info.get()));
    if (m_item->parentItem() != m_parentItem)
        m_item->setParentItem(m_parentItem);
}

void TCreatureHUDController::setOnTop(const bool onTop)
{
    if (m_onTop == onTop)
        return;
    m_onTop = onTop;
    m_item->setProperty("z", onTop ? 9999 : 0);
}

void TCreatureHUDController::update()
{
    if (!m_item) {
        if (m_incubator && m_incubator->isReady()) {
            QQuickItem* item = qobject_cast<QQuickItem*>(m_incubator->object());
            m_incubator.reset();
            setItem(item);
        }
        if (!m_item)
            return;
    }
    m_info->setRenderInfo(m_renderInfo);
    setOnTop(m_renderInfo.isLocalPlayer);

    // Centred on the field (the HUD is 32 wide), standing on its top edge.
    const QPointF position(std::floor(m_geometry.x() + m_geometry.width() * 0.5 - 16 + 0.5), std::floor(m_geometry.y()));
    if (m_item->position() != position)
        m_item->setPosition(position);
}

}
