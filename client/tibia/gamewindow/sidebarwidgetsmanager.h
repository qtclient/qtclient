#pragma once

#include <cip_gen.h>

#include <QJsonArray>
#include <QPointer>

#include <functional>
#include <map>
#include <memory>

class QQmlEngine;
class QQuickItem;

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

class TBattleListControllerStorage;
class TContainerStorageController;
class TSidebarManager;
class TSidebarWidget;
class TSkillsWidgetController;

/// The widgets in the sidebars' lower panes (CIP: TSidebarWidgetsManager,
/// 0x100bc5890): opens, closes and toggles them (TOGGLE_WIDGET,
/// OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR) and saves and restores which widgets
/// each sidebar holds, in order (sidebars.json's sidebarWidgetsMangerOptions).
/// It owns the skills widget and the battle lists; other widgets join
/// through registerWidget.
class TSidebarWidgetsManager final : public cip::tibia::gamewindow::TSidebarWidgetsManager
{
    Q_OBJECT
public:
    /// CIP EWidgetType.
    enum WidgetType
    {
        Skills = 0,
        BattleList = 1,
        PartyList = 2,
        Vip = 3,
        UnjustifiedPoints = 4,
        Prey = 5,
        SpellList = 6,
        Container = 8,
        QuestTracker = 10,
        BestiaryTracker = 11,
        BosstiaryTracker = 12,
        BossCooldowns = 13,
        LootTracker = 14,
        XpAnalyser = 15,
        LootAnalyser = 16,
        WasteAnalyser = 17,
        ImpactAnalyser = 18,
        DamageInputAnalyser = 19,
        HuntingSessionAnalyser = 20,
        AnalyticsSelector = 21,
        PartyHuntAnalyser = 22,
        ImbuementTracker = 23,
        PlayerGuide = 24,
    };

    /// A widget another controller owns: `open` returns its widget (made if
    /// needed), which the manager then places; `close` runs once the widget
    /// has left its sidebar; `current` is the widget while it is open.
    struct WidgetHooks
    {
        std::function<TSidebarWidget*()> open;
        std::function<void()> close;
        std::function<TSidebarWidget*()> current;
    };

    TSidebarWidgetsManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSidebarWidgetsManager() override;

    /// The character's options; null while no character is in the world
    /// (every widget closes).
    void setSidebarOptions(config::TSidebarOptions* options);
    void setContainerStorage(TContainerStorageController* containers);
    void setControlScheme(int controlScheme);
    void onLoginRampUpFinished();
    TBattleListControllerStorage* battleLists() const { return m_battleLists.get(); }

    void registerWidget(int widgetType, WidgetHooks hooks);
    bool isWidgetOpen(int widgetType) const;
    /// Opens the widget in `sidebarId` (0xff or a missing sidebar: the first
    /// with room, or HUD right when hud placement is on).
    void openWidget(int widgetType, uint sidebarId = 0xff);
    /// The widget saves its options and is deleted (CIP 0x100bc9710).
    void closeWidget(int widgetType);
    /// TOGGLE_WIDGET (CIP 0x100bcc080).
    void toggleWidget(int widgetType);
    /// Close every registered widget and battle list (product mode switch).
    void closeAllWidgets();
    /// When true, openWidget without an explicit classic id targets HudRight.
    void setHudPlacementEnabled(bool on);
    bool hudPlacementEnabled() const { return m_hudPlacement; }
    /// The open widget's QML root.
    QQuickItem* widgetQuickItem(int widgetType) const;

    static QString widgetTypeName(int widgetType);
    static int widgetTypeOf(const QString& name);

public Q_SLOTS:
    /// CIP 0x100bc56b0.
    void saveCurrentOpenWidgets() override;
    /// CIP 0x100bc6b90: closes every widget, then opens the saved ones
    /// sidebar by sidebar, unless the open widgets already match.
    void restoreSidebarConfigurationFromOptions() override;

Q_SIGNALS:
    void widgetVisible(int widgetType);
    void widgetHidden(int widgetType);

private:
    TSidebarWidget* currentWidget(int widgetType) const;
    void closeAll();
    QJsonArray serializeSidebar(uint sidebarId) const;
    uint resolveOpenSidebarId(uint sidebarId) const;

    QPointer<TSidebarManager> m_sidebars;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<TContainerStorageController> m_containers;
    std::unique_ptr<TBattleListControllerStorage> m_battleLists;
    TSkillsWidgetController* m_skills = nullptr;
    std::map<int, WidgetHooks> m_widgets;
    std::map<int, QMetaObject::Connection> m_closeConnections;
    int m_controlScheme = 0;
    bool m_restoring = false;
    bool m_hudPlacement = false;
};

}
