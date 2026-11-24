#pragma once

#include <cip_gen.h>

#include "shared/simplevectorabstractlistmodel.h"
#include "taskboard/monsterlistmodelsortfilterproxy.h"
#include "taskboard/taskboardbountytasksstorage.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>

namespace tibia {
namespace taskboard {

/// The bounty board's tasks, one row per task of the bounty data.
class TBountyTasksListModel final
    : public shared::TSimpleVectorAbstractListModel<TBountyTask, cip::tibia::taskboard::TBountyTasksListModel>
{
    Q_OBJECT
public:
    enum Roles {
        TaskIndexRole = Qt::UserRole + 1,
        CaptionRole,
        RaceIdRole,
        KillTextRole,
        /// The experience of the task's mode (elite or normal).
        RewardXpRole,
        /// The elite experience; it has no role name.
        RewardXpEliteRole,
        RewardBountyPointsRole,
        /// The dust of the task whether it is elite or not.
        RewardDustEliteRole,
        /// The dust of an elite task, 0 otherwise.
        RewardDustRole,
        CanAcceptRole,
        CanClaimRole,
        TaskTierImageSourceRole,
        TaskTierIconSourceRole,
        IsEliteIconSourceRole,
    };

    explicit TBountyTasksListModel(QObject* parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant itemData(const TBountyTask& task, int role) const override;
};

/// One kill task of the weekly board; race 0 is "any creature".
struct MonsterKillTaskRow {
    int raceId = 0;
    int requiredAmount = 0;
    int currentKillAmount = 0;
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

/// One delivery task of the weekly board; state 1 is delivered.
struct DeliveryTaskRow {
    int taskIndex = 0;
    int typeId = 0;
    int requiredAmount = 0;
    int availableAmount = 0;
    int state = 0;
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
    /// Race 0 only while no slot exists; any other race unless a slot
    /// prefers or avoids it already.
    Q_INVOKABLE bool isMonsterRaceAssignable(int RaceId) override;
    /// Race 0, always announced.
    Q_INVOKABLE void clearAssignableMonster() override;

private:
    QVector<PreferredMonsterSlotRow> m_rows;
};

}
}
