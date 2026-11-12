#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

#include <map>
#include <string>
#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace prey {
class TPreyStorage;
}
}

namespace tibia {
namespace gamewindow {

/// CIP `TKillTrackerWidgetController::TTaskMonsterModel` (ledger short name
/// TTaskMonsterModel) — bounty/weekly task monster rows.
class TTaskMonsterModel final : public cip::tibia::gamewindow::TKillTrackerWidgetController_TTaskMonsterModel
{
    Q_OBJECT
public:
    enum Role { NameRole = Qt::UserRole + 1, CountRole, RaceIdRole };

    struct Entry {
        QString name;
        int count = 0;
        int raceId = 0;
    };

    explicit TTaskMonsterModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
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

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome (shared_ptr / TSidebarID skipped) — qtclient NOTIFY deepen; drag/move
    /// forwarded via TSidebarWidget dynamic emit.
    void publishGameAction();
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, TSidebarWidget* widget);
    /// CIP preyList NOTIFY — qtclient emits after slot render-info list is built.

private Q_SLOTS:
    /// CIP bountyTaskDataChanged — qtclient fills from onBountyTaskData notify.
    void bountyTaskDataChanged();
    /// CIP weeklyTaskDataChanged — qtclient fills from onWeeklyTaskData notify.
    void weeklyTaskDataChanged();
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void applyBountyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters);
    void applyWeeklyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters);

    prey::TPreyStorage* m_storage;
    QPointer<config::TSidebarOptions> m_options;
    std::vector<client::gamecore::Hook> m_hooks;
    std::vector<std::map<std::string, uint32_t>> m_pendingBounty;
    std::vector<std::map<std::string, uint32_t>> m_pendingWeekly;
};

/// Owns the kill tracker widget while it is open. The sidebar widgets manager
/// opens and closes it (EWidgetType 5) through open / close / current.
class TKillTrackerWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TKillTrackerWidgetManager(prey::TPreyStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TKillTrackerWidgetManager() override;

    /// Makes the widget (if needed) with the session's sidebar options and returns it.
    TSidebarWidget* open(config::TSidebarOptions* options);
    /// Saves its options and deletes it.
    void close();
    TSidebarWidget* current() const;

private:
    prey::TPreyStorage* m_storage;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TKillTrackerWidgetController> m_controller;
};

}
}
