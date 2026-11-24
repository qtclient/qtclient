#pragma once

#include "input/gameactiondispatch.h"
#include "config/sidebaroptions.h"

#include <cip_gen.h>

#include <QJsonArray>
#include <QPointer>

#include <functional>
#include <map>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace analysers {
class TAnalyserStorages;
}
namespace config {
class TClientOptions;
}
namespace news {
class TNewsStorage;
}
namespace shared {
class TTibiaFileSystemHelper;
}
namespace spells {
class TSpellInfoStorage;
}
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
namespace input {
class IGameAction;
class TEventFilterManager;
class TGameActionMapper;
}
namespace gamewindow {

class TBattleListControllerStorage;
class TBossTrackerWidgetController;
class TContainerStorageController;
class TCreatureTrackerWidgetController;
class TDepotSearchWidgetController;
class TImbuementTrackerWidgetController;
class TKillTrackerWidgetController;
class TLootTrackingWidgetManager;
class TPlayerGuideWidgetManager;
class TPlayerTradeController;
class TQuestTrackerWidgetManager;
class TSidebarManager;
class TSidebarWidget;
class TSkillsWidgetController;
class TSpellListWidgetManager;
class TUnjustifiedPointsWidgetManager;
class TVipWidgetManager;

/// The widgets in the sidebars' lower panes (CIP: TSidebarWidgetsManager,
/// ): opens, closes and toggles them (TOGGLE_WIDGET,
/// OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR) and saves and restores which widgets
/// each sidebar holds, in order (sidebars.json's sidebarWidgetsMangerOptions).
/// CIP: it owns every sidebar widget it opens: the skills widget, the kill,
/// bestiary, bosstiary, boss and imbuement trackers, the battle lists, the
/// player trade and depot search widgets (opened and closed by the session's
/// protocol handlers) and the other widget types (held here through
/// qtclient's per-widget managers). The analysers and pack widgets join
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
    /// closes every widget that way when the session changes);
    /// `connectToGameSession`, when given, is the widget manager's connector.
    struct WidgetHooks
    {
        std::function<TSidebarWidget*()> open;
        std::function<void()> close;
        std::function<TSidebarWidget*()> current;
        std::function<void()> discard = {};
        std::function<void(const std::shared_ptr<game::IGameSession>&)> connectToGameSession = {};
    };

    /// Hooks for a qtclient widget manager with open(options) / close(save) / current(),
    /// and its connectToGameSession(session) when it has one.
    template <class Manager, class Open>
    static WidgetHooks hooksFor(Manager& manager, Open open)
    {
        WidgetHooks hooks{ std::move(open), [&manager] { manager.close(true); }, [&manager] { return manager.current(); },
                           [&manager] { manager.close(false); } };
        if constexpr (requires { manager.connectToGameSession(std::shared_ptr<game::IGameSession>{}); })
            hooks.connectToGameSession = [&manager](const std::shared_ptr<game::IGameSession>& GameSession) {
                manager.connectToGameSession(GameSession);
            };
        return hooks;
    }

    /// CIP: makes the battle lists and connects them; the other widgets are
    /// made when they open. `analyserStorages` is the loot tracker's source
    /// (host: CipSoft passes the session's storages).
    /// The widgets are made with the sidebar manager's engine over
    /// `dialogParentItem`. Host: `analyserStorages` feed the loot tracker.
    TSidebarWidgetsManager(TSidebarManager* sidebars, TContainerStorageController* containers,
                           std::shared_ptr<input::TGameActionMapper> gameActionMapper, input::TEventFilterManager* eventFilterManager,
                           config::TClientOptions* clientOptions, std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                           std::shared_ptr<news::TNewsStorage> newsStorage, spells::TSpellInfoStorage* spellInfoStorage,
                           QQuickItem* dialogParentItem, analysers::TAnalyserStorages* analyserStorages, QObject* parent = nullptr);
    /// CIP: deletes the widgets, the battle lists, the depot search and
    /// player trade widgets, then the widgets kept by type.
    ~TSidebarWidgetsManager() override;

    /// The character's options; null while no character is in the world
    /// (every widget closes).
    void setSidebarOptions(config::TSidebarOptions* options);
    /// CIP: every widget closes without saving its options (the old options
    /// go first); the options become the session's sidebar options, which
    /// are restored when the character's configuration loads or they change.
    /// The session's player trade and depot search requests open and close
    /// those widgets; every owned widget connects to the session.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Follows the account's premium state: a free account loses the hunting
    /// session analyser (checked at once and on every change). The widgets
    /// read the live storage themselves.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage);
    void setControlScheme(int controlScheme);
    void onLoginRampUpFinished();
    /// CIP: the frame update (TGameClient, before every rendered frame): the
    /// battle lists, the depot search's hover cursor, the boss tracker's time,
    /// the bosstiary tracker's cooldowns, the containers' clocks and the
    /// imbuement tracker's countdown.
    void updateWithTimestamp(qint64 TimestampMilliseconds);
    /// The game session the widgets follow (null without one); widgets made
    /// on toggle connect to it.
    const std::shared_ptr<game::IGameSession>& gameSession() const { return m_gameSession; }
    TBattleListControllerStorage* battleLists() const { return m_battleLists.get(); }
    /// The widgets the manager keeps by type (host: their qtclient managers).
    TVipWidgetManager* vipWidget() const { return m_vip.get(); }
    TQuestTrackerWidgetManager* questTrackerWidget() const { return m_questTracker.get(); }

