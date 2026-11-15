#include "friends/friendlistmodels.h"

#include "client/outfit.h"

#include <QColor>
#include <QDateTime>

namespace tibia {

namespace {

QColor outfitColor(const uint8_t index)
{
    const Color color = Outfit::getColor(index);
    return QColor(color.r(), color.g(), color.b());
}

/// "yyyy-MM-dd, HH:mm:ss" in local time.
QString formatDateTime(const uint32_t timestamp)
{
    return QDateTime::fromSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
}

/// The row count follows the inserted and removed rows.
template<typename Model>
void connectRowCount(Model* model)
{
    QObject::connect(model, &QAbstractItemModel::rowsInserted, model, &Model::rowCountChanged);
    QObject::connect(model, &QAbstractItemModel::rowsRemoved, model, &Model::rowCountChanged);
}

}

TFriendListModel::TFriendListModel(QObject* parent)
    : TSimpleVectorAbstractListModel(parent)
{
    connectRowCount(this);
}

int TFriendListModel::count() const
{
    return rowCount();
}

QHash<int, QByteArray> TFriendListModel::roleNames() const
{
    return {
        { AccountIdRole, "accountID" },
        { MaincharNameRole, "maincharName" },
        { FriendGroupRole, "friendGroup" },
        { WorldRole, "world" },
        { VocationRole, "vocation" },
        { LevelRole, "level" },
        { TitleRole, "title" },
        { OutfitIdRole, "outfitID" },
        { OutfitHeadColorRole, "outfitHeadColor" },
        { OutfitTorsoColorRole, "outfitTorsoColor" },
        { OutfitLegsColorRole, "outfitLegsColor" },
        { OutfitDetailColorRole, "outfitDetailColor" },
        { OutfitFirstAddOnRole, "outfitFirstAddOn" },
        { OutfitSecondAddOnRole, "outfitSecondAddOn" },
    };
}

QVariant TFriendListModel::entryData(const friends::TFriendList::TFriendEntry& entry, const int role) const
{
    const friends::TAccountCharacterListEntry& character = entry.character;
    switch (role) {
    case AccountIdRole:
        return character.accountId;
    case MaincharNameRole:
        return character.name;
    case TitleRole:
        return character.title;
    case FriendGroupRole:
        return entry.friendGroup;
    case WorldRole:
        return character.world;
    case VocationRole:
        return character.vocation;
    case LevelRole:
        return static_cast<uint>(character.level);
    case OutfitIdRole:
        return static_cast<uint>(character.outfitId);
    case OutfitHeadColorRole:
        return outfitColor(character.headColor);
    case OutfitTorsoColorRole:
        return outfitColor(character.torsoColor);
    case OutfitLegsColorRole:
        return outfitColor(character.legsColor);
    case OutfitDetailColorRole:
        return outfitColor(character.detailColor);
    case OutfitFirstAddOnRole:
        return character.firstAddon;
    case OutfitSecondAddOnRole:
        return character.secondAddon;
    default:
        return {};
    }
}

TFriendListSortProxyModel::TFriendListSortProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(&m_model);
    sort(0, Qt::AscendingOrder);
}

bool TFriendListSortProxyModel::setShowFilter(const int showFilter)
{
    if (showFilter == m_showFilter)
        return false;
    beginFilterChange();
    m_showFilter = showFilter;
    endFilterChange(Direction::Both);
    return true;
}

void TFriendListSortProxyModel::setFriendList(const std::vector<friends::TFriendList::TFriendEntry>& friendList)
{
    beginFilterChange();
    m_model.setEntries(friendList);
    endFilterChange(Direction::Both);
    invalidate();
}

bool TFriendListSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = m_model.index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    if (m_showFilter == 0)
        return true;
    // Contacts (1) show under filter 3, friends (2) under 2, close friends (3) under 1.
    switch (m_model.data(index, TFriendListModel::FriendGroupRole).toInt()) {
    case 1:
        return m_showFilter == 3;
    case 2:
        return m_showFilter == 2;
    case 3:
        return m_showFilter == 1;
    default:
        return false;
    }
}

