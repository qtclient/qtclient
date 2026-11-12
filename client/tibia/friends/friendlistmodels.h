#pragma once

#include <cip_gen.h>

#include <QColor>
#include <QString>
#include <QVector>

namespace tibia {

/// FriendsFriendListPage.qml rows (CIP: TFriendListModel — count / rowCountChanged).
class TFriendListModel final : public cip::TFriendListModel
{
    Q_OBJECT
public:
    enum Role {
        AccountIdRole = Qt::UserRole + 1,
        MaincharNameRole,
        TitleRole,
        LevelRole,
        VocationRole,
        WorldRole,
        FriendGroupRole,
        OutfitIdRole,
        OutfitHeadColorRole,
        OutfitTorsoColorRole,
        OutfitLegsColorRole,
        OutfitDetailColorRole,
        OutfitFirstAddOnRole,
        OutfitSecondAddOnRole,
    };

    struct Row {
        int accountId = 0;
        QString maincharName;
        QString title;
        int level = 0;
        QString vocation;
        QString world;
        int friendGroup = 0;
        int outfitId = 0;
        QColor outfitHeadColor = Qt::black;
        QColor outfitTorsoColor = Qt::black;
        QColor outfitLegsColor = Qt::black;
        QColor outfitDetailColor = Qt::black;
        bool outfitFirstAddOn = false;
        bool outfitSecondAddOn = false;
    };

    explicit TFriendListModel(QObject* parent = nullptr);

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<Row> rows);
    const QVector<Row>& rows() const { return m_rows; }

private:
    QVector<Row> m_rows;
};

/// FriendsInvitesPage.qml received / sent invite tables (CIP: TInviteListModel).
class TInviteListModel final : public cip::TInviteListModel
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

    struct Row {
        uint accountId = 0;
        QString maincharName;
        QString title;
        QString inviterName;
        QString inviteDate;
    };

    explicit TInviteListModel(QObject* parent = nullptr);

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<Row> rows);
    const QVector<Row>& rows() const { return m_rows; }

private:
    QVector<Row> m_rows;
};

/// FriendsInvitesPage.qml blacklist table (CIP: TBlacklistModel).
class TBlacklistModel final : public cip::TBlacklistModel
{
    Q_OBJECT
public:
    enum Role {
        AccountIdRole = Qt::UserRole + 1,
        MaincharNameRole,
        BlacklistDateRole,
    };

    struct Row {
        uint accountId = 0;
        QString maincharName;
        QString blacklistDate;
    };

    explicit TBlacklistModel(QObject* parent = nullptr);

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QVector<Row> rows);
    const QVector<Row>& rows() const { return m_rows; }

private:
    QVector<Row> m_rows;
};

}
