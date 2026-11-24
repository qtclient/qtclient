#include "taskboardmodels.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"
#include <tibia/assets/things.h>

#include <QSortFilterProxyModel>

#include <algorithm>

namespace tibia::taskboard {

namespace {

/// The QML AppearanceTypeHelper service the delivery captions come from.
qmlcomponents::TAppearanceTypeHelperQmlService* appearanceTypeHelper()
{
    return qmlcomponents::TAppearanceTypeHelperQmlService::instance();
}

}

TBountyTasksListModel::TBountyTasksListModel(QObject* parent)
    : TSimpleVectorAbstractListModel(parent)
{
}

QVariant TBountyTasksListModel::itemData(const TBountyTask& task, const int role) const
{
    switch (role) {
    case TaskIndexRole:
        return uint(task.index);
    case CaptionRole:
        return cyclopedia::capitalizeWords(QString::fromStdString(tibia::appearances::TAppearancesManager::instance().getRaceData(task.raceId).name));
    case RaceIdRole:
        return uint(task.raceId);
    case KillTextRole:
        return qtTrId("taskboard_kill_text_format").arg(task.currentKills).arg(task.requiredKills);
    case RewardXpRole:
        return uint(task.elite ? task.rewardExperienceElite : task.rewardExperience);
    case RewardXpEliteRole:
        return uint(task.rewardExperienceElite);
    case RewardBountyPointsRole:
        return uint(task.rewardBountyPoints);
    case RewardDustEliteRole:
        return uint(task.rewardDust);
    case RewardDustRole:
        return task.elite ? uint(task.rewardDust) : 0u;
    case CanAcceptRole:
        return task.claimState == EBountyTaskClaimState::Available;
    case CanClaimRole:
        return task.claimState == EBountyTaskClaimState::Claimable;
    case TaskTierImageSourceRole:
        return QStringLiteral("/images/taskboard/backdrop-bounty-tasks.png");
    case TaskTierIconSourceRole:
        switch (task.grade) {
        case 1:
            return QStringLiteral("/images/taskboard/icon-bounty-task-silver.png");
        case 2:
            return QStringLiteral("/images/taskboard/icon-bounty-task-gold.png");
        case 3:
            return QStringLiteral("/images/taskboard/icon-bounty-task-platinum.png");
        default:
            return QString();
        }
    case IsEliteIconSourceRole:
        return task.elite ? QStringLiteral("/images/taskboard/icon-difficulty.png") : QString();
    default:
        return {};
    }
}

QHash<int, QByteArray> TBountyTasksListModel::roleNames() const
{
    return {
        { TaskIndexRole, "taskIndex" },
        { CaptionRole, "caption" },
        { RaceIdRole, "raceId" },
        { KillTextRole, "killText" },
        { RewardXpRole, "rewardXp" },
        { RewardBountyPointsRole, "rewardBountyPoints" },
        { RewardDustEliteRole, "rewardDustElite" },
        { RewardDustRole, "rewardDust" },
        { CanAcceptRole, "canAccept" },
        { CanClaimRole, "canClaim" },
        { TaskTierImageSourceRole, "taskTierImageSource" },
        { TaskTierIconSourceRole, "taskTierIconSource" },
        { IsEliteIconSourceRole, "isEliteIconSource" },
    };
}

TMonsterKillTasksListModel::TMonsterKillTasksListModel(QObject* parent)
    : cip::tibia::taskboard::TMonsterKillTasksListModel(parent)
{
}

int TMonsterKillTasksListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TMonsterKillTasksListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const MonsterKillTaskRow& row = m_rows.at(index.row());
    switch (role) {
    case RaceIdRole:
        return row.raceId;
    case CaptionRole:
        if (row.raceId == 0)
            return qtTrId("kill_tracker_arbitrary_monster_name");
        return cyclopedia::bossName(row.raceId);
    case CurrentKillAmountRole:
        return row.currentKillAmount;
    case RequiredAmountRole:
        return row.requiredAmount;
    default:
        return {};
    }
}

