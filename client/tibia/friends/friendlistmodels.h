#pragma once

#include "friends/friendsciptypes.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include <vector>

namespace tibia {

/// CIP shared::TSimpleVectorAbstractListModel<Entry>: a list model over a
/// vector of entries; the derived model maps an entry to its roles.
template<typename Base, typename Entry>
class TSimpleVectorAbstractListModel : public Base
{
public:
    using Base::Base;

    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
    }

    QVariant data(const QModelIndex& index, const int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
            return {};
        return entryData(m_entries[static_cast<size_t>(index.row())], role);
    }

    const std::vector<Entry>& entries() const { return m_entries; }

    /// Takes the new entries: rows past the old count are inserted, rows past
    /// the new count removed, and the rows both have are reported as changed.
    void setEntries(const std::vector<Entry>& entries)
    {
        int count = static_cast<int>(m_entries.size());
        const int newCount = static_cast<int>(entries.size());
        if (newCount < count) {
            this->beginRemoveRows({}, newCount, count - 1);
            if (&m_entries != &entries)
                m_entries = entries;
            this->endRemoveRows();
            count = newCount;
        } else if (count < newCount) {
            this->beginInsertRows({}, count, newCount - 1);
            if (&m_entries != &entries)
                m_entries = entries;
            this->endInsertRows();
        } else if (&m_entries != &entries) {
            m_entries = entries;
        }
        if (count > 0)
            Q_EMIT this->dataChanged(this->index(0, 0), this->index(count - 1, 0));
    }

    /// Empties the model.
    void clearEntries() { setEntries({}); }

protected:
    virtual QVariant entryData(const Entry& entry, int role) const = 0;

    std::vector<Entry> m_entries;
};

/// CIP (anonymous)::TFriendListModel: FriendsFriendListPage.qml rows.
class TFriendListModel final : public TSimpleVectorAbstractListModel<cip::TFriendListModel, friends::TFriendList::TFriendEntry>
{
    Q_OBJECT
public:
    enum Role {
        AccountIdRole = Qt::UserRole + 1,
        MaincharNameRole,
        TitleRole,
        FriendGroupRole,
        WorldRole,
        VocationRole,
        LevelRole,
        OutfitIdRole,
        OutfitHeadColorRole,
        OutfitTorsoColorRole,
        OutfitLegsColorRole,
        OutfitDetailColorRole,
        OutfitFirstAddOnRole,
        OutfitSecondAddOnRole,
    };

    explicit TFriendListModel(QObject* parent = nullptr);

    int count() const override;
    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant entryData(const friends::TFriendList::TFriendEntry& entry, int role) const override;
};

/// CIP (anonymous)::TFriendListSortProxyModel: the friend list by name, filtered
/// by friend group (0 everyone, 1 close friends, 2 friends, 3 contacts).
class TFriendListSortProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TFriendListSortProxyModel(QObject* parent = nullptr);

    TFriendListModel* friendListModel() { return &m_model; }
    int showFilter() const { return m_showFilter; }
    /// The new filter; false when it is the current one.
    bool setShowFilter(int showFilter);
    /// Takes the friend list and filters it again.
    void setFriendList(const std::vector<friends::TFriendList::TFriendEntry>& friendList);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TFriendListModel m_model;
    int m_showFilter = 0;
};

/// CIP (anonymous)::TInviteListModel: FriendsInvitesPage.qml received / sent invites.
class TInviteListModel final : public TSimpleVectorAbstractListModel<cip::TInviteListModel, friends::TInviteList::TInviteListEntry>
{
    Q_OBJECT
public:
    enum Role {
        AccountIdRole = Qt::UserRole + 1,
        MaincharNameRole,
        TitleRole,
        InviterNameRole,
        InviteDateRole,
    };

    explicit TInviteListModel(QObject* parent = nullptr);

    int count() const override;
    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant entryData(const friends::TInviteList::TInviteListEntry& entry, int role) const override;
};

