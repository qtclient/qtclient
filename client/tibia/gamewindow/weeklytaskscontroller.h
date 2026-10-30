#pragma once

#include "gamewindow/taskboardmodels.h"

#include <cip_gen.h>

#include <map>
#include <string>
#include <vector>

namespace tibia::gamewindow {

class TWeeklyTasksController final : public cip::tibia::gamewindow::TWeeklyTasksController
{
    Q_OBJECT
public:
    explicit TWeeklyTasksController(QObject* parent = nullptr);

    void applyWeeklyData(const std::map<std::string, uint32_t>& header,
                         const std::vector<std::map<std::string, uint32_t>>& monsters,
                         const std::vector<std::map<std::string, uint32_t>>& items);

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;

public:
    Q_INVOKABLE void openPermanentWeeklyTaskExpansionUnlockStore() override;
    Q_INVOKABLE void deliverTask(int taskIndex) override;
    Q_INVOKABLE void selectDifficulty(int difficulty) override;
    Q_INVOKABLE int getRequiredLevelForDifficulty(int difficulty) override;
    Q_INVOKABLE QVariantList getTaskRewardFactors() override;
    Q_INVOKABLE QVariantList getTaskRewardCounts() override;
    Q_INVOKABLE void onDeliveryItemClicked(int TypeID, int MouseButtonRaw, int KeyboardModifiersRaw) override;

private:
    void notifyChanged();
    static int htpPerKillTask(int cipDifficulty);
    static int htpMultiplier(int completedTasks);
    static uint16_t toLuaDifficulty(int cipDifficulty);

    taskboard::TMonsterKillTasksListModel* m_killModel = nullptr;
    taskboard::TDeliveryTasksListModel* m_deliveryModel = nullptr;
};

}
