#include "friends/friendlistmodels.h"

namespace tibia {

TFriendListModel::TFriendListModel(QObject* parent)
    : cip::TFriendListModel(parent)
{}

int TFriendListModel::count() const
{
    return m_count;
}

int TFriendListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TFriendListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case AccountIdRole:
        return row.accountId;
    case MaincharNameRole:
        return row.maincharName;
    case TitleRole:
        return row.title;
    case LevelRole:
        return row.level;
    case VocationRole:
        return row.vocation;
    case WorldRole:
        return row.world;
    case FriendGroupRole:
        return row.friendGroup;
    case OutfitIdRole:
        return row.outfitId;
    case OutfitHeadColorRole:
        return row.outfitHeadColor;
    case OutfitTorsoColorRole:
        return row.outfitTorsoColor;
    case OutfitLegsColorRole:
        return row.outfitLegsColor;
    case OutfitDetailColorRole:
        return row.outfitDetailColor;
    case OutfitFirstAddOnRole:
        return row.outfitFirstAddOn;
    case OutfitSecondAddOnRole:
        return row.outfitSecondAddOn;
    default:
        return {};
    }
}

QHash<int, QByteArray> TFriendListModel::roleNames() const
{
    return {
        { AccountIdRole, "accountID" },
        { MaincharNameRole, "maincharName" },
        { TitleRole, "title" },
        { LevelRole, "level" },
        { VocationRole, "vocation" },
        { WorldRole, "world" },
        { FriendGroupRole, "friendGroup" },
        { OutfitIdRole, "outfitID" },
        { OutfitHeadColorRole, "outfitHeadColor" },
        { OutfitTorsoColorRole, "outfitTorsoColor" },
        { OutfitLegsColorRole, "outfitLegsColor" },
        { OutfitDetailColorRole, "outfitDetailColor" },
        { OutfitFirstAddOnRole, "outfitFirstAddOn" },
        { OutfitSecondAddOnRole, "outfitSecondAddOn" },
    };
}

void TFriendListModel::setRows(QVector<Row> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    m_count = m_rows.size();
    endResetModel();
    Q_EMIT rowCountChanged();
}

TInviteListModel::TInviteListModel(QObject* parent)
    : cip::TInviteListModel(parent)
{}

int TInviteListModel::count() const
{
    return m_count;
}

int TInviteListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TInviteListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case AccountIdRole:
        return row.accountId;
    case MaincharNameRole:
        return row.maincharName;
    case TitleRole:
        return row.title;
    case InviterNameRole:
        return row.inviterName;
    case InviteDateRole:
        return row.inviteDate;
    default:
        return {};
    }
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

void TInviteListModel::setRows(QVector<Row> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    m_count = m_rows.size();
    endResetModel();
    Q_EMIT rowCountChanged();
}

TBlacklistModel::TBlacklistModel(QObject* parent)
    : cip::TBlacklistModel(parent)
{}

int TBlacklistModel::count() const
{
    return m_count;
}

int TBlacklistModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TBlacklistModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case AccountIdRole:
        return row.accountId;
    case MaincharNameRole:
        return row.maincharName;
    case BlacklistDateRole:
        return row.blacklistDate;
    default:
        return {};
    }
}

QHash<int, QByteArray> TBlacklistModel::roleNames() const
{
    return {
        { AccountIdRole, "accountID" },
        { MaincharNameRole, "maincharName" },
        { BlacklistDateRole, "blacklistDate" },
    };
}

void TBlacklistModel::setRows(QVector<Row> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    m_count = m_rows.size();
    endResetModel();
    Q_EMIT rowCountChanged();
}

}
