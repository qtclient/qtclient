#pragma once

#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "qtconnectionmanager.h"

#include "shared/sortedvectorlistmodel.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>

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
class TTaskboardBountyTasksStorage;
class TTaskboardWeeklyTasksStorage;
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
/// TKillTrackerWidgetController): one render info per prey slot of the
/// session's prey storage, plus the bounty and weekly task lists of the
/// session's task board storages. The sidebar widgets manager makes it and
/// connects it to the game session.
class TKillTrackerWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TKillTrackerWidgetController>
{
    Q_OBJECT

public:
    /// CipSoft's constructor: the two task models and the widget item.
    TKillTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    /// CIP: clears the prey list first (clearPreyList).
    ~TKillTrackerWidgetController() override;

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
    /// CIP: the prey list is rebuilt from the session's prey storage (its
    /// reroll and choose prices announced), the bounty and weekly task lists
    /// follow the session's task board storages (filled now) and the widget
    /// options follow the session's sidebar options (applied now).
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
    /// CIP: the weekly task list from the weekly tasks storage.
    void weeklyTaskDataChanged();
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    /// CIP: empties preyList, announces it, then deletes the old render infos.
    void clearPreyList();
    void applyBountyMonsters(const std::vector<taskboard::TBountyTask>& tasks);
    void applyWeeklyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters);

    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<config::TSidebarOptions> m_options;
    std::shared_ptr<prey::TPreyStorage> m_storage;
    std::shared_ptr<taskboard::TTaskboardBountyTasksStorage> m_bountyTasksStorage;
    std::shared_ptr<taskboard::TTaskboardWeeklyTasksStorage> m_weeklyTasksStorage;
    bool m_weeklyActive = true;
    shared::TQtConnectionManager m_gameSessionConnections;
};


}
}
