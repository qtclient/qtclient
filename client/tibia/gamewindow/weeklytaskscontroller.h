#pragma once

#include "gamewindow/taskboardmodels.h"
#include "input/gameactionmapper.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <memory>

namespace tibia::game {
class IGameSession;
}
namespace tibia::taskboard {
struct TWeeklyTaskData;
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// The task board's weekly tab (CIP: TWeeklyTasksController). Its properties
/// read the session's weekly task data; a weekly board message refills the
/// kill and delivery task lists. A request to the server (weekly tasks,
/// difficulty) waits for the answer before the next one goes out.
class TWeeklyTasksController final : public cip::tibia::gamewindow::TWeeklyTasksController
{
    Q_OBJECT
public:
    explicit TWeeklyTasksController(std::shared_ptr<input::TGameActionMapper> GameActionMapper, QObject* parent = nullptr);

    /// Takes the session; its weekly tasks storage's changes refill the lists.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    bool displayResults() const override;
    bool showPermanentWeeklyTaskExpansionUnlockButton() const override;
    int rewardHuntingTaskPoints() const override;
    int rewardSoulSeals() const override;
    int rewardXPPerKillTask() const override;
    int rewardXPPerDeliveryTask() const override;
    int showResultsInResultsPanel() const override;
    int maxDifficultyLevel() const override;
    int monsterKillTasksCompleted() const override;
    int monsterKillTasksCount() const override;
    int deliveryTasksCompleted() const override;
    int deliveryTasksCount() const override;
    int finishedMonsterKillTasks() const override;
    int huntingTaskPointsPerMonsterKillTask() const override;
    int finishedDeliverTasks() const override;
    int huntingTaskPointsPerDeliverTask() const override;
    double huntingTaskPointsMultiplicatorForFinishedTasks() const override;
    QVariantList taskRewardFactors() const override;
    QVariantList taskRewardCounts() const override;
    QString remainingTimeString() const override;
    bool remainingTimeLessThanOneDayLeft() const override;

public Q_SLOTS:
    void showDialogTab() override;
    /// Closes the delivery confirmation.
    void hideDialogTab() override;

public:
    /// OPEN_STORE at the useful thing of the permanent weekly task expansion.
    Q_INVOKABLE void openPermanentWeeklyTaskExpansionUnlockStore() override;
    /// Asks to hand in the delivery task's items (amount and capitalized item
    /// name); OK sends the delivery unless a request is pending.
    Q_INVOKABLE void deliverTask(int taskIndex) override;
    Q_INVOKABLE void selectDifficulty(int difficulty) override;
    Q_INVOKABLE int getRequiredLevelForDifficulty(int difficulty) override;
    Q_INVOKABLE QVariantList getTaskRewardFactors() override;
    Q_INVOKABLE QVariantList getTaskRewardCounts() override;
    Q_INVOKABLE void onDeliveryItemClicked(int TypeID, int MouseButtonRaw, int KeyboardModifiersRaw) override;

private:
    /// The session's weekly task data (null without a session).
    const taskboard::TWeeklyTaskData* weeklyTaskData() const;
    /// Seconds until the weekly reset, 0 once it passed.
    uint32_t remainingSeconds() const;

    std::shared_ptr<game::IGameSession> m_gameSession;
    QMetaObject::Connection m_storageConnection;
    /// The delivery confirmation's connections (dropped when it closes).
    shared::TQtConnectionManager m_deliverTaskConfirmationConnections;
    /// Asks before a delivery task's items are handed in.
    TMessageDialogController* m_deliverTaskConfirmationDialog = nullptr;
    taskboard::TMonsterKillTasksListModel* m_killModel = nullptr;
    taskboard::TDeliveryTasksListModel* m_deliveryModel = nullptr;
    /// The game window's mapper: a delivery object click to its game action.
    std::shared_ptr<input::TGameActionMapper> m_pGameActionMapper;
    /// A request went out; the next weekly data message clears it.
    bool m_requestPending = false;
};

}
}
