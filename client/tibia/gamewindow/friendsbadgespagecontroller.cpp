#include "friendsbadgespagecontroller.h"

namespace tibia::gamewindow {

TFriendsBadgesPageController::TFriendsBadgesPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsBadgesPageController(parent)
    , m_model(new cip::TFriendListModel(this))
{
    m_badges = m_model;
}

TFriendsBadgesPageController::EBadgeFilter TFriendsBadgesPageController::badgeFilter() const
{
    return m_badgeFilter;
}

void TFriendsBadgesPageController::setBadgeFilter(const EBadgeFilter filter)
{
    cip::tibia::gamewindow::TFriendsBadgesPageController::setBadgeFilter(filter);
}

QObject* TFriendsBadgesPageController::badges() const
{
    return m_badges;
}

void TFriendsBadgesPageController::showDialogTab()
{
}

void TFriendsBadgesPageController::hideDialogTab()
{
}

void TFriendsBadgesPageController::updateBadgeVisibility(const uint BadgeID, const bool ShouldBeVisibleForOthers)
{
    Q_UNUSED(BadgeID);
    Q_UNUSED(ShouldBeVisibleForOthers);
}

}
