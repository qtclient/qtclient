#pragma once

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <map>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace gamewindow {

class TSidebarWidget;

/// CIP : creates a QML file of the qmlcomponents module with no
/// parent item; null (and a warning) when it fails.
QQuickItem* createQuickItem(QQmlEngine* engine, const QString& file);

/// One sidebar.qml column (CIP: TSidebarController,). Panels go
/// into its sidebarUpperPane and widgets into its sidebarLowerPane, ordered by
/// their identifier (Layout.row) from one counter.
class TSidebarController final : public cip::tibia::gamewindow::TSidebarController
{
    Q_OBJECT
public:
    TSidebarController(uint sidebarId, QQmlEngine* engine, QQuickItem* placeholder, QObject* parent = nullptr);
    ~TSidebarController() override;

    QQuickItem* item() const { return m_item; }
    QQuickItem* upperPane() const { return m_upperPane; }
    QQuickItem* lowerPane() const { return m_lowerPane; }

    /// CIP : the panel goes to the end of the upper pane.
    void addPanel(QQuickItem* panel);
    /// CIP : the panel gets the next identifier (moves to the end).
    void movePanelToEnd(QQuickItem* panel);

    /// CIP : the widget goes to the end of the lower pane, or to
    /// the place of a reserved `identifier`.
    void addWidget(TSidebarWidget* widget, uint identifier = 0);
    /// The identifier the next widget would get, kept for a widget that comes
    /// later (pack widgets).
    uint reserveIdentifier() { return m_nextIdentifier++; }
    /// CIP : takes the widget out if it is here, keeping its size
    /// for its next sidebar; `keepParentItem` while it moves on.
    bool removeWidget(TSidebarWidget* widget, bool keepParentItem);
    /// CIP : the height no widget uses.
    int remainingHeight() const;
    /// CIP .
    void setAcceptsWidgetDrops(bool accepts);
    /// The sidebar item's triggerCheckBoundsEnabled.
    void setTriggerCheckBoundsEnabled(bool enabled);

private:
    QQmlEngine* m_engine = nullptr;
    QPointer<QQuickItem> m_item;
    QPointer<QQuickItem> m_upperPane;
    QPointer<QQuickItem> m_lowerPane;
    /// The next widget's and the next panel's identifiers (two counters).
    uint m_nextIdentifier = 1;
    uint m_nextPanelIdentifier = 1;
};

/// The sidebar columns on both sides of the game window (CIP: TSidebarManager,
/// ctor). Right sidebars have ids 0.., left ones 100..; id 0 is the
/// main sidebar holding the panels and is never closed.
class TSidebarManager final : public cip::tibia::gamewindow::TSidebarManager
{
    Q_OBJECT
public:
    enum Side { Right = 0, Left = 1 };

    TSidebarManager(QQmlEngine* engine, QQuickItem* rightPlaceholder, QObject* rightManagerItem, QQuickItem* leftPlaceholder,
                    QObject* leftManagerItem, QObject* parent = nullptr);

    /// The engine the sidebars (and the widgets the widgets manager opens) are created with.
    QQmlEngine* engine() const { return m_engine; }
    ~TSidebarManager() override;

    int sidebarCount(Side side) const { return side == Right ? m_rightCount : m_leftCount; }
    /// CIP : adds or removes the sidebars closest to the game window.
    void setSidebarCount(Side side, int count);
    TSidebarController* sidebar(uint sidebarId) const;

    /// CIP : a panel joins the main sidebar.
    void addPanel(cip::tibia::gamewindow::TSidebarPanelControllerBase* controller, QQuickItem* panel);
    /// CIP .
    void movePanelToEnd(QQuickItem* panel);
    /// Every sidebar checks its widgets against the window bounds.
    void emitCheckBounds();
    /// Every sidebar's triggerCheckBoundsEnabled.
    void setTriggerCheckBoundsEnabled(bool enabled);

    /// CIP .
    bool hasSidebar(uint sidebarId) const { return m_sidebars.contains(sidebarId); }
    /// CIP: plays the open sound, then the widget goes to the first sidebar
    /// with room for it, else to the outermost one (the last left sidebar,
    /// else the last right).
    void addWidget(TSidebarWidget* widget);
    /// CIP: into an existing sidebar; `identifier` from
    /// TSidebarController::reserveIdentifier.
    void addWidget(uint sidebarId, TSidebarWidget* widget, uint identifier = 0);
    /// CIP: into `sidebarId` when it exists, else where addWidget(widget) puts it.
    void placeWidget(uint sidebarId, TSidebarWidget* widget);
    /// CIP: takes the widget out of its parent sidebar.
    bool removeWidget(TSidebarWidget* widget);
    /// Sidebar ids, right ones first (0..), then left ones (100..).
    std::vector<uint> sidebarIds() const;
    static constexpr uint LeftSidebarIdBase = 100;

    bool canOpenSidebar() const override { return m_unusedWidth > MinUnusedWidthForSidebar; }
    bool canCloseRightSidebar() const override { return m_rightCount > 1; }
    bool canCloseLeftSidebar() const override { return m_leftCount != 0; }

    Q_INVOKABLE void openAdditionalSidebar(const QString& position) override;
    Q_INVOKABLE void closeSidebarClosestToGamewindow(const QString& position) override;

public Q_SLOTS:
    void onUnusedWidthChanged(int unusedWidth) override;
    void enqueuePanelConfigurationChangedToBeEmitted() override;
    void enqueueWidgetConfigurationChangedToBeEmitted() override;

private Q_SLOTS:
    /// Tip NOTIFY deepen fan-in for controller positionInSidebarChanged.
    void onControllerPositionInSidebarChanged();

Q_SIGNALS:
    void numberOfSidebarsChanged(size_t newNumber);

private:
    static constexpr int MinUnusedWidthForSidebar = 175;

    /// CIP onWidgetWantsToMoveToSidebar.
    void moveWidgetToSidebar(int targetSidebar, TSidebarWidget* widget);
    /// CIP : while a widget is dragged, the other sidebars take it.
    void onWidgetDragChanged(uint sourceSidebar, bool active);

    /// CIP.
    void addSidebar(Side side);
    void removeSidebar(Side side);
    QQuickItem* placeholder(Side side) const { return side == Right ? m_rightPlaceholder : m_leftPlaceholder; }

    QQmlEngine* m_engine = nullptr;
    QPointer<QQuickItem> m_rightPlaceholder;
    QPointer<QQuickItem> m_leftPlaceholder;
    std::map<uint, std::unique_ptr<TSidebarController>> m_sidebars;
    int m_rightCount = 0;
    int m_leftCount = 0;
    int m_unusedWidth = 0;
    QTimer m_panelConfigurationTimer;
    QTimer m_widgetConfigurationTimer;
};

}
}
