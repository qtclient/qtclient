#include "tooltiphelper.h"

#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QJSEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace tibia::qmlcomponents {

bool TMouseHoverEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Wheel:
        case QEvent::UpdateRequest:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove: {
            const QPointer<QObject> guard(watched);
            Q_EMIT hoverEvent(watched, event->type());
            return watched && !guard;
        }
        default:
            return false;
    }
}

void TTooltipEventWatcher::unregister()
{
    Q_EMIT unregistered(this);
}

bool TTooltipEventWatcher::contained(QQuickItem* pParent)
{
    return pParent && pParent->contains(pParent->mapFromGlobal(QCursor::pos()));
}

QPointF TTooltipEventWatcher::hoverPosition(QQuickItem* pParent)
{
    return pParent ? pParent->mapFromGlobal(QCursor::pos()) : QPointF();
}

TTooltipHelperQmlService::TTooltipHelperQmlService(QObject* parent) :
    cip::tibia::qmlcomponents::TTooltipHelperQmlService(parent),
    m_filter(new TMouseHoverEventFilter(this))
{}

/// CIP 0x1010d7510.
cip::tibia::qmlcomponents::TTooltipEventWatcher* TTooltipHelperQmlService::registerTooltip(QQuickItem* pQuickItem)
{
    if (!pQuickItem)
        return nullptr;

    if (const auto previous = m_watcher) {
        QMetaObject::invokeMethod(previous, [previous] { previous->unregister(); }, Qt::QueuedConnection);
        release(previous);
    }

    auto* watcher = new TTooltipEventWatcher(this);
    QJSEngine::setObjectOwnership(watcher, QJSEngine::JavaScriptOwnership);
    if (auto* app = QCoreApplication::instance())
        app->installEventFilter(m_filter);
    connect(m_filter, &TMouseHoverEventFilter::hoverEvent, this, &TTooltipHelperQmlService::onHoverEvent, Qt::UniqueConnection);
    connect(watcher, &cip::tibia::qmlcomponents::TTooltipEventWatcher::unregistered, this, &TTooltipHelperQmlService::release);
    connect(watcher, &QObject::destroyed, this, [this] {
        if (m_watcher)
            return;
        disconnect(m_filter, &TMouseHoverEventFilter::hoverEvent, this, &TTooltipHelperQmlService::onHoverEvent);
        if (auto* app = QCoreApplication::instance())
            app->removeEventFilter(m_filter);
        m_window = nullptr;
        m_inWindow = false;
    });
    connect(pQuickItem, &QObject::destroyed, watcher, &TTooltipEventWatcher::unregister, Qt::QueuedConnection);

    m_watcher = watcher;
    m_window = pQuickItem->window();
    m_inWindow = true;
    scheduleHoverRefresh();
    return watcher;
}

QQuickItem* TTooltipHelperQmlService::findWindowContentItemForQuickItem(QQuickItem* pQuickItem)
{
    if (!pQuickItem || !pQuickItem->window())
        return nullptr;
    return pQuickItem->window()->contentItem();
}

/// CIP 0x1010d7a70.
void TTooltipHelperQmlService::onHoverEvent(QObject* watched, const int type)
{
    if (!m_watcher)
        return;

    if (type == QEvent::Enter || type == QEvent::Leave) {
        if (!m_window || watched != m_window)
            return;
        m_inWindow = type == QEvent::Enter;
    } else if (type == QEvent::MouseButtonPress) {
        const QPointer<cip::tibia::qmlcomponents::TTooltipEventWatcher> watcher = m_watcher;
        QTimer::singleShot(0, this, [this, watcher] {
            if (!watcher || watcher != m_watcher)
                return;
            Q_EMIT watcher->hoverRefresh();
            if (watcher && watcher == m_watcher)
                Q_EMIT watcher->mouseButtonPressed();
        });
        return;
    }
    scheduleHoverRefresh();
}

/// CIP 0x1010d7950.
void TTooltipHelperQmlService::release(cip::tibia::qmlcomponents::TTooltipEventWatcher* watcher)
{
    if (!watcher || !m_watcher || m_watcher != watcher)
        return;

    m_watcher = nullptr;
    disconnect(m_filter, &TMouseHoverEventFilter::hoverEvent, this, &TTooltipHelperQmlService::onHoverEvent);
    if (auto* app = QCoreApplication::instance())
        app->removeEventFilter(m_filter);
    m_window = nullptr;
    m_inWindow = false;
    watcher->setParent(nullptr);
}

void TTooltipHelperQmlService::scheduleHoverRefresh()
{
    if (m_refreshScheduled)
        return;
    m_refreshScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshScheduled = false;
        if (m_watcher)
            Q_EMIT m_watcher->hoverRefresh();
    });
}

}
