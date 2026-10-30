#pragma once

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>

namespace tibia::taskboard {

struct BountyTaskRow {
    int taskIndex = 0;
    int raceId = 0;
    QString caption;
    QString killText;
    int rewardXp = 0;
    int rewardBountyPoints = 0;
    bool canAccept = false;
    bool canClaim = false;
    QString taskTierImageSource;
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
        CanAcceptRole,
        CanClaimRole,
        TaskTierImageSourceRole,
    };

    explicit TBountyTasksListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<BountyTaskRow> rows);

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

struct ShopRewardRow {
    int offerIndex = 0;
    int rewardType = 0;
    int id = 0;
    int optionalId = 0;
    QString title;
    QString description;
    int cost = 0;
    QString costString;
    bool canBuy = false;
    bool alreadyOwned = false;
    QString noBuyReasonString;
    bool addOn1 = false;
    bool addOn2 = false;
};

class TTaskboardRewardListModel final : public cip::tibia::taskboard::TTaskboardRewardListModel
{
    Q_OBJECT
public:
    enum Roles {
        RewardTypeRole = Qt::UserRole + 1,
        IdRole,
        OptionalIdRole,
        TitleRole,
        DescriptionRole,
        CostRole,
        CostStringRole,
        CanBuyRole,
        AlreadyOwnedRole,
        NoBuyReasonStringRole,
        AddOn1Role,
        AddOn2Role,
    };

    explicit TTaskboardRewardListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<ShopRewardRow> rows);
    const ShopRewardRow* rowAt(int index) const;

private:
    QVector<ShopRewardRow> m_rows;
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

struct MonsterRaceRow {
    int raceId = 0;
    QString name;
};

/// Source model for the preferred-monster race picker.
class TMonsterListFilterModel final : public cip::tibia::taskboard::TMonsterListFilterModel
{
    Q_OBJECT
public:
    enum Roles {
        RaceIdRole = Qt::UserRole + 1,
        RaceIDRole,
        NameRole,
    };

    explicit TMonsterListFilterModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<MonsterRaceRow> rows);
    int raceIdAt(int row) const;

private:
    QVector<MonsterRaceRow> m_rows;
};

class TMonsterListModelSortFilterProxy final : public cip::tibia::taskboard::TMonsterListModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TMonsterListModelSortFilterProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setNameFilter(const QString& NameFilter) override;
    Q_INVOKABLE QVariant getRaceIDAtIndex(int RowIndex) override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_nameFilter;
};

}
