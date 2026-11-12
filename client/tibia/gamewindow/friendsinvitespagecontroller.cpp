#include "friendsinvitespagecontroller.h"

namespace tibia::gamewindow {

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
}

void TFriendsInvitesPageController::hideDialogTab()
{
}

void TFriendsInvitesPageController::showContextMenuForSentInvite(const uint InvitedAccountID)
{
    Q_UNUSED(InvitedAccountID);
    // CIP : cancel invite.
}

void TFriendsInvitesPageController::showContextMenuForReceivedInvite(const uint InvitedAccountID)
{
    Q_UNUSED(InvitedAccountID);
    // CIP: accept / decline / group entries.
}

void TFriendsInvitesPageController::showContextMenuForBlacklistedAccount(const uint BlacklistedAccountID)
{
    Q_UNUSED(BlacklistedAccountID);
    // CIP: remove from blacklist.
}

}