bool TFriendListSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftName = left.model() ? left.data(TFriendListModel::MaincharNameRole).toString().toLower() : QString();
    const QString rightName = right.model() ? right.data(TFriendListModel::MaincharNameRole).toString().toLower() : QString();
    return QString::compare(leftName, rightName, Qt::CaseSensitive) < 0;
}

TInviteListModel::TInviteListModel(QObject* parent)
    : TSimpleVectorAbstractListModel(parent)
{
    connectRowCount(this);
}

int TInviteListModel::count() const
{
    return rowCount();
}

QHash<int, QByteArray> TInviteListModel::roleNames() const
{
    return {
        { AccountIdRole, "accountID" },
        { MaincharNameRole, "maincharName" },
        { TitleRole, "title" },
        { InviterNameRole, "inviterName" },
        { InviteDateRole, "inviteDate" },
    };
}

QVariant TInviteListModel::entryData(const friends::TInviteList::TInviteListEntry& entry, const int role) const
{
    switch (role) {
    case AccountIdRole:
        return entry.accountId;
    case MaincharNameRole:
        return entry.name;
    case TitleRole:
        return entry.title;
    case InviterNameRole:
        return entry.inviterName;
    case InviteDateRole:
        return formatDateTime(entry.timestamp);
    default:
        return {};
    }
}

TBlacklistModel::TBlacklistModel(QObject* parent)
    : TSimpleVectorAbstractListModel(parent)
{
    connectRowCount(this);
}

int TBlacklistModel::count() const
{
    return rowCount();
}

QHash<int, QByteArray> TBlacklistModel::roleNames() const
{
    return {
        { AccountIdRole, "accountID" },
        { MaincharNameRole, "maincharName" },
        { BlacklistDateRole, "blacklistDate" },
    };
}

QVariant TBlacklistModel::entryData(const friends::TBlacklist::TBlacklistEntry& entry, const int role) const
{
    switch (role) {
    case AccountIdRole:
        return entry.accountId;
    case MaincharNameRole:
        return entry.name;
    case BlacklistDateRole:
        return formatDateTime(entry.timestamp);
    default:
        return {};
    }
}

QHash<int, QByteArray> TFriendsAccountBadgesModel::roleNames() const
{
    return {
        { BadgeIdRole, "badgeID" },
        { BadgeNameRole, "badgeName" },
        { BadgeDescriptionRole, "badgeDescription" },
        { BadgeIconRole, "badgeIcon" },
        { IsBadgeVisibleRole, "isBadgeVisible" },
        { IsBadgeUnlockedRole, "isBadgeUnlocked" },
    };
}

QVariant TFriendsAccountBadgesModel::entryData(const friends::TAccountBadge& entry, const int role) const
{
    switch (role) {
    case BadgeIdRole:
        return entry.id;
    case BadgeNameRole:
        return entry.name;
    case BadgeDescriptionRole:
        return entry.description;
    case BadgeIconRole:
        // The icon atlas counts from 0, badge ids from 1.
        return QStringLiteral("image://friendbadge-icons/%1").arg(static_cast<qint64>(entry.id) - 1);
    case IsBadgeVisibleRole:
        return entry.visible;
    case IsBadgeUnlockedRole:
        return entry.unlocked;
    default:
        return {};
    }
}

TAccountBadgesSortProxyModel::TAccountBadgesSortProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(&m_model);
}

bool TAccountBadgesSortProxyModel::setBadgeFilter(const int badgeFilter)
{
    if (badgeFilter == m_badgeFilter)
        return false;
    beginFilterChange();
    m_badgeFilter = badgeFilter;
    endFilterChange(Direction::Both);
    return true;
}

void TAccountBadgesSortProxyModel::setBadges(const std::vector<friends::TAccountBadge>& badges)
{
    beginFilterChange();
    m_model.setEntries(badges);
    endFilterChange(Direction::Both);
}

