#include "weeklytaskscontroller.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include "gamewindow/messagedialogcontroller.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "game/gamesessionbase.h"
#include "taskboard/taskboardciptypes.h"
#include "taskboard/taskboardweeklytasksstorage.h"

#include <tibia/game/gamesession.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <iterator>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

/// The weekly task tiers: from this many finished tasks on, the hunting task
/// points are multiplied by the factor. 18 tasks is the last count.
constexpr std::pair<int, double> RewardTiers[] = { { 0, 1.0 }, { 4, 2.0 }, { 8, 3.0 }, { 12, 5.0 }, { 16, 8.0 } };
constexpr int LastRewardCount = 18;
/// Hunting task points per kill task by difficulty, and per delivery task.
constexpr std::pair<int, int> PointsPerKillTask[] = { { 0, 25 }, { 1, 50 }, { 2, 100 }, { 3, 110 } };
constexpr int PointsPerDeliverTask = 75;
/// The level each difficulty needs (difficulties 1 to 3).
constexpr int RequiredLevels[] = { 30, 150, 400 };

QString raceCaption(const uint16_t raceId)
{
    if (raceId == 0)
        return QStringLiteral("Any Creature");
    const RaceType& race = tibia::game::raceData(raceId);
    if (race.name.empty())
        return QStringLiteral("Unknown");
    QString name = QString::fromStdString(race.name);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

QString itemCaption(const uint16_t itemId)
{
    const TAppearanceTypePtr type = tibia::game::thingType(itemId, ThingCategoryItem);
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
    , m_deliverTaskConfirmationDialog(new TMessageDialogController(this))
{
    m_monsterKillTasks = m_killModel;
    m_deliveryTasks = m_deliveryModel;
    for (const auto& [count, factor] : RewardTiers) {
        m_taskRewardFactors.append(factor);
        m_taskRewardCounts.append(count);
    }
    m_taskRewardCounts.append(LastRewardCount);
}

void TWeeklyTasksController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    hideDialogTab();
    disconnect(m_storageConnection);
    m_gameSession = GameSession;
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    taskboard::TTaskboardWeeklyTasksStorage* storage = session ? session->taskboardWeeklyTasksStorage().get() : nullptr;
    if (!storage)
        return;
    // A weekly board message: the "any creature" kill task (when it has a
    // goal) comes first, then the race kill tasks; the delivery tasks as sent.
    m_storageConnection = connect(storage, &taskboard::TTaskboardWeeklyTasksStorage::weeklyTaskDataChanged, this,
            [this](const taskboard::TWeeklyTaskData& data) {
                QVector<taskboard::MonsterKillTaskRow> killRows;
                if (data.anyCreatureRequiredKills != 0) {
                    taskboard::MonsterKillTaskRow row;
                    row.raceId = 0;
                    row.requiredAmount = data.anyCreatureRequiredKills;
                    row.currentKillAmount = data.anyCreatureCurrentKills;
                    row.caption = raceCaption(0);
                    killRows.push_back(std::move(row));
                }
                for (const taskboard::TWeeklyKillTask& task : data.killTasks) {
                    taskboard::MonsterKillTaskRow row;
                    row.raceId = task.raceId;
                    row.requiredAmount = task.requiredKills;
                    row.currentKillAmount = task.currentKills;
                    row.caption = raceCaption(task.raceId);
                    killRows.push_back(std::move(row));
                }
                m_killModel->setRows(std::move(killRows));
                QVector<taskboard::DeliveryTaskRow> deliveryRows;
                for (const taskboard::TWeeklyDeliveryTask& task : data.deliveryTasks) {
                    taskboard::DeliveryTaskRow row;
                    row.taskIndex = task.slotIndex;
                    row.typeId = task.objectTypeId;
                    row.availableAmount = static_cast<int>(task.availableAmount);
                    row.requiredAmount = static_cast<int>(task.requiredAmount);
                    row.delivered = task.state != 0;
                    row.caption = itemCaption(task.objectTypeId);
                    deliveryRows.push_back(std::move(row));
                }
                m_deliveryModel->setRows(std::move(deliveryRows));
                m_requestPending = false;
                Q_EMIT weeklyTaskDataChanged();
            });
}

const taskboard::TWeeklyTaskData* TWeeklyTasksController::weeklyTaskData() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    const taskboard::TTaskboardWeeklyTasksStorage* storage = session ? session->taskboardWeeklyTasksStorage().get() : nullptr;
    return storage ? &storage->weeklyTaskData() : nullptr;
}

