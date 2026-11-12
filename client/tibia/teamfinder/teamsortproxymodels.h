#pragma once

#include "teamfinder/teamfinderciptypes.h"

#include <cip_gen.h>

#include <QObject>
#include <QSortFilterProxyModel>

namespace tibia {

/// CIP: TTeamsListSortProxyModel (Join Team `teamsList`).
class TTeamsListSortProxyModel final : public cip::TTeamsListSortProxyModel
{
    Q_OBJECT
public:
    explicit TTeamsListSortProxyModel(QAbstractItemModel* source, QObject* parent = nullptr);

    int numberOfTeams() const override;
    QString filterLevel() const override;
    void setFilterLevel(const QString& v) override;
    QString minTeamSize() const override;
    void setMinTeamSize(const QString& v) override;
    QString maxTeamSize() const override;
    void setMaxTeamSize(const QString& v) override;
    bool druidWanted() const override;
    void setDruidWanted(bool v) override;
    bool knightWanted() const override;
    void setKnightWanted(bool v) override;
    bool paladinWanted() const override;
    void setPaladinWanted(bool v) override;
    bool sorcererWanted() const override;
    void setSorcererWanted(bool v) override;
    bool monkWanted() const override;
    void setMonkWanted(bool v) override;
    QString nameFilter() const override;
    void setNameFilter(const QString& v) override;
    bool showBosses() const override;
    void setShowBosses(bool v) override;
    bool showHunts() const override;
    void setShowHunts(bool v) override;
    bool showQuests() const override;
    void setShowQuests(bool v) override;
    bool showOther() const override;
    void setShowOther(bool v) override;
    uint jitterTime() const override;
    void setJitterTime(uint v) override;
    QObject* startTimeManager() const override;

    /// CIP Join tab open: restore vocation / activity type defaults.
    void resetFilters();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    /// CIP private slots (typos intentional — match moc).
    void anouceFilterChanged();
    void anouceFilterChangedAndEmitfilterchanged();
    QString getUIntFilterAsString(uint UIntMember) const;
    void setStringAsUIntFilter(QString NumberText, uint& UIntMember);

    void refreshNumberOfTeams();
    bool vocationWanted(int bits) const;
    bool typeShown(int teamType) const;

    teamfinder::TTeamStartTimeManager* m_time = nullptr;
};

/// CIP: TTeamMembersSortProxyModel (Assemble Team `teamMembers`).
class TTeamMembersSortProxyModel final : public cip::TTeamMembersSortProxyModel
{
    Q_OBJECT
public:
    explicit TTeamMembersSortProxyModel(QAbstractItemModel* source, QObject* parent = nullptr);

    uint countAccepted() const override;
    uint countInvited() const override;
    uint countRejected() const override;
    uint countPending() const override;
    uint countDruid() const override;
    uint countKnight() const override;
    uint countPaladin() const override;
    uint countSorcerer() const override;
    uint countMonk() const override;

    void refreshCounts();

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void onSourceChanged();
};

/// CIP: TActivitiesSortProxyModel (Assemble Team `activityList`).
class TActivitiesSortProxyModel final : public cip::TActivitiesSortProxyModel
{
    Q_OBJECT
public:
    explicit TActivitiesSortProxyModel(QAbstractItemModel* source, QObject* parent = nullptr);

    QString nameFilter() const override;
    void setNameFilter(const QString& v) override;
    bool showBosses() const override;
    void setShowBosses(bool v) override;
    bool showHunts() const override;
    void setShowHunts(bool v) override;
    bool showQuests() const override;
    void setShowQuests(bool v) override;
    bool showOther() const override;
    void setShowOther(bool v) override;

    void resetFilters();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void onFilterAnnounced();
    bool typeShown(int teamType) const;
};

}
