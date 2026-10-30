#include "weeklytaskscontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QString>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

uint32_t mapU32(const std::map<std::string, uint32_t>& map, const char* key, const uint32_t fallback = 0)
{
    const auto it = map.find(key);
    return it == map.end() ? fallback : it->second;
}

QString raceCaption(const uint16_t raceId)
{
    if (raceId == 0)
        return QStringLiteral("Any Creature");
    const RaceType& race = g_things.getRaceData(raceId);
    if (race.name.empty())
        return QStringLiteral("Unknown");
    QString name = QString::fromStdString(race.name);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

QString itemCaption(const uint16_t itemId)
{
    const ThingTypePtr type = g_things.getThingType(itemId, ThingCategoryItem);
    if (!type)
        return QString::number(itemId);
    const std::string name = type->getName();
    if (name.empty())
        return QString::number(itemId);
    QString text = QString::fromStdString(name);
    if (!text.isEmpty())
        text[0] = text[0].toUpper();
    return text;
}

}

TWeeklyTasksController::TWeeklyTasksController(QObject* parent)
    : cip::tibia::gamewindow::TWeeklyTasksController(parent)
    , m_killModel(new taskboard::TMonsterKillTasksListModel(this))
    , m_deliveryModel(new taskboard::TDeliveryTasksListModel(this))
{
    m_monsterKillTasks = m_killModel;
    m_deliveryTasks = m_deliveryModel;
    m_taskRewardFactors = getTaskRewardFactors();
    m_taskRewardCounts = getTaskRewardCounts();
    m_huntingTaskPointsPerDeliverTask = 75;
    m_huntingTaskPointsMultiplicatorForFinishedTasks = 1.0;
}

void TWeeklyTasksController::showDialogTab()
{
    g_game.weeklyTaskAction(Otc::WEEKLY_ACTION_REFRESH_DATA);
}

void TWeeklyTasksController::hideDialogTab() {}

int TWeeklyTasksController::htpPerKillTask(const int cipDifficulty)
{
    switch (std::clamp(cipDifficulty, 0, 3)) {
    case 1:
        return 50;
    case 2:
        return 100;
    case 3:
        return 110;
    default:
        return 25;
    }
}

int TWeeklyTasksController::htpMultiplier(const int completedTasks)
{
    if (completedTasks >= 17)
        return 8;
    if (completedTasks >= 13)
        return 5;
    if (completedTasks >= 9)
        return 3;
    if (completedTasks >= 5)
        return 2;
    return 1;
}

uint16_t TWeeklyTasksController::toLuaDifficulty(const int cipDifficulty)
{
    return static_cast<uint16_t>(std::clamp(cipDifficulty, 0, 3) + 1);
}

