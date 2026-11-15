#include "friendsinvitespagecontroller.h"

#include "friends/friendsciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "protocol/clientmessages.h"

#include <QDateTime>
#include <QLocale>

namespace tibia::gamewindow {

namespace {

// CIP FriendSystemAction numbers.
constexpr uint8_t kLoadInvites = 1;
constexpr uint8_t kLoadBlacklist = 2;
constexpr uint8_t kAcceptInvite = 4;
constexpr uint8_t kDeclineInvite = 5;
constexpr uint8_t kCancelInvite = 6;
constexpr uint8_t kAddToBlacklist = 7;
constexpr uint8_t kRemoveFromBlacklist = 8;

QString dateText(const uint32_t timestamp)
{
    if (timestamp == 0)
        return {};
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(timestamp).date(), QLocale::ShortFormat);
}

}

TFriendsInvitesPageController::TFriendsInvitesPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsInvitesPageController(parent)
    , m_receivedModel(new TInviteListModel(this))
    , m_sentModel(new TInviteListModel(this))
    , m_blacklistModel(new TBlacklistModel(this))
{
    m_receivedInvites = m_receivedModel;
    m_sentInvites = m_sentModel;
    m_blacklist = m_blacklistModel;
    auto* invites = friends::TFriendsStorage::instance()->inviteStorage();
    connect(invites, &friends::TFriendsInviteStorage::inviteListChanged, this, &TFriendsInvitesPageController::rebuildInvites);
    connect(invites, &friends::TFriendsInviteStorage::blacklistChanged, this, &TFriendsInvitesPageController::rebuildBlacklist);
    rebuildInvites();
    rebuildBlacklist();
}

void TFriendsInvitesPageController::rebuildInvites()
{
    QVector<TInviteListModel::Row> received;
    QVector<TInviteListModel::Row> sent;
    for (const auto& invite : friends::TFriendsStorage::instance()->inviteStorage()->invites()) {
        TInviteListModel::Row row;
        row.accountId = invite.account().account_id();
        row.maincharName = QString::fromStdString(invite.account().name());
        row.title = QString::fromStdString(invite.account().title());
        row.inviterName = QString::fromStdString(invite.text());
        row.inviteDate = dateText(invite.timestamp());
        (invite.sent() ? sent : received).push_back(std::move(row));
    }
    m_receivedModel->setRows(std::move(received));
    m_sentModel->setRows(std::move(sent));
}

void TFriendsInvitesPageController::rebuildBlacklist()
{
    QVector<TBlacklistModel::Row> rows;
    for (const auto& entry : friends::TFriendsStorage::instance()->inviteStorage()->blacklist()) {
        TBlacklistModel::Row row;
        row.accountId = entry.account().account_id();
        row.maincharName = QString::fromStdString(entry.account().name());
        row.blacklistDate = dateText(entry.timestamp());
        rows.push_back(std::move(row));
    }
    m_blacklistModel->setRows(std::move(rows));
}

QObject* TFriendsInvitesPageController::receivedInvites() const
{
    return m_receivedInvites;
}

QObject* TFriendsInvitesPageController::sentInvites() const
{
    return m_sentInvites;
}

QObject* TFriendsInvitesPageController::blacklist() const
{
    return m_blacklist;
}

TFriendsInvitesPageController::EShownListType TFriendsInvitesPageController::shownListType() const
{
    return m_shownListType;
}

void TFriendsInvitesPageController::setShownListType(const EShownListType type)
{
    cip::tibia::gamewindow::TFriendsInvitesPageController::setShownListType(type);
}

void TFriendsInvitesPageController::showDialogTab()
{
    // CIP fetches both lists when the tab opens.
    protocol::gameclient::sendFriendSystemAction(kLoadInvites);
    protocol::gameclient::sendFriendSystemAction(kLoadBlacklist);
}

void TFriendsInvitesPageController::hideDialogTab()
{
}

void TFriendsInvitesPageController::showContextMenuForSentInvite(const uint InvitedAccountID)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("friends_cancel_invite")), &TContextMenuEntryItem::triggered, this, [InvitedAccountID] {
        protocol::gameclient::sendFriendSystemAccountAction(kCancelInvite, InvitedAccountID);
    });
    menu->showCustomMenu();
}

void TFriendsInvitesPageController::showContextMenuForReceivedInvite(const uint InvitedAccountID)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    const auto accept = [this, menu, InvitedAccountID](const char* label, const uint8_t group) {
        connect(menu->addCustomEntry(qtTrId(label)), &TContextMenuEntryItem::triggered, this, [InvitedAccountID, group] {
            protocol::gameclient::sendFriendSystemAccountAction(kAcceptInvite, InvitedAccountID, group);
        });
    };
    accept("friends_accept_invite_contact", 1);
    accept("friends_accept_invite_friend", 2);
    accept("friends_accept_invite_closefriend", 3);
    menu->addCustomSeparator();
    connect(menu->addCustomEntry(qtTrId("friends_decline_invite")), &TContextMenuEntryItem::triggered, this, [InvitedAccountID] {
        protocol::gameclient::sendFriendSystemAccountAction(kDeclineInvite, InvitedAccountID);
    });
    // Blacklisting the inviter declines the invite first.
    connect(menu->addCustomEntry(qtTrId("friends_blacklist_inviter")), &TContextMenuEntryItem::triggered, this, [InvitedAccountID] {
        protocol::gameclient::sendFriendSystemAccountAction(kDeclineInvite, InvitedAccountID);
        protocol::gameclient::sendFriendSystemAccountAction(kAddToBlacklist, InvitedAccountID);
    });
    menu->showCustomMenu();
}

void TFriendsInvitesPageController::showContextMenuForBlacklistedAccount(const uint BlacklistedAccountID)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("friends_remove_from_blacklist")), &TContextMenuEntryItem::triggered, this, [BlacklistedAccountID] {
        protocol::gameclient::sendFriendSystemAccountAction(kRemoveFromBlacklist, BlacklistedAccountID);
    });
    menu->showCustomMenu();
}

}
