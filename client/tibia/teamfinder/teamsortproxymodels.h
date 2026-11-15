#pragma once

#include "teamfinder/teamfinderciptypes.h"

#include <cip_gen.h>

#include <QObject>
#include <QSortFilterProxyModel>

#include <vector>

namespace tibia {

/// A list model over a vector of items. Rows are the items; `itemData` maps one item and role.
template<typename Base, typename T>
class TSimpleVectorListModel : public Base
{
public:
    using Base::Base;

    int rowCount(const QModelIndex& = {}) const override { return static_cast<int>(m_items.size()); }

    QVariant data(const QModelIndex& index, const int role) const override
    {
        if (index.row() >= 0 && index.column() >= 0 && index.model() && index.row() < static_cast<int>(m_items.size()))
            return itemData(m_items[static_cast<size_t>(index.row())], role);
        return defaultData(role);
    }

    /// Replaces the items: rows are removed / inserted for the size difference, the rest reported changed.
    virtual void setItems(const std::vector<T>& Items)
    {
        int count = rowCount();
        const int newCount = static_cast<int>(Items.size());
        if (newCount < count) {
            this->beginRemoveRows({}, newCount, count - 1);
            m_items = Items;
            this->endRemoveRows();
            count = newCount;
        } else if (count < newCount) {
            this->beginInsertRows({}, count, newCount - 1);
            m_items = Items;
            this->endInsertRows();
        } else {
            m_items = Items;
        }
        if (count > 0)
            Q_EMIT this->dataChanged(this->index(0, 0), this->index(count - 1, 0));
    }

    void clear() { setItems({}); }
    const std::vector<T>& items() const { return m_items; }

protected:
    virtual QVariant defaultData(int) const { return {}; }
    virtual QVariant itemData(const T& Item, int Role) const = 0;

    std::vector<T> m_items;
};

/// Assemble Team: the own team's members.
class TTeamMembersModel final : public TSimpleVectorListModel<cip::TTeamMembersModel, teamfinder::TTeamMember>
{
public:
    enum Role
    {
        CharacterIdRole = 0x101,
        NameRole,
        LevelRole,
        VocationRole,
        VocationStringRole,
        StatusRole,
        IsLeaderRole,
    };

    using TSimpleVectorListModel::TSimpleVectorListModel;

    QHash<int, QByteArray> roleNames() const override;
    /// The own character's name: that member is the leader. Rows are refreshed when it changes.
    void setCharacterName(const QString& Name);

protected:
    QVariant itemData(const teamfinder::TTeamMember& Member, int Role) const override;

private:
    QString m_characterName;
};

/// One selectable activity of the Assemble Team page.
struct TActivity
{
    teamfinder::TTeamActivity type;
    QString typeName;
    QString name;
    QString alias;
};

/// Assemble Team: the activities a team can be assembled for.
class TActivitiesModel final : public TSimpleVectorListModel<cip::TActivitiesModel, TActivity>
{
public:
    enum Role
    {
        TypeRole = 0x101,
        TypeNameRole,
        NameRole,
        AliasRole,
    };

    using TSimpleVectorListModel::TSimpleVectorListModel;

    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant itemData(const TActivity& Activity, int Role) const override;
};

/// Join Team: the listed teams.
class TTeamsListModel final : public TSimpleVectorListModel<cip::TTeamsListModel, teamfinder::TTeamData>
{
public:
    enum Role
    {
        TeamIdRole = 0x101,
        ActivityTypeRole,
        TypeNameRole,
        NameRole,
        AliasRole,
        MinLevelRole,
        MaxLevelRole,
        LevelRangeRole,
        TeamSizeRole,
        TakenSlotsRole,
        TeamSizeStringRole,
        VocationStringRole,
        StartTimeRole,
        StartTimeStringRole,
        StartTimeDetailStringRole,
        LeaderNameRole,
        StatusRole,
        IsLeaderRole,
    };

    using TSimpleVectorListModel::TSimpleVectorListModel;

    QHash<int, QByteArray> roleNames() const override;
    /// The own character's name: a team it leads reports isLeader. Rows are refreshed when it changes.
    void setCharacterName(const QString& Name);
    /// Names the listed activities; without one the model has no data.
    void setResolver(const teamfinder::TTeamfinderActivityNameResolver* Resolver) { m_resolver = Resolver; }

protected:
    QVariant itemData(const teamfinder::TTeamData& Team, int Role) const override;

private:
    QString m_characterName;
    const teamfinder::TTeamfinderActivityNameResolver* m_resolver = nullptr;
};

/// "Boss" / "Hunt" / "Quest" / "Other" for an activity kind.
QString activityTypeName(int Kind);
/// An activity row: the kind's type name and the resolver's name and alias.
TActivity makeActivity(const teamfinder::TTeamActivity& Type, const teamfinder::TTeamfinderActivityNameResolver& Resolver);
/// "Hunt: Edron" (the kind's type name and the trimmed activity name).
QString activityDisplayName(const teamfinder::TTeamActivity& Activity, const teamfinder::TTeamfinderActivityNameResolver& Resolver);

/// CIP: TTeamsListSortProxyModel (Join Team `teamsList`).
class TTeamsListSortProxyModel final : public cip::TTeamsListSortProxyModel
{
    Q_OBJECT
public:
    explicit TTeamsListSortProxyModel(QObject* parent = nullptr);

