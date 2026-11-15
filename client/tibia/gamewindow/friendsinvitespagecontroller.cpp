#include "friendsinvitespagecontroller.h"

#include "friends/friendsciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "protocol/clientmessages.h"

namespace tibia::gamewindow {

namespace {

// FriendSystemAction numbers.
constexpr uint8_t kLoadInvites = 1;
constexpr uint8_t kLoadBlacklist = 2;
constexpr uint8_t kAcceptInvite = 4;
constexpr uint8_t kDeclineInvite = 5;
constexpr uint8_t kCancelInvite = 6;
constexpr uint8_t kAddToBlacklist = 7;
constexpr uint8_t kRemoveFromBlacklist = 8;

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
}

void TFriendsInvitesPageController::updateInviteLists()
{
    const friends::TInviteList& inviteList = friends::TFriendsStorage::instance()->inviteStorage()->inviteList();
    m_receivedModel->setEntries(inviteList.received);
    m_sentModel->setEntries(inviteList.sent);
}

void TFriendsInvitesPageController::updateBlacklist()
{
    m_blacklistModel->setEntries(friends::TFriendsStorage::instance()->inviteStorage()->blacklist());
}

void TFriendsInvitesPageController::disconnectStorage()
{
    disconnect(m_inviteListConnection);
    disconnect(m_blacklistConnection);
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
    // The tab opens on the received invites, follows the stored lists while it
    // is open and asks for fresh ones.
    disconnectStorage();
    setShownListType(ReceivedInvites);
    auto* invites = friends::TFriendsStorage::instance()->inviteStorage();
    m_inviteListConnection = connect(invites, &friends::TFriendsInviteStorage::inviteListChanged, this,
                                     &TFriendsInvitesPageController::updateInviteLists);
    m_blacklistConnection = connect(invites, &friends::TFriendsInviteStorage::blacklistChanged, this,
                                    &TFriendsInvitesPageController::updateBlacklist);
    updateInviteLists();
    updateBlacklist();
    protocol::gameclient::sendFriendSystemAction(kLoadInvites);
    protocol::gameclient::sendFriendSystemAction(kLoadBlacklist);
}

void TFriendsInvitesPageController::hideDialogTab()
{
    disconnectStorage();
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
