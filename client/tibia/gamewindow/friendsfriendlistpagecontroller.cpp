#include "friendsfriendlistpagecontroller.h"

namespace tibia::gamewindow {

TFriendsFriendListPageController::TFriendsFriendListPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsFriendListPageController(parent)
    , m_model(new TFriendListModel(this))
{
    m_friendList = m_model;
}

QObject* TFriendsFriendListPageController::friendList() const
{
    return m_friendList;
}

TFriendsFriendListPageController::EShowFilter TFriendsFriendListPageController::showFilter() const
{
    return m_showFilter;
}

void TFriendsFriendListPageController::setShowFilter(const EShowFilter ShowFilter)
{
    cip::tibia::gamewindow::TFriendsFriendListPageController::setShowFilter(ShowFilter);
}

void TFriendsFriendListPageController::showDialogTab()
{
    // CIP: friend-list storage refresh when the tab becomes visible.
}

void TFriendsFriendListPageController::hideDialogTab()
{
}

void TFriendsFriendListPageController::showAccountDetails(const int AccountIDRaw)
{
    Q_UNUSED(AccountIDRaw);
    // CIP: opens AccountSearch for the row's account.
}

void TFriendsFriendListPageController::showContextMenuForAccountID(const int AccountIDRaw)
{
    Q_UNUSED(AccountIDRaw);
    // CIP: group / unfriend context menu for the selected friend.
}

void TFriendsFriendListPageController::onConfirmDialogAccepted()
{
    // CIP private slot after the confirm dialog for unfriend / group change.
}

}