uint32_t TWeeklyTasksController::remainingSeconds() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    if (!data)
        return 0;
    const auto now = static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch());
    return data->resetTime > now ? data->resetTime - now : 0;
}

bool TWeeklyTasksController::displayResults() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data && data->progressFinished == 1;
}

bool TWeeklyTasksController::showPermanentWeeklyTaskExpansionUnlockButton() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return !data || !data->permanentExpansion;
}

int TWeeklyTasksController::rewardHuntingTaskPoints() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->rewardHuntingTaskPoints) : 0;
}

int TWeeklyTasksController::rewardSoulSeals() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->rewardSoulSeals) : 0;
}

int TWeeklyTasksController::rewardXPPerKillTask() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->rewardExperiencePerKillTask) : 0;
}

int TWeeklyTasksController::rewardXPPerDeliveryTask() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->rewardExperiencePerDeliveryTask) : 0;
}

int TWeeklyTasksController::showResultsInResultsPanel() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data && data->progressFinished == 1 && !data->killTasks.empty() && !data->deliveryTasks.empty();
}

int TWeeklyTasksController::maxDifficultyLevel() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? data->maxUnlockedDifficulty : 0;
}

int TWeeklyTasksController::monsterKillTasksCompleted() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->completedKillTasks) : 0;
}

int TWeeklyTasksController::monsterKillTasksCount() const
{
    // The race kill tasks and the "any creature" one.
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->killTasks.size()) + 1 : 0;
}

int TWeeklyTasksController::deliveryTasksCompleted() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->completedDeliveryTasks) : 0;
}

int TWeeklyTasksController::deliveryTasksCount() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->deliveryTasks.size()) : 0;
}

int TWeeklyTasksController::finishedMonsterKillTasks() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->completedKillTasks) : 0;
}

int TWeeklyTasksController::huntingTaskPointsPerMonsterKillTask() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    if (!data)
        return 0;
    for (const auto& [difficulty, points] : PointsPerKillTask) {
        if (difficulty == data->difficulty)
            return points;
    }
    return 0;
}

int TWeeklyTasksController::finishedDeliverTasks() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    return data ? static_cast<int>(data->completedDeliveryTasks) : 0;
}

int TWeeklyTasksController::huntingTaskPointsPerDeliverTask() const { return PointsPerDeliverTask; }

double TWeeklyTasksController::huntingTaskPointsMultiplicatorForFinishedTasks() const
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    if (!data)
        return 0.0;
    // The tier whose count range holds the finished tasks; the last tier
    // from its count on.
    const uint64_t finished = static_cast<uint64_t>(data->completedDeliveryTasks) + data->completedKillTasks;
    constexpr size_t TierCount = std::size(RewardTiers);
    for (size_t i = 0; i + 1 < TierCount; ++i) {
        if (static_cast<uint64_t>(RewardTiers[i].first) <= finished && finished < static_cast<uint64_t>(RewardTiers[i + 1].first))
            return RewardTiers[i].second;
    }
    return RewardTiers[TierCount - 1].second;
}

QVariantList TWeeklyTasksController::taskRewardFactors() const { return m_taskRewardFactors; }
QVariantList TWeeklyTasksController::taskRewardCounts() const { return m_taskRewardCounts; }

QString TWeeklyTasksController::remainingTimeString() const
{
    const uint32_t seconds = remainingSeconds();
    if (seconds == 0)
        return qtTrId("taskboard_weekly_tasks_remaining_hours").arg(0);
    if (seconds < 86400)
        return qtTrId("taskboard_weekly_tasks_remaining_hours").arg(seconds / 3600);
    return qtTrId("taskboard_weekly_tasks_remaining_days").arg(seconds / 86400);
}