bool TAccountBadgesSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = m_model.index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    switch (m_badgeFilter) {
    case 0:
        return true;
    case 1:
        return !m_model.data(index, TFriendsAccountBadgesModel::IsBadgeUnlockedRole).toBool();
    case 2:
        return m_model.data(index, TFriendsAccountBadgesModel::IsBadgeUnlockedRole).toBool();
    case 3:
        return m_model.data(index, TFriendsAccountBadgesModel::IsBadgeVisibleRole).toBool();
    case 4:
        return !m_model.data(index, TFriendsAccountBadgesModel::IsBadgeVisibleRole).toBool();
    default:
        return false;
    }
}

QHash<int, QByteArray> TVisibilitySettingsModel::roleNames() const
{
    return {
        { SettingTypeRole, "settingType" },
        { ShowCharacterInfoRole, "showCharacterInfo" },
        { ShowAccountInfoRole, "showAccountInfo" },
        { AllowInspectRole, "allowInspect" },
        { EnableShowCharacterInfoRole, "enableShowCharacterInfo" },
        { EnableShowAccountInfoRole, "enableShowAccountInfo" },
        { EnableAllowInspectRole, "enableAllowInspect" },
    };
}

friends::TVisibilitySetting TVisibilitySettingsModel::everyone() const
{
    for (const auto& setting : m_entries) {
        if (setting.friendGroup == 0)
            return setting;
    }
    return {};
}

QVariant TVisibilitySettingsModel::entryData(const friends::TVisibilitySetting& entry, const int role) const
{
    switch (role) {
    case SettingTypeRole:
        return entry.friendGroup;
    case ShowCharacterInfoRole:
        return entry.showCharacterInfo || everyone().showCharacterInfo;
    case ShowAccountInfoRole:
        return entry.showAccountInfo || everyone().showAccountInfo;
    case AllowInspectRole:
        return entry.allowInspect || everyone().allowInspect;
    case EnableShowCharacterInfoRole:
        return entry.friendGroup == 0 || !everyone().showCharacterInfo;
    case EnableShowAccountInfoRole:
        return entry.friendGroup == 0 || !everyone().showAccountInfo;
    case EnableAllowInspectRole:
        return entry.friendGroup == 0 || !everyone().allowInspect;
    default:
        return {};
    }
}

QHash<int, QByteArray> TAccountCharactersModel::roleNames() const
{
    return {
        { CharacterIdRole, "characterID" },
        { CharacterNameRole, "characterName" },
        { IsMainCharacterRole, "isMainCharacter" },
        { CharacterTitleRole, "characterTitle" },
        { WorldRole, "world" },
        { VocationRole, "vocation" },
        { LevelRole, "level" },
        { OutfitIdRole, "outfitID" },
        { HeadColorRole, "headColor" },
        { TorsoColorRole, "torsoColor" },
        { LegsColorRole, "legsColor" },
        { DetailColorRole, "detailColor" },
        { Addon1Role, "addon1" },
        { Addon2Role, "addon2" },
        { IsOnlineRole, "isOnline" },
    };
}

QVariant TAccountCharactersModel::entryData(const friends::TAccountCharacterListEntry& entry, const int role) const
{
    switch (role) {
    case CharacterIdRole:
        return entry.characterId;
    case CharacterNameRole:
        return entry.name;
    case IsMainCharacterRole:
        return entry.isMainCharacter;
    case CharacterTitleRole:
        return entry.title;
    case WorldRole:
        return entry.world;
    case VocationRole:
        return entry.vocation;
    case LevelRole:
        return static_cast<uint>(entry.level);
    case OutfitIdRole:
        return static_cast<uint>(entry.outfitId);
    case HeadColorRole:
        return outfitColor(entry.headColor);
    case TorsoColorRole:
        return outfitColor(entry.torsoColor);
    case LegsColorRole:
        return outfitColor(entry.legsColor);
    case DetailColorRole:
        return outfitColor(entry.detailColor);
    case Addon1Role:
        return entry.firstAddon;
    case Addon2Role:
        return entry.secondAddon;
    case IsOnlineRole:
        return entry.isOnline;
    default:
        return {};
    }
}

}
