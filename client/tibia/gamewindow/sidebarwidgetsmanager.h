#pragma once

#include "config/sidebaroptions.h"

#include <cip_gen.h>

#include <QJsonArray>
#include <QPointer>

#include <functional>
#include <map>
#include <memory>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace input {
class IGameAction;
}
namespace gamewindow {

class TBattleListControllerStorage;
class TContainerStorageController;
class TSidebarManager;
class TSidebarWidget;
class TSkillsWidgetController;

/// The widgets in the sidebars' lower panes (CIP: TSidebarWidgetsManager,
/// ): opens, closes and toggles them (TOGGLE_WIDGET,
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
        KillTracker = Prey,
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
    /// has left its sidebar (the controller saves its widget options and is
    /// deleted); `current` is the widget while it is open; `discard`, when
    /// given, deletes the controller without saving its widget options (CIP
    /// closes every widget that way when the session changes).
    struct WidgetHooks
    {
        std::function<TSidebarWidget*()> open;
        std::function<void()> close;
        std::function<TSidebarWidget*()> current;
        std::function<void()> discard = {};
    };

    /// Hooks for a qtclient widget manager with open(options) / close(save) / current().
    template <class Manager, class Open>
    static WidgetHooks hooksFor(Manager& manager, Open open)
    {
        return { std::move(open), [&manager] { manager.close(true); }, [&manager] { return manager.current(); },
                 [&manager] { manager.close(false); } };
    }

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
    /// CIP onWidgetClosed: the widget saves its options when
    /// `saveWidgetOptions`, is deleted, the open widgets are saved and
    /// widgetHidden is emitted.
    void closeWidget(int widgetType, bool saveWidgetOptions = true);
    /// The CIP per-type toggle: an open widget saves its options and closes,
    /// a closed one opens in `sidebarId`.
    void toggleWidget(int widgetType, uint sidebarId = 0xff);
    /// Close every registered widget and battle list (product mode switch).
    void closeAllWidgets();
    /// The open widget's QML root.
    QQuickItem* widgetQuickItem(int widgetType) const;

    static QString widgetTypeName(int widgetType);
    static int widgetTypeOf(const QString& name);

public Q_SLOTS:
    /// CIP: the open widgets become sidebarWidgetsMangerOptions.
    void saveCurrentOpenWidgets() override;
    /// CIP: unless the open widgets already match the saved ones, closes every
    /// widget, opens as many sidebars as fit the game window and reopens the
    /// saved widgets sidebar by sidebar.
    void restoreSidebarConfigurationFromOptions() override;

    void onToggleSkillsWidget(uint targetSidebarId) { toggleWidget(Skills, targetSidebarId); }
    void onToggleUnjustifiedPointsWidget(uint targetSidebarId) { toggleWidget(UnjustifiedPoints, targetSidebarId); }
    void onToggleKillTrackerWidget(uint targetSidebarId) { toggleWidget(KillTracker, targetSidebarId); }
    void onToggleSpellListWidget(uint targetSidebarId) { toggleWidget(SpellList, targetSidebarId); }
    void onToggleBossTrackerWidget(uint targetSidebarId) { toggleWidget(BossCooldowns, targetSidebarId); }
    void onToggleQuestTrackerWidget(uint targetSidebarId) { toggleWidget(QuestTracker, targetSidebarId); }
    void onToggleLootTrackerWidget(uint targetSidebarId) { toggleWidget(LootTracker, targetSidebarId); }
    void onToggleLootAnalyserWidget(uint targetSidebarId) { toggleWidget(LootAnalyser, targetSidebarId); }
    void onToggleWasteAnalyserWidget(uint targetSidebarId) { toggleWidget(WasteAnalyser, targetSidebarId); }
    void onToggleImpactAnalyserWidget(uint targetSidebarId) { toggleWidget(ImpactAnalyser, targetSidebarId); }
    void onToggleDamageInputAnalyserWidget(uint targetSidebarId) { toggleWidget(DamageInputAnalyser, targetSidebarId); }
    /// Free accounts get a status message instead (the analyser's factory publishes it).
    void onToggleHuntingSessionAnalyserWidget(uint targetSidebarId) { toggleWidget(HuntingSessionAnalyser, targetSidebarId); }
    void onToggleProgressAnalyserWidget(uint targetSidebarId) { toggleWidget(XpAnalyser, targetSidebarId); }
    void onToggleAnalyticsSelectorWidget(uint targetSidebarId) { toggleWidget(AnalyticsSelector, targetSidebarId); }
    void onTogglePartyHuntAnalyserWidget(uint targetSidebarId) { toggleWidget(PartyHuntAnalyser, targetSidebarId); }

