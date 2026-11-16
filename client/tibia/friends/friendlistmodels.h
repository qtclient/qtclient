#pragma once

#include "friends/friendsciptypes.h"
#include "shared/simplevectorabstractlistmodel.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include <vector>

namespace tibia {

/// CIP (anonymous)::TFriendListModel: FriendsFriendListPage.qml rows.
class TFriendListModel final : public shared::TSimpleVectorAbstractListModel<friends::TFriendList::TFriendEntry, cip::TFriendListModel>
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
    QVariant itemData(const friends::TFriendList::TFriendEntry& entry, int role) const override;
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
class TInviteListModel final : public shared::TSimpleVectorAbstractListModel<friends::TInviteList::TInviteListEntry, cip::TInviteListModel>
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
    QVariant itemData(const friends::TInviteList::TInviteListEntry& entry, int role) const override;
};

/// CIP (anonymous)::TBlacklistModel: FriendsInvitesPage.qml blacklist.
class TBlacklistModel final : public shared::TSimpleVectorAbstractListModel<friends::TBlacklist::TBlacklistEntry, cip::TBlacklistModel>
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
    QVariant itemData(const friends::TBlacklist::TBlacklistEntry& entry, int role) const override;
};

/// CIP (anonymous)::TAccountBadgesModel: FriendsBadgesPage.qml rows.
class TFriendsAccountBadgesModel final : public shared::TSimpleVectorAbstractListModel<friends::TAccountBadge, QAbstractListModel>
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
    QVariant itemData(const friends::TAccountBadge& entry, int role) const override;
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
class TVisibilitySettingsModel final : public shared::TSimpleVectorAbstractListModel<friends::TVisibilitySetting, QAbstractListModel>
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
    QVariant itemData(const friends::TVisibilitySetting& entry, int role) const override;

private:
    /// The "everyone" entry, or an empty one.
    friends::TVisibilitySetting everyone() const;
};

/// CIP gamewindow::TAccountCharactersModel: the characters of a searched account.
class TAccountCharactersModel final : public shared::TSimpleVectorAbstractListModel<friends::TAccountCharacterListEntry, cip::tibia::gamewindow::TAccountCharactersModel>
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
    QVariant itemData(const friends::TAccountCharacterListEntry& entry, int role) const override;
};

}