    TTeamsListModel* model() const { return m_model; }
    teamfinder::TTeamStartTimeManager* startTime() const { return m_time; }

    int numberOfTeams() const override;
    QString filterLevel() const override;
    void setFilterLevel(const QString& v) override;
    QString minTeamSize() const override;
    void setMinTeamSize(const QString& v) override;
    QString maxTeamSize() const override;
    void setMaxTeamSize(const QString& v) override;
    bool druidWanted() const override { return m_druid; }
    void setDruidWanted(bool v) override;
    bool knightWanted() const override { return m_knight; }
    void setKnightWanted(bool v) override;
    bool paladinWanted() const override { return m_paladin; }
    void setPaladinWanted(bool v) override;
    bool sorcererWanted() const override { return m_sorcerer; }
    void setSorcererWanted(bool v) override;
    bool monkWanted() const override { return m_monk; }
    void setMonkWanted(bool v) override;
    QString nameFilter() const override;
    void setNameFilter(const QString& v) override;
    bool showBosses() const override { return m_showBosses; }
    void setShowBosses(bool v) override;
    bool showHunts() const override { return m_showHunts; }
    void setShowHunts(bool v) override;
    bool showQuests() const override { return m_showQuests; }
    void setShowQuests(bool v) override;
    bool showOther() const override { return m_showOther; }
    void setShowOther(bool v) override;
    uint jitterTime() const override { return m_jitterTime; }
    void setJitterTime(uint v) override;
    QObject* startTimeManager() const override;

    /// No filters: any level, size, vocation, name, activity and start time.
    void resetFilters();
    /// The first time the page opens: filter for the own level and vocation.
    void setCharacterFilters(uint32_t Level, uint8_t Vocation);
    bool characterFiltersSet() const { return m_characterFiltersSet; }
    /// New team listing from the storage.
    void setTeams(const std::vector<teamfinder::TTeamData>& Teams);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    /// Re-applies the filter.
    void anouceFilterChanged();
    /// Re-applies the filter and tells the page.
    void anouceFilterChangedAndEmitfilterchanged();
    void setFlag(bool& Flag, bool Value);
    void setUIntFilter(uint32_t& Field, const QString& Text);

    TTeamsListModel* m_model = nullptr;
    uint32_t m_filterLevel = 0;
    uint32_t m_minSize = 0;
    uint32_t m_maxSize = 0;
    bool m_druid = false;
    bool m_knight = false;
    bool m_paladin = false;
    bool m_sorcerer = false;
    bool m_monk = false;
    QString m_name;
    bool m_showBosses = true;
    bool m_showHunts = true;
    bool m_showQuests = true;
    bool m_showOther = true;
    uint32_t m_jitterTime = 0;
    teamfinder::TTeamStartTimeManager* m_time = nullptr;
    bool m_characterFiltersSet = false;
};

/// CIP: TTeamMembersSortProxyModel (Assemble Team `teamMembers`).
class TTeamMembersSortProxyModel final : public cip::TTeamMembersSortProxyModel
{
    Q_OBJECT
public:
    explicit TTeamMembersSortProxyModel(QObject* parent = nullptr);

    TTeamMembersModel* model() const { return m_model; }

    uint countAccepted() const override { return m_countAccepted; }
    uint countInvited() const override { return m_countInvited; }
    uint countRejected() const override { return m_countRejected; }
    uint countPending() const override { return m_countPending; }
    uint countDruid() const override { return m_countDruid; }
    uint countKnight() const override { return m_countKnight; }
    uint countPaladin() const override { return m_countPaladin; }
    uint countSorcerer() const override { return m_countSorcerer; }
    uint countMonk() const override { return m_countMonk; }

    /// New member list: recounts members per status, and accepted members per vocation.
    void setMembers(const std::vector<teamfinder::TTeamMember>& Members);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TTeamMembersModel* m_model = nullptr;
};

/// CIP: TActivitiesSortProxyModel (Assemble Team `activityList`).
class TActivitiesSortProxyModel final : public cip::TActivitiesSortProxyModel
{
    Q_OBJECT
public:
    explicit TActivitiesSortProxyModel(QObject* parent = nullptr);

    TActivitiesModel* model() const { return m_model; }
    /// New activity catalog.
    void setActivities(const std::vector<TActivity>& Activities);

    QString nameFilter() const override;
    void setNameFilter(const QString& v) override;
    bool showBosses() const override { return m_showBosses; }
    void setShowBosses(bool v) override;
    bool showHunts() const override { return m_showHunts; }
    void setShowHunts(bool v) override;
    bool showQuests() const override { return m_showQuests; }
    void setShowQuests(bool v) override;
    bool showOther() const override { return m_showOther; }
    void setShowOther(bool v) override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void setFlag(bool& Flag, bool Value);
    void filterUpdated();

    TActivitiesModel* m_model = nullptr;
    QString m_name;
    bool m_showBosses = true;
    bool m_showHunts = true;
    bool m_showQuests = true;
    bool m_showOther = true;
};

}
