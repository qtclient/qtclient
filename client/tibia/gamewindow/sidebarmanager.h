#pragma once

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <map>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia::gamewindow {

class TSidebarWidget;

/// CIP FUN_10109b3a0: creates a QML file of the qmlcomponents module with no
/// parent item; null (and a warning) when it fails.
QQuickItem* createQuickItem(QQmlEngine* engine, const QString& file);

/// CIP TSidebarPanelControllerBase v14 (0x100b8e930): the panel item's
/// panelController, and its positionInSidebarChanged forwarded to the controller.
void bindPanelItem(cip::tibia::gamewindow::TSidebarPanelControllerBase* controller, QQuickItem* item);

/// One sidebar.qml column (CIP: TSidebarController, 0x100b69580). Panels go
/// into its sidebarUpperPane and widgets into its sidebarLowerPane, ordered by
/// their identifier (Layout.row) from one counter.
class TSidebarController final : public cip::tibia::gamewindow::TSidebarController
{
    Q_OBJECT
public:
    TSidebarController(uint sidebarId, QQuickItem* placeholder, QObject* parent = nullptr);
    ~TSidebarController() override;

    QQuickItem* item() const { return m_item; }
    QQuickItem* upperPane() const { return m_upperPane; }
    QQuickItem* lowerPane() const { return m_lowerPane; }

    /// CIP 0x100b69980: the panel goes to the end of the upper pane.
    void addPanel(QQuickItem* panel);
    /// CIP 0x100b69ad0: the panel gets the next identifier (moves to the end).
    void movePanelToEnd(QQuickItem* panel);

    /// CIP 0x100b69b60: the widget goes to the end of the lower pane, or to
    /// the place of a reserved `identifier`.
    void addWidget(TSidebarWidget* widget, uint identifier = 0);
    /// The identifier the next widget would get, kept for a widget that comes
    /// later (CIP holds the place with the widget itself, see
    /// TContainerStorageController::PendingPlace).
    uint reserveIdentifier() { return m_nextIdentifier++; }
    /// CIP 0x100b69cb0: takes the widget out if it is here, keeping its size
    /// for its next sidebar; `keepParentItem` while it moves on.
    bool removeWidget(TSidebarWidget* widget, bool keepParentItem);
    /// CIP 0x100b69eb0: the height no widget uses.
    int remainingHeight() const;
    /// CIP 0x100b69960.
    void setAcceptsWidgetDrops(bool accepts);

private:
    QPointer<QQuickItem> m_item;
    QPointer<QQuickItem> m_upperPane;
    QPointer<QQuickItem> m_lowerPane;
    uint m_nextIdentifier = 1;
};

/// The sidebar columns on both sides of the game window (CIP: TSidebarManager,
/// ctor 0x100c72a50). Right sidebars have ids 0.., left ones 100..; id 0 is the
/// main sidebar holding the panels and is never closed.
class TSidebarManager final : public cip::tibia::gamewindow::TSidebarManager
{
    Q_OBJECT
public:
    enum Side { Right = 0, Left = 1 };

    TSidebarManager(QQuickItem* rightPlaceholder, QObject* rightManagerItem, QQuickItem* leftPlaceholder, QObject* leftManagerItem,
                    QObject* parent = nullptr);
    ~TSidebarManager() override;

    int sidebarCount(Side side) const { return side == Right ? m_rightCount : m_leftCount; }
    /// CIP 0x100c72f10: adds or removes the sidebars closest to the game window.
    void setSidebarCount(Side side, int count);
    TSidebarController* sidebar(uint sidebarId) const;

    /// CIP 0x100c73790: a panel joins the main sidebar.
    void addPanel(cip::tibia::gamewindow::TSidebarPanelControllerBase* controller, QQuickItem* panel);
    /// CIP 0x100c738a0.
    void movePanelToEnd(QQuickItem* panel);

    /// CIP 0x100c735f0.
    bool hasSidebar(uint sidebarId) const { return m_sidebars.contains(sidebarId); }
    /// CIP 0x100c738f0: the widget goes to the first sidebar with room for it,
    /// else to the outermost one (the last left sidebar, else the last right).
    void addWidget(TSidebarWidget* widget);
    /// CIP 0x100c73d60; `identifier` from TSidebarController::reserveIdentifier.
    void addWidget(uint sidebarId, TSidebarWidget* widget, uint identifier = 0);
    /// CIP 0x100c74bb0: takes the widget out of the sidebar holding it.
    bool removeWidget(TSidebarWidget* widget);
    /// Sidebar ids, right ones first (0..), then left ones (100..), then HUD hosts.
    std::vector<uint> sidebarIds() const;
    /// Classic column ids only (excludes product HUD hosts) — for options serialize/restore.
    std::vector<uint> classicSidebarIds() const;
    static constexpr uint LeftSidebarIdBase = 100;
    /// product HUD hosts (not classic left/right columns; not counted in m_rightCount/m_leftCount).
    static constexpr uint HudRightId = 200;
    static constexpr uint HudLeftId = 201;
    static constexpr uint HudContainersId = 202;
    static bool isHudSidebarId(uint sidebarId);

    /// Create or reparent a HUD sidebar column onto `hostPlaceholder` (mapWindow chrome).
    void ensureHudSidebar(uint sidebarId, QQuickItem* hostPlaceholder);
    void teardownHudSidebars();

    bool canOpenSidebar() const override { return m_unusedWidth > MinUnusedWidthForSidebar; }
    bool canCloseRightSidebar() const override { return m_rightCount > 1; }
    bool canCloseLeftSidebar() const override { return m_leftCount != 0; }

    Q_INVOKABLE void openAdditionalSidebar(const QString& position) override;
    Q_INVOKABLE void closeSidebarClosestToGamewindow(const QString& position) override;

public Q_SLOTS:
    void onUnusedWidthChanged(int unusedWidth) override;
    void enqueuePanelConfigurationChangedToBeEmitted() override;
    void enqueueWidgetConfigurationChangedToBeEmitted() override;

Q_SIGNALS:
    void numberOfSidebarsChanged(size_t newNumber);

private:
    static constexpr int MinUnusedWidthForSidebar = 175;

    /// CIP 0x100c74cc0.
    void moveWidgetToSidebar(int targetSidebar, TSidebarWidget* widget);
    /// CIP 0x100c74ed0: while a widget is dragged, the other sidebars take it.
    void onWidgetDragChanged(uint sourceSidebar, bool active);

    /// CIP 0x100c74310 / 0x100c74770.
    void addSidebar(Side side);
    void removeSidebar(Side side);
    QQuickItem* placeholder(Side side) const { return side == Right ? m_rightPlaceholder : m_leftPlaceholder; }

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
