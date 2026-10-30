#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickWindow;

namespace tibia::qmlcomponents {

/// Application event filter that reports hover-relevant events
/// (CIP: TMouseHoverEventFilter::eventFilter 0x1010d7d60).
class TMouseHoverEventFilter : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void hoverEvent(QObject* watched, int type);
};

/// Hover state of the single registered tooltip (CIP: TTooltipEventWatcher,
/// metacall 0x101088f40).
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
/// 0x101089320). One tooltip is registered at a time; registering another
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
