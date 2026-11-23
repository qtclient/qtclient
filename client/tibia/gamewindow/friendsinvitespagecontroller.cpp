#include "friendsinvitespagecontroller.h"
#include "game/gamesessionbase.h"

#include "friends/friendsciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "protocol/clientmessages.h"

namespace tibia::gamewindow {

namespace {

// FriendSystemAction numbers.
constexpr uint8_t kLoadInvites = 1;
constexpr uint8_t kLoadBlacklist = 2;
constexpr uint8_t kAddToBlacklist = 7;

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
    friends::TFriendsStorage* storage = friendsStorage();
    if (!storage)
        return;
    const friends::TInviteList& inviteList = storage->inviteStorage()->inviteList();
    m_receivedModel->setItems(inviteList.received);
    m_sentModel->setItems(inviteList.sent);
}

void TFriendsInvitesPageController::updateBlacklist()
{
    if (friends::TFriendsStorage* storage = friendsStorage())
        m_blacklistModel->setItems(storage->inviteStorage()->blacklist());
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
    friends::TFriendsStorage* storage = friendsStorage();
    if (!storage)
        return;
    auto* invites = storage->inviteStorage();
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
    connect(menu->addCustomEntry(qtTrId("friends_cancel_invite")), &TContextMenuEntryItem::triggered, this, [this, InvitedAccountID] {
        if (auto* handler = friendsProtocolMessageHandler())
            handler->cancelInvite(InvitedAccountID);
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
        connect(menu->addCustomEntry(qtTrId(label)), &TContextMenuEntryItem::triggered, this, [this, InvitedAccountID, group] {
            if (auto* handler = friendsProtocolMessageHandler())
                handler->acceptInvite(InvitedAccountID, group);
        });
    };
    accept("friends_accept_invite_contact", 1);
    accept("friends_accept_invite_friend", 2);
    accept("friends_accept_invite_closefriend", 3);
    menu->addCustomSeparator();
    connect(menu->addCustomEntry(qtTrId("friends_decline_invite")), &TContextMenuEntryItem::triggered, this, [this, InvitedAccountID] {
        if (auto* handler = friendsProtocolMessageHandler())
            handler->declineInvite(InvitedAccountID);
    });
    // Blacklisting the inviter declines the invite first.
    connect(menu->addCustomEntry(qtTrId("friends_blacklist_inviter")), &TContextMenuEntryItem::triggered, this, [this, InvitedAccountID] {
        if (auto* handler = friendsProtocolMessageHandler())
            handler->declineInvite(InvitedAccountID);
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
    connect(menu->addCustomEntry(qtTrId("friends_remove_from_blacklist")), &TContextMenuEntryItem::triggered, this, [this, BlacklistedAccountID] {
        if (auto* handler = friendsProtocolMessageHandler())
            handler->removeFromBlacklist(BlacklistedAccountID);
    });
    menu->showCustomMenu();
}


void TFriendsInvitesPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
}

friends::TFriendsStorage* TFriendsInvitesPageController::friendsStorage() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session ? session->friendsStorage().get() : nullptr;
}

friends::TFriendsProtocolMessageHandler* TFriendsInvitesPageController::friendsProtocolMessageHandler() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session ? session->friendsProtocolMessageHandler().get() : nullptr;
}

}