void TWeeklyTasksController::applyWeeklyData(const std::map<std::string, uint32_t>& header,
                                               const std::vector<std::map<std::string, uint32_t>>& monsters,
                                               const std::vector<std::map<std::string, uint32_t>>& items)
{
    const int luaDifficulty = static_cast<int>(mapU32(header, "difficulty"));
    const bool weeklyFinished = mapU32(header, "weeklyProgressFinished") != 0;
    const bool needsDifficultyPick = luaDifficulty == 0 || weeklyFinished;
    // unlockedDifficulty is stored as Lua 1..4 by the parser.
    const int unlockedLua = static_cast<int>(std::clamp<uint32_t>(mapU32(header, "unlockedDifficulty", 1), 1, 4));
    const int cipDifficulty = luaDifficulty > 0 ? std::clamp(luaDifficulty - 1, 0, 3) : 0;

    m_displayResults = needsDifficultyPick;
    m_showResultsInResultsPanel = weeklyFinished ? 1 : 0;
    m_maxDifficultyLevel = unlockedLua - 1;
    m_showPermanentWeeklyTaskExpansionUnlockButton = mapU32(header, "extraSlot") == 0;

    m_rewardXPPerKillTask = static_cast<int>(mapU32(header, "maxExperience"));
    m_rewardXPPerDeliveryTask = static_cast<int>(mapU32(header, "maxDeliveryExperience"));
    m_rewardHuntingTaskPoints = static_cast<int>(mapU32(header, "pointsEarned"));
    m_rewardSoulSeals = static_cast<int>(mapU32(header, "soulsealsEarned"));

    m_finishedMonsterKillTasks = static_cast<int>(mapU32(header, "completedKillTasks"));
    m_finishedDeliverTasks = static_cast<int>(mapU32(header, "completedDeliveryTasks"));
    m_monsterKillTasksCompleted = m_finishedMonsterKillTasks;
    m_deliveryTasksCompleted = m_finishedDeliverTasks;

    m_huntingTaskPointsPerMonsterKillTask = htpPerKillTask(cipDifficulty);
    m_huntingTaskPointsPerDeliverTask = 75;
    m_huntingTaskPointsMultiplicatorForFinishedTasks =
        static_cast<double>(htpMultiplier(m_finishedMonsterKillTasks + m_finishedDeliverTasks));

    const int remainingDays = static_cast<int>(mapU32(header, "remainingDays"));
    m_remainingTimeLessThanOneDayLeft = remainingDays <= 0;
    m_remainingTimeString = m_remainingTimeLessThanOneDayLeft
        ? QStringLiteral("Less than one day")
        : (remainingDays == 1 ? QStringLiteral("1 day") : QStringLiteral("%1 days").arg(remainingDays));

    QVector<taskboard::MonsterKillTaskRow> killRows;
    killRows.reserve(static_cast<int>(monsters.size()));
    for (const auto& monster : monsters) {
        taskboard::MonsterKillTaskRow row;
        row.raceId = static_cast<int>(mapU32(monster, "raceId"));
        row.currentKillAmount = static_cast<int>(mapU32(monster, "current"));
        row.requiredAmount = static_cast<int>(mapU32(monster, "total"));
        row.caption = raceCaption(static_cast<uint16_t>(row.raceId));
        killRows.push_back(std::move(row));
    }
    m_killModel->setRows(std::move(killRows));
    m_monsterKillTasksCount = m_killModel->rowCount();

    QVector<taskboard::DeliveryTaskRow> deliveryRows;
    deliveryRows.reserve(static_cast<int>(items.size()));
    for (const auto& item : items) {
        taskboard::DeliveryTaskRow row;
        row.taskIndex = static_cast<int>(mapU32(item, "slotIndex"));
        row.typeId = static_cast<int>(mapU32(item, "itemId"));
        row.availableAmount = static_cast<int>(mapU32(item, "current"));
        row.requiredAmount = static_cast<int>(mapU32(item, "total"));
        row.delivered = mapU32(item, "claimed") != 0;
        row.caption = itemCaption(static_cast<uint16_t>(row.typeId));
        deliveryRows.push_back(std::move(row));
    }
    m_deliveryModel->setRows(std::move(deliveryRows));
    m_deliveryTasksCount = m_deliveryModel->rowCount();

    m_taskRewardFactors = getTaskRewardFactors();
    m_taskRewardCounts = getTaskRewardCounts();
    notifyChanged();
}

void TWeeklyTasksController::notifyChanged() { Q_EMIT weeklyTaskDataChanged(); }

void TWeeklyTasksController::openPermanentWeeklyTaskExpansionUnlockStore()
{
    // OPEN_STORE open-action 4 jumps to the offer id (Crystal SubActions.WEEKLY_TASK_EXPANSION).
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 4;
    action->instance = 16;
    input::publish(action);
}

void TWeeklyTasksController::deliverTask(const int taskIndex)
{
    g_game.weeklyTaskAction(Otc::WEEKLY_ACTION_DELIVER_ITEM, static_cast<uint16_t>(std::max(0, taskIndex)));
}

void TWeeklyTasksController::selectDifficulty(const int difficulty)
{
    g_game.weeklyTaskAction(Otc::WEEKLY_ACTION_SELECT_DIFFICULTY, toLuaDifficulty(difficulty));
}

int TWeeklyTasksController::getRequiredLevelForDifficulty(const int difficulty)
{
    switch (std::clamp(difficulty, 0, 3)) {
    case 1:
        return 30;
    case 2:
        return 150;
    case 3:
        return 300;
    default:
        return 8;
    }
}

QVariantList TWeeklyTasksController::getTaskRewardFactors()
{
    return { 1, 2, 3, 5, 8 };
}

QVariantList TWeeklyTasksController::getTaskRewardCounts()
{
    return { 0, 4, 8, 12, 16, 18 };
}

void TWeeklyTasksController::onDeliveryItemClicked(int, int, int) {}

}