    void onSkillsWidgetClosed(bool saveWidgetOptions) { closeWidget(Skills, saveWidgetOptions); }
    void onUnjustifiedPointsWidgetClosed(bool saveWidgetOptions) { closeWidget(UnjustifiedPoints, saveWidgetOptions); }
    void onKillTrackerWidgetClosed(bool saveWidgetOptions) { closeWidget(KillTracker, saveWidgetOptions); }
    void onSpellListWidgetClosed(bool saveWidgetOptions) { closeWidget(SpellList, saveWidgetOptions); }
    void onBossTrackerWidgetClosed(bool saveWidgetOptions) { closeWidget(BossCooldowns, saveWidgetOptions); }
    void onQuestTrackerWidgetClosed(bool saveWidgetOptions) { closeWidget(QuestTracker, saveWidgetOptions); }
    void onLootTrackingWidgetClosed(bool saveWidgetOptions) { closeWidget(LootTracker, saveWidgetOptions); }
    void onLootAnalyserWidgetClosed(bool saveWidgetOptions) { closeWidget(LootAnalyser, saveWidgetOptions); }
    void onWasteAnalyserWidgetClosed(bool saveWidgetOptions) { closeWidget(WasteAnalyser, saveWidgetOptions); }
    void onImpactAnalyserWidgetClosed(bool saveWidgetOptions) { closeWidget(ImpactAnalyser, saveWidgetOptions); }
    void onDamageInputAnalyserWidgetClosed(bool saveWidgetOptions) { closeWidget(DamageInputAnalyser, saveWidgetOptions); }
    void onHuntingSessionAnalyserWidgetClosed(bool saveWidgetOptions) { closeWidget(HuntingSessionAnalyser, saveWidgetOptions); }
    void onProgressAnalyserWidgetClosed(bool saveWidgetOptions) { closeWidget(XpAnalyser, saveWidgetOptions); }
    void onAnalyticsSelectorWidgetClosed(bool saveWidgetOptions) { closeWidget(AnalyticsSelector, saveWidgetOptions); }
    void onPartyHuntAnalyserWidgetClosed(bool saveWidgetOptions) { closeWidget(PartyHuntAnalyser, saveWidgetOptions); }
    void onWidgetClosed(int type, bool saveWidgetOptions) { closeWidget(type, saveWidgetOptions); }

    /// CIP: the imbuement tracker widget subscribes to imbuement durations
    /// while it is shown (GameclientMessageTrackImbuements).
    void onWidgetVisible(int widgetType);
    void onWidgetHidden(int widgetType);

Q_SIGNALS:
    void widgetVisible(int widgetType);
    void widgetHidden(int widgetType);

private Q_SLOTS:
    /// Tip NOTIFY deepen fan-in for registered widget controllers' widgetClosed.
    void onRegisteredWidgetClosed();

private:
    TSidebarWidget* currentWidget(int widgetType) const;
    /// TOGGLE_WIDGET: the widget type's toggle in the action's sidebar.
    bool handleGameAction(const input::IGameAction& action);
    /// CIP: on a session change every widget closes without saving its
    /// options, and one right sidebar remains.
    void closeAll();
    void closeSkills(bool saveWidgetOptions);
    QJsonArray serializeSidebar(uint sidebarId) const;
    /// CIP: the open widgets per sidebar; empty while restoring.
    config::SidebarWidgetsManagerOptions currentWidgetsManagerOptions() const;

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
};

}
}