/// CIP (anonymous)::TBlacklistModel: FriendsInvitesPage.qml blacklist.
class TBlacklistModel final : public TSimpleVectorAbstractListModel<cip::TBlacklistModel, friends::TBlacklist::TBlacklistEntry>
{
    Q_OBJECT
public:
    enum Role {
        AccountIdRole = Qt::UserRole + 1,
        MaincharNameRole,
        BlacklistDateRole,
    };

    explicit TBlacklistModel(QObject* parent = nullptr);

    int count() const override;
    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant entryData(const friends::TBlacklist::TBlacklistEntry& entry, int role) const override;
};

/// CIP (anonymous)::TAccountBadgesModel: FriendsBadgesPage.qml rows.
class TFriendsAccountBadgesModel final : public TSimpleVectorAbstractListModel<QAbstractListModel, friends::TAccountBadge>
{
    Q_OBJECT
public:
    enum Role {
        BadgeIdRole = Qt::UserRole + 1,
        BadgeNameRole,
        BadgeDescriptionRole,
        BadgeIconRole,
        IsBadgeVisibleRole,
        IsBadgeUnlockedRole,
    };

    using TSimpleVectorAbstractListModel::TSimpleVectorAbstractListModel;

    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant entryData(const friends::TAccountBadge& entry, int role) const override;
};

/// CIP (anonymous)::TAccountBadgesSortProxyModel: the badges filter
/// (0 all, 1 locked, 2 unlocked, 3 public, 4 hidden).
class TAccountBadgesSortProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TAccountBadgesSortProxyModel(QObject* parent = nullptr);

    int badgeFilter() const { return m_badgeFilter; }
    /// The new filter; false when it is the current one.
    bool setBadgeFilter(int badgeFilter);
    /// Takes the badges and filters them again.
    void setBadges(const std::vector<friends::TAccountBadge>& badges);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TFriendsAccountBadgesModel m_model;
    int m_badgeFilter = 0;
};

/// CIP (anonymous)::TVisibilitySettingsModel: FriendsConfigurationPage.qml rows.
/// A group shows what it or "everyone" (group 0) may see; a group's switch is
/// enabled while "everyone" does not already allow it.
class TVisibilitySettingsModel final : public TSimpleVectorAbstractListModel<QAbstractListModel, friends::TVisibilitySetting>
{
    Q_OBJECT
public:
    enum Role {
        SettingTypeRole = Qt::UserRole + 1,
        ShowCharacterInfoRole,
        ShowAccountInfoRole,
        AllowInspectRole,
        EnableShowCharacterInfoRole,
        EnableShowAccountInfoRole,
        EnableAllowInspectRole,
    };

    using TSimpleVectorAbstractListModel::TSimpleVectorAbstractListModel;

    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant entryData(const friends::TVisibilitySetting& entry, int role) const override;

private:
    /// The "everyone" entry, or an empty one.
    friends::TVisibilitySetting everyone() const;
};

/// CIP gamewindow::TAccountCharactersModel: the characters of a searched account.
class TAccountCharactersModel final : public TSimpleVectorAbstractListModel<cip::tibia::gamewindow::TAccountCharactersModel,
                                                                           friends::TAccountCharacterListEntry>
{
    Q_OBJECT
public:
    enum Role {
        CharacterIdRole = Qt::UserRole + 1,
        CharacterNameRole,
        IsMainCharacterRole,
        CharacterTitleRole,
        WorldRole,
        VocationRole,
        LevelRole,
        OutfitIdRole,
        HeadColorRole,
        TorsoColorRole,
        LegsColorRole,
        DetailColorRole,
        Addon1Role,
        Addon2Role,
        IsOnlineRole,
    };

    using TSimpleVectorAbstractListModel::TSimpleVectorAbstractListModel;

    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant entryData(const friends::TAccountCharacterListEntry& entry, int role) const override;
};

}
