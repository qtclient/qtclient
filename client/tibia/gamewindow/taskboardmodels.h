#pragma once

#include <cip_gen.h>

#include "taskboard/monsterlistmodelsortfilterproxy.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>

namespace tibia {
namespace taskboard {

struct BountyTaskRow {
    int taskIndex = 0;
    int raceId = 0;
    QString caption;
    QString killText;
    int rewardXp = 0;
    int rewardBountyPoints = 0;
    int rewardDust = 0;
    bool canAccept = false;
    bool canClaim = false;
    QString taskTierImageSource;
    QString taskTierIconSource;
    QString isEliteIconSource;
};

class TBountyTasksListModel final : public cip::tibia::taskboard::TBountyTasksListModel
{
    Q_OBJECT
public:
    enum Roles {
        TaskIndexRole = Qt::UserRole + 1,
        CaptionRole,
        RaceIdRole,
        KillTextRole,
        RewardXpRole,
        RewardBountyPointsRole,
        RewardDustRole,
        CanAcceptRole,
        CanClaimRole,
        TaskTierImageSourceRole,
        TaskTierIconSourceRole,
        IsEliteIconSourceRole,
    };

    explicit TBountyTasksListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<BountyTaskRow> rows);
    /// Update rewardDust for all rows when the Elite checkbox toggles (OTC has no dust wire).
    void setEliteDust(bool isElite, int cipDifficulty);

private:
    QVector<BountyTaskRow> m_rows;
};

struct MonsterKillTaskRow {
    int raceId = 0;
    QString caption;
    int currentKillAmount = 0;
    int requiredAmount = 0;
};

class TMonsterKillTasksListModel final : public cip::tibia::taskboard::TMonsterKillTasksListModel
{
    Q_OBJECT
public:
    enum Roles {
        RaceIdRole = Qt::UserRole + 1,
        CaptionRole,
        CurrentKillAmountRole,
        RequiredAmountRole,
    };

    explicit TMonsterKillTasksListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<MonsterKillTaskRow> rows);

private:
    QVector<MonsterKillTaskRow> m_rows;
};

struct DeliveryTaskRow {
    int taskIndex = 0;
    int typeId = 0;
    QString caption;
    int availableAmount = 0;
    int requiredAmount = 0;
    bool delivered = false;
};

class TDeliveryTasksListModel final : public cip::tibia::taskboard::TDeliveryTasksListModel
{
    Q_OBJECT
public:
    enum Roles {
        TaskIndexRole = Qt::UserRole + 1,
        TypeIdRole,
        CaptionRole,
        AvailableAmountRole,
        RequiredAmountRole,
        DeliveredRole,
    };

    explicit TDeliveryTasksListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<DeliveryTaskRow> rows);

private:
    QVector<DeliveryTaskRow> m_rows;
};

struct PreferredMonsterSlotRow {
    int slotIndex = 0;
    bool isUnlocked = false;
    bool canBeUnlocked = false;
    int unlockCost = 0;
    int preferredMonsterRaceId = 0;
    int avoidedMonsterRaceId = 0;
    QString preferredMonsterRaceName;
    QString avoidedMonsterRaceName;
};

/// CIP PreferredMonsterSlotPanel.qml roles.
class TBountyTasksPreferredMonsterListModel final : public cip::tibia::taskboard::TBountyTasksPreferredMonsterListModel
{
    Q_OBJECT
public:
    enum Roles {
        SlotIndexRole = Qt::UserRole + 1,
        IsUnlockedRole,
        CanBeUnlockedRole,
        UnlockCostRole,
        PreferredMonsterRaceIdRole,
        AvoidedMonsterRaceIdRole,
        PreferredMonsterRaceNameRole,
        AvoidedMonsterRaceNameRole,
    };

    explicit TBountyTasksPreferredMonsterListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<PreferredMonsterSlotRow> rows);
    Q_INVOKABLE bool isMonsterRaceAssignable(int RaceId) override;
    Q_INVOKABLE void clearAssignableMonster() override;

private:
    QVector<PreferredMonsterSlotRow> m_rows;
};

}
}
