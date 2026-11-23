#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "qtconnectionmanager.h"

#include "shared/sortedvectorlistmodel.h"

#include <cip_gen.h>

#include <QPointer>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace game {
class IGameSession;
}
namespace prey {
class TPreyStorage;
}
namespace taskboard {
struct TBountyTask;
}
}

namespace tibia {
namespace gamewindow {

/// CIP `TKillTrackerWidgetController::TTaskMonsterModel` (ledger short name
/// TTaskMonsterModel) — bounty/weekly task monster rows.
class TTaskMonsterModel final : public shared::TSortedVectorListModel<cip::tibia::gamewindow::TKillTrackerWidgetController_TTaskMonsterModel>
{
    Q_OBJECT
public:
    enum Role
    {
        RaceID = 0x101,
        RaceName,
        AmountToHunt,
        AmountHunted,
        IsActive,
    };

    /// One hunting task row (race 0 with no name: none, or any creature).
    struct Entry
    {
        /// The row's key: rows are kept in this order.
        uint8_t index = 0;
        uint16_t raceId = 0;
        QString raceName;
        uint16_t amountToHunt = 0;
        uint16_t amountHunted = 0;
        bool isActive = false;
    };

    explicit TTaskMonsterModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    /// Merges the rows by `index`; an existing row with other values is
    /// copied and reported changed.
    void setEntries(std::vector<Entry> entries);

private:
    std::vector<Entry> m_entries;
};

/// The kill tracker widget, prey/KillTrackerWidget.qml (CIP:
/// TKillTrackerWidgetController, ctor , init): one
/// render info per prey slot, built once from the storage, plus bounty/weekly
/// task lists from onBountyTaskData / onWeeklyTaskData.
class TKillTrackerWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TKillTrackerWidgetController>
{
    Q_OBJECT

public:
    TKillTrackerWidgetController(prey::TPreyStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    /// CIP: clears the prey list first (clearPreyList).
    ~TKillTrackerWidgetController() override;

    /// preyWidgetOptions: contentHeight, contentMaximized.
    void setSidebarOptions(config::TSidebarOptions* options);

    /// CIP : the storage's bonusRerollPossible.
    bool bonusRerollpossible() const override;
    /// CIP : the storage's choosePreyPossible.
    bool lockPreyPossible() const override;
    QList<QObject*> preyList() const override;
    QObject* bountyTasksModel() const override;
    QObject* weeklyTasksModel() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// CIP : OPEN_PREY_DIALOG.
    void requestOpenPreyDialog() override;
    /// CIP: OPEN_TASKBOARD_DIALOG (BountyTasks tab).
    void requestOpenBountyTasksDialog() override;
    /// CIP: OPEN_TASKBOARD_DIALOG (WeeklyTasks tab).
    void requestOpenWeeklyTasksDialog() override;
    /// CIP .
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();
    /// Host: the bounty and weekly rows follow the session's task board
    /// storages (until TSidebarWidgetsManager's session connector is ported).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome (shared_ptr / TSidebarID skipped) — qtclient NOTIFY deepen; drag/move
    /// forwarded via TSidebarWidget dynamic emit.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, TSidebarWidget* widget);
    /// CIP preyList NOTIFY — qtclient emits after slot render-info list is built.

private Q_SLOTS:
    /// CIP weeklyTaskDataChanged — qtclient fills from onWeeklyTaskData notify.
    void weeklyTaskDataChanged();
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    /// CIP: empties preyList, announces it, then deletes the old render infos.
    void clearPreyList();
    void applyBountyMonsters(const std::vector<taskboard::TBountyTask>& tasks);
    void applyWeeklyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters);

    prey::TPreyStorage* m_storage;
    QPointer<config::TSidebarOptions> m_options;
    std::vector<client::gamecore::Hook> m_hooks;
    std::vector<std::map<std::string, uint32_t>> m_pendingWeekly;
    bool m_weeklyActive = true;
    shared::TQtConnectionManager m_gameSessionConnections;
};

/// Owns the kill tracker widget while it is open. The sidebar widgets manager
/// opens and closes it (EWidgetType 5) through open / close / current.
class TKillTrackerWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TKillTrackerWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TKillTrackerWidgetManager() override;

    /// Host: the game session's prey storage (CIP hands it over in the sidebar
    /// widgets' session connector).
    void setPreyStorage(prey::TPreyStorage* storage);

    /// Makes the widget (if needed) with the session's sidebar options and returns it.
    TSidebarWidget* open(config::TSidebarOptions* options);
    /// Saves its options and deletes it.
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;
    /// Host: the open widget and the next one follow the session's task board.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    prey::TPreyStorage* m_storage = nullptr;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TKillTrackerWidgetController> m_controller;
    std::shared_ptr<game::IGameSession> m_gameSession;
};

}
}
