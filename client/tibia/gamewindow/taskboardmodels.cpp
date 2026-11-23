#include "taskboardmodels.h"

#include <QSortFilterProxyModel>

#include <algorithm>

namespace tibia::taskboard {

TBountyTasksListModel::TBountyTasksListModel(QObject* parent)
    : cip::tibia::taskboard::TBountyTasksListModel(parent)
{
}

int TBountyTasksListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TBountyTasksListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const BountyTaskRow& row = m_rows.at(index.row());
    switch (role) {
    case TaskIndexRole:
        return row.taskIndex;
    case CaptionRole:
        return row.caption;
    case RaceIdRole:
        return row.raceId;
    case KillTextRole:
        return row.killText;
    case RewardXpRole:
        return row.rewardXp;
    case RewardBountyPointsRole:
        return row.rewardBountyPoints;
    case RewardDustRole:
        return row.rewardDust;
    case CanAcceptRole:
        return row.canAccept;
    case CanClaimRole:
        return row.canClaim;
    case TaskTierImageSourceRole:
        return row.taskTierImageSource;
    case TaskTierIconSourceRole:
        return row.taskTierIconSource;
    case IsEliteIconSourceRole:
        return row.isEliteIconSource;
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
        { RewardDustRole, "rewardDust" },
        { CanAcceptRole, "canAccept" },
        { CanClaimRole, "canClaim" },
        { TaskTierImageSourceRole, "taskTierImageSource" },
        { TaskTierIconSourceRole, "taskTierIconSource" },
        { IsEliteIconSourceRole, "isEliteIconSource" },
    };
}

void TBountyTasksListModel::setRows(QVector<BountyTaskRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

void TBountyTasksListModel::setEliteDust(const bool isElite, const int cipDifficulty)
{
    if (m_rows.isEmpty())
        return;

    static constexpr int kBaseByDifficulty[] = { 3, 7, 16, 27 };
    const int base = kBaseByDifficulty[std::clamp(cipDifficulty, 0, 3)];
    const auto dustForIcon = [base, isElite](const QString& icon) -> int {
        if (!isElite)
            return 0;
        if (icon.contains(QStringLiteral("platinum")))
            return base * 8;
        if (icon.contains(QStringLiteral("gold")))
            return base * 4;
        if (icon.contains(QStringLiteral("silver")))
            return base * 2;
        return base;
    };

    for (int i = 0; i < m_rows.size(); ++i) {
        const int next = dustForIcon(m_rows[i].taskTierIconSource);
        if (m_rows[i].rewardDust == next)
            continue;
        m_rows[i].rewardDust = next;
        const QModelIndex idx = index(i, 0);
        Q_EMIT dataChanged(idx, idx, { RewardDustRole });
    }
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
        return row.caption;
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
        return row.caption;
    case AvailableAmountRole:
        return row.availableAmount;
    case RequiredAmountRole:
        return row.requiredAmount;
    case DeliveredRole:
        return row.delivered;
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
        return row.preferredMonsterRaceName;
    case AvoidedMonsterRaceNameRole:
        return row.avoidedMonsterRaceName;
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
    return RaceId > 0;
}

void TBountyTasksPreferredMonsterListModel::clearAssignableMonster()
{
    setAssignableMonsterRaceId(0);
}

}
