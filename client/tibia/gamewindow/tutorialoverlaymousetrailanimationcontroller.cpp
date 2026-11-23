#include "gamewindow/tutorialoverlaymousetrailanimationcontroller.h"

#include "gamewindow/remainingdialogs.h"

#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QVariant>

namespace tibia::gamewindow {

namespace {

/// The centre of an overlay item, in its parent's coordinates.
QPointF centreOf(const QQuickItem* item)
{
    const QPointF position = item->position();
    const QRectF bounds = item->boundingRect();
    return { position.x() + bounds.width() * 0.5, position.y() + bounds.height() * 0.5 };
}

}

TTutorialOverlayMouseTrailAnimationController::TTutorialOverlayMouseTrailAnimationController(QQuickItem* item)
    : cip::tibia::gamewindow::TTutorialOverlayMouseTrailAnimationController(nullptr)
    , m_item(item)
{}

TTutorialOverlayMouseTrailAnimationController::~TTutorialOverlayMouseTrailAnimationController()
{
    if (!m_item)
        return;
    m_item->setParent(nullptr);
    m_item->deleteLater();
    m_item = nullptr;
}

void TTutorialOverlayMouseTrailAnimationController::updateAnimation()
{
    const std::shared_ptr<TTutorialOverlayItemController> source = m_source.lock();
    const std::shared_ptr<TTutorialOverlayItemController> destination = m_destination.lock();
    if (!m_item)
        return;
    if (source && source->item())
        m_item->setProperty("sourcePoint", centreOf(source->item()));
    if (destination && destination->item())
        m_item->setProperty("destinationPoint", centreOf(destination->item()));
    QString animationType = QStringLiteral("none");
    if (m_animationType == DRAG_AND_DROP)
        animationType = QStringLiteral("draganddrop");
    else if (m_animationType == MULTI_USE)
        animationType = QStringLiteral("multiuse");
    m_item->setProperty("animationType", animationType);
}

}
