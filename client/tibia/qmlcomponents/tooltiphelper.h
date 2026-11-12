#pragma once

#include <cip_gen.h>

#include <QEvent>
#include <QPointer>

class QQuickWindow;

namespace tibia {
namespace qmlcomponents {

/// Application event filter that reports hover-relevant events
/// (CIP: TMouseHoverEventFilter::eventFilter / eventTriggered).
class TMouseHoverEventFilter : public cip::tibia::qmlcomponents::TMouseHoverEventFilter
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TMouseHoverEventFilter::TMouseHoverEventFilter;

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    /// Tip alias used by TTooltipHelperQmlService.
    void hoverEvent(QObject* watched, int type);
    /// CIP moc signal (QObject*, QEvent::Type).
    void eventTriggered(QObject* pObject, QEvent::Type EventType);
};

/// Hover state of the single registered tooltip (CIP: TTooltipEventWatcher,
/// metacall).
class TTooltipEventWatcher : public cip::tibia::qmlcomponents::TTooltipEventWatcher
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TTooltipEventWatcher::TTooltipEventWatcher;

    void unregister() override;
    bool contained(QQuickItem* pParent) override;
    QPointF hoverPosition(QQuickItem* pParent) override;
};

/// QML singleton TooltipHelper (CIP: TTooltipHelperQmlService, metacall
/// ). One tooltip is registered at a time; registering another
/// unregisters the previous one.
class TTooltipHelperQmlService : public cip::tibia::qmlcomponents::TTooltipHelperQmlService
{
    Q_OBJECT
public:
    explicit TTooltipHelperQmlService(QObject* parent = nullptr);

    cip::tibia::qmlcomponents::TTooltipEventWatcher* registerTooltip(QQuickItem* pQuickItem) override;
    QQuickItem* findWindowContentItemForQuickItem(QQuickItem* pQuickItem) override;
    bool inWindow() override { return m_inWindow; }

private:
    void onHoverEvent(QObject* watched, int type);
    void release(cip::tibia::qmlcomponents::TTooltipEventWatcher* watcher);
    void scheduleHoverRefresh();

    QPointer<cip::tibia::qmlcomponents::TTooltipEventWatcher> m_watcher;
    QPointer<QQuickWindow> m_window;
    TMouseHoverEventFilter* m_filter;
    bool m_refreshScheduled = false;
    bool m_inWindow = false;
};

}
}