bool TWeeklyTasksController::remainingTimeLessThanOneDayLeft() const
{
    const uint32_t seconds = remainingSeconds();
    return seconds - 1U < 86399U;
}

void TWeeklyTasksController::showDialogTab()
{
    if (!m_gameSession || m_requestPending)
        return;
    taskboard::TTaskboardProtocolMessageHandler::requestWeeklyTasks();
    m_requestPending = true;
}

void TWeeklyTasksController::hideDialogTab()
{
    m_deliverTaskConfirmationDialog->dialog()->close();
    m_deliverTaskConfirmationConnections.disconnectAll();
}

void TWeeklyTasksController::openPermanentWeeklyTaskExpansionUnlockStore()
{
    // Open action 3 (useful thing), useful thing 13.
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenStore>(3, 13));
}

void TWeeklyTasksController::deliverTask(const int taskIndex)
{
    const taskboard::TWeeklyTaskData* data = weeklyTaskData();
    if (!data)
        return;
    // Host: an index outside the delivery tasks is ignored.
    if (taskIndex < 0 || static_cast<size_t>(taskIndex) >= data->deliveryTasks.size())
        return;
    const taskboard::TWeeklyDeliveryTask& task = data->deliveryTasks[static_cast<size_t>(taskIndex)];
    m_deliverTaskConfirmationDialog->setMessageDialogTextFormat(TMessageDialogController::RICH_TEXT);
    qmlcomponents::TAppearanceTypeHelperQmlService appearanceTypeHelper;
    const QString itemName = cyclopedia::capitalizeWords(
        QString::fromStdString(appearanceTypeHelper.getObjectAppearanceTypeNameForID(task.objectTypeId).toStdString()));
    const QString caption = qtTrId("taskboard_deliver_task_confirmation_caption");
    const QString text = qtTrId("taskboard_deliver_task_confirmation_text").arg(task.requiredAmount).arg(itemName);
    m_deliverTaskConfirmationDialog->setButtons(okCancelDialogButtons());
    m_deliverTaskConfirmationDialog->showMessage(caption, text);
    m_deliverTaskConfirmationConnections.add(
        connect(m_deliverTaskConfirmationDialog, &TMessageDialogController::acceptClicked, this, [this, taskIndex](bool) {
            m_deliverTaskConfirmationDialog->dialog()->close();
            m_deliverTaskConfirmationConnections.disconnectAll();
            if (!m_gameSession || m_requestPending)
                return;
            taskboard::TTaskboardProtocolMessageHandler::deliverWeeklyTask(taskIndex);
            m_requestPending = true;
        }));
    m_deliverTaskConfirmationConnections.add(connect(m_deliverTaskConfirmationDialog, &TMessageDialogController::discardClicked, this, [this](bool) {
        m_deliverTaskConfirmationConnections.disconnectAll();
        m_deliverTaskConfirmationDialog->dialog()->close();
    }));
}

void TWeeklyTasksController::selectDifficulty(const int difficulty)
{
    if (!m_gameSession || m_requestPending)
        return;
    taskboard::TTaskboardProtocolMessageHandler::selectWeeklyTaskDifficulty(difficulty);
    m_requestPending = true;
}

int TWeeklyTasksController::getRequiredLevelForDifficulty(const int difficulty)
{
    const auto index = static_cast<uint8_t>(static_cast<uint8_t>(difficulty) - 1U);
    if (!m_gameSession || index >= std::size(RequiredLevels))
        return 0;
    return RequiredLevels[index];
}

QVariantList TWeeklyTasksController::getTaskRewardFactors() { return m_taskRewardFactors; }

QVariantList TWeeklyTasksController::getTaskRewardCounts() { return m_taskRewardCounts; }

void TWeeklyTasksController::onDeliveryItemClicked(int, int, int) {}

}
