#pragma once

#include <memory>

#include <cip_gen.h>

#include <QObject>

#include <cstdint>
#include <vector>

namespace tibia {
namespace taskboard {

/// A weekly kill task: a race, its required and its current kills.
struct TWeeklyKillTask
{
    uint16_t raceId = 0;
    uint16_t requiredKills = 0;
    uint16_t currentKills = 0;
};

/// A weekly delivery task (state 0 open, 1 delivered).
struct TWeeklyDeliveryTask
{
    uint8_t slotIndex = 0;
    uint16_t objectTypeId = 0;
    uint32_t requiredAmount = 0;
    uint32_t availableAmount = 0;
    int state = 0;
};

struct TWeeklyTaskData
{
    uint16_t anyCreatureRequiredKills = 0;
    uint16_t anyCreatureCurrentKills = 0;
    std::vector<TWeeklyKillTask> killTasks;
    std::vector<TWeeklyDeliveryTask> deliveryTasks;
    uint8_t difficulty = 0;
    uint32_t rewardExperiencePerKillTask = 0;
    uint32_t rewardExperiencePerDeliveryTask = 0;
    uint32_t completedKillTasks = 0;
    uint32_t completedDeliveryTasks = 0;
    /// 1 once this week's results are in.
    int progressFinished = 0;
    uint8_t maxUnlockedDifficulty = 0;
    uint32_t resetTime = 0;
    bool permanentExpansion = false;
    uint32_t rewardHuntingTaskPoints = 0;
    uint32_t rewardSoulSeals = 0;
};

/// The weekly tab's data, replaced as a whole by every weekly board message.
class TTaskboardWeeklyTasksStorage final : public cip::tibia::taskboard::TTaskboardWeeklyTasksStorage
{
    Q_OBJECT
public:
    explicit TTaskboardWeeklyTasksStorage(QObject* parent = nullptr);

    void setWeeklyTaskData(TWeeklyTaskData data);
    const TWeeklyTaskData& weeklyTaskData() const { return m_data; }

Q_SIGNALS:
    void weeklyTaskDataChanged(const tibia::taskboard::TWeeklyTaskData& WeeklyTaskData);

private:
    TWeeklyTaskData m_data;
};

}
}