QHash<int, QByteArray> TMonsterKillTasksListModel::roleNames() const
{
    return {
        { RaceIdRole, "raceId" },
        { CaptionRole, "caption" },
        { CurrentKillAmountRole, "currentKillAmount" },
        { RequiredAmountRole, "requiredAmount" },
    };
}

void TMonsterKillTasksListModel::setRows(QVector<MonsterKillTaskRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

TDeliveryTasksListModel::TDeliveryTasksListModel(QObject* parent)
    : cip::tibia::taskboard::TDeliveryTasksListModel(parent)
{
}

int TDeliveryTasksListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TDeliveryTasksListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const DeliveryTaskRow& row = m_rows.at(index.row());
    switch (role) {
    case TaskIndexRole:
        return row.taskIndex;
    case TypeIdRole:
        return row.typeId;
    case CaptionRole:
        return cyclopedia::capitalizeWords(appearanceTypeHelper()->getObjectAppearanceTypeNameForID(row.typeId));
    case AvailableAmountRole:
        return row.availableAmount;
    case RequiredAmountRole:
        return row.requiredAmount;
    case DeliveredRole:
        return row.state == 1;
    default:
        return {};
    }
}

QHash<int, QByteArray> TDeliveryTasksListModel::roleNames() const
{
    return {
        { TaskIndexRole, "taskIndex" },
        { TypeIdRole, "typeId" },
        { CaptionRole, "caption" },
        { AvailableAmountRole, "availableAmount" },
        { RequiredAmountRole, "requiredAmount" },
        { DeliveredRole, "delivered" },
    };
}

void TDeliveryTasksListModel::setRows(QVector<DeliveryTaskRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

TBountyTasksPreferredMonsterListModel::TBountyTasksPreferredMonsterListModel(QObject* parent)
    : cip::tibia::taskboard::TBountyTasksPreferredMonsterListModel(parent)
{
    m_assignableMonsterRaceId = 0;
    m_clearCost = 10;
}

int TBountyTasksPreferredMonsterListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TBountyTasksPreferredMonsterListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const PreferredMonsterSlotRow& row = m_rows.at(index.row());
    switch (role) {
    case SlotIndexRole:
        return row.slotIndex;
    case IsUnlockedRole:
        return row.isUnlocked;
    case CanBeUnlockedRole:
        return row.canBeUnlocked;
    case UnlockCostRole:
        return row.unlockCost;
    case PreferredMonsterRaceIdRole:
        return row.preferredMonsterRaceId;
    case AvoidedMonsterRaceIdRole:
        return row.avoidedMonsterRaceId;
    case PreferredMonsterRaceNameRole:
        return cyclopedia::bossName(row.preferredMonsterRaceId);
    case AvoidedMonsterRaceNameRole:
        return cyclopedia::bossName(row.avoidedMonsterRaceId);
    default:
        return {};
    }
}

QHash<int, QByteArray> TBountyTasksPreferredMonsterListModel::roleNames() const
{
    return {
        { SlotIndexRole, "slotIndex" },
        { IsUnlockedRole, "isUnlocked" },
        { CanBeUnlockedRole, "canBeUnlocked" },
        { UnlockCostRole, "unlockCost" },
        { PreferredMonsterRaceIdRole, "preferredMonsterRaceId" },
        { AvoidedMonsterRaceIdRole, "avoidedMonsterRaceId" },
        { PreferredMonsterRaceNameRole, "preferredMonsterRaceName" },
        { AvoidedMonsterRaceNameRole, "avoidedMonsterRaceName" },
    };
}

void TBountyTasksPreferredMonsterListModel::setRows(QVector<PreferredMonsterSlotRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

bool TBountyTasksPreferredMonsterListModel::isMonsterRaceAssignable(const int RaceId)
{
    if (RaceId == 0 || m_rows.isEmpty())
        return m_rows.isEmpty();
    return std::none_of(m_rows.begin(), m_rows.end(), [RaceId](const PreferredMonsterSlotRow& row) {
        return row.preferredMonsterRaceId == RaceId || row.avoidedMonsterRaceId == RaceId;
    });
}

void TBountyTasksPreferredMonsterListModel::clearAssignableMonster()
{
    m_assignableMonsterRaceId = 0;
    Q_EMIT assignableMonsterRaceIdChanged();
}

}