    void registerWidget(int widgetType, WidgetHooks hooks);
    bool isWidgetOpen(int widgetType) const;
    /// Opens the widget in `sidebarId` (0xff or a missing sidebar: the first
    /// with room, or HUD right when hud placement is on).
    void openWidget(int widgetType, uint sidebarId = 0xff);
    /// The widget saves its options when
    /// `saveWidgetOptions`, is deleted, the open widgets are saved and
    /// widgetHidden is emitted.
    void onWidgetClosed(int widgetType, bool saveWidgetOptions = true);
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

    void onSkillsWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(Skills, saveWidgetOptions); }
    void onUnjustifiedPointsWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(UnjustifiedPoints, saveWidgetOptions); }
    void onKillTrackerWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(KillTracker, saveWidgetOptions); }
    void onSpellListWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(SpellList, saveWidgetOptions); }
    void onBossTrackerWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(BossCooldowns, saveWidgetOptions); }
    void onQuestTrackerWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(QuestTracker, saveWidgetOptions); }
    void onLootTrackingWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(LootTracker, saveWidgetOptions); }
    void onLootAnalyserWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(LootAnalyser, saveWidgetOptions); }
    void onWasteAnalyserWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(WasteAnalyser, saveWidgetOptions); }
    void onImpactAnalyserWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(ImpactAnalyser, saveWidgetOptions); }
    void onDamageInputAnalyserWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(DamageInputAnalyser, saveWidgetOptions); }
    void onHuntingSessionAnalyserWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(HuntingSessionAnalyser, saveWidgetOptions); }
    void onProgressAnalyserWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(XpAnalyser, saveWidgetOptions); }
    void onAnalyticsSelectorWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(AnalyticsSelector, saveWidgetOptions); }
    void onPartyHuntAnalyserWidgetClosed(bool saveWidgetOptions) { onWidgetClosed(PartyHuntAnalyser, saveWidgetOptions); }

    /// CIP: a new trade window replaces the open one; it follows the session,
    /// is maximized once the login ramp-up is over and goes to the sidebars.
    void onPlayerTradeWidgetOpenRequest() override;
    /// CIP: the trade window is deleted.
    void onPlayerTradeWidgetCloseRequest() override;
    /// CIP: unless it is open, the depot search widget is made, follows the
    /// session, is maximized once the login ramp-up is over and goes to the
    /// sidebars.
    void onDepotSearchWidgetOpenRequest() override;
    /// CIP: the depot search widget saves its options and is deleted.
    void onDepotSearchWidgetCloseRequest() override;

    /// CIP: the imbuement tracker widget subscribes to imbuement durations
    /// while it is shown (GameclientMessageTrackImbuements).
    void onWidgetVisible(int widgetType);
    void onWidgetHidden(int widgetType);
    /// Without premium the hunting session analyser closes without saving
    /// its options; the open widgets are saved and widgetHidden is emitted.
    void onPremiumTimeChanged();

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
    /// CIP's toggle of a widget the manager owns: the controller is made with
    /// the engine over the dialog parent when it opens, its game actions are
    /// published, it connects to the game session and is maximized once the
    /// login ramp-up is over; closing saves its options (unless discarded) and
    /// deletes it.
    template <class Controller, class Make>
    void registerOwnedWidget(int widgetType, QPointer<Controller>& controller, Make make);
    QJsonArray serializeSidebar(uint sidebarId) const;
    /// CIP: the open widgets per sidebar; empty while restoring.
    config::SidebarWidgetsManagerOptions currentWidgetsManagerOptions() const;

    QPointer<TSidebarManager> m_sidebars;
    QPointer<TContainerStorageController> m_containers;
    std::shared_ptr<input::TGameActionMapper> m_pGameActionMapper;
    input::TEventFilterManager* m_eventFilterManager = nullptr;
    config::TClientOptions* m_pClientOptions = nullptr;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_pFileSystemHelper;
    std::shared_ptr<news::TNewsStorage> m_newsStorage;
    spells::TSpellInfoStorage* m_spellInfoStorage = nullptr;
    QQuickItem* m_dialogParentItem = nullptr;
    QPointer<config::TSidebarOptions> m_options;
    std::unique_ptr<TBattleListControllerStorage> m_battleLists;
    TSkillsWidgetController* m_skills = nullptr;
    /// CIP: the trade window and the depot search widget, while open.
    QPointer<TPlayerTradeController> m_playerTrade;
    QPointer<TDepotSearchWidgetController> m_depotSearch;
    /// The widgets kept by type (CIP: the widget map and members; qtclient's
    /// per-widget managers make and delete the controllers).
    std::unique_ptr<TVipWidgetManager> m_vip;
    QPointer<TKillTrackerWidgetController> m_killTracker;
    std::unique_ptr<TQuestTrackerWidgetManager> m_questTracker;
    std::unique_ptr<TLootTrackingWidgetManager> m_lootTracking;
    std::unique_ptr<TSpellListWidgetManager> m_spellList;
    std::unique_ptr<TUnjustifiedPointsWidgetManager> m_unjustifiedPoints;
    QPointer<TImbuementTrackerWidgetController> m_imbuementTracker;
    QPointer<TCreatureTrackerWidgetController> m_bestiaryTracker;
    QPointer<TCreatureTrackerWidgetController> m_bosstiaryTracker;
    QPointer<TBossTrackerWidgetController> m_bossTracker;
    std::unique_ptr<TPlayerGuideWidgetManager> m_playerGuide;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    std::map<int, WidgetHooks> m_widgets;
    std::map<int, QMetaObject::Connection> m_closeConnections;
    int m_controlScheme = 1; // CIP control scheme (1 classic)
    bool m_restoring = false;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
