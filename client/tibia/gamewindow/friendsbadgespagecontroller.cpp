#include "friendsbadgespagecontroller.h"

#include "friends/friendsciptypes.h"
#include "protocol/clientmessages.h"

namespace tibia::gamewindow {

namespace {

constexpr uint8_t kLoadBadges = 12;

}

TFriendsBadgesPageController::TFriendsBadgesPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsBadgesPageController(parent)
    , m_model(new TFriendsRowsModel({ "badgeID", "badgeIcon", "badgeName", "badgeDescription", "isBadgeUnlocked", "isBadgeVisible" }, this))
{
    m_badges = m_model;
    connect(friends::TFriendsStorage::instance(), &friends::TFriendsStorage::accountBadgesChanged, this,
            &TFriendsBadgesPageController::rebuildRows);
    rebuildRows();
}

void TFriendsBadgesPageController::rebuildRows()
{
    QVector<QVariantMap> rows;
    for (const auto& badge : friends::TFriendsStorage::instance()->badges()) {
        const bool shown = [&] {
            switch (m_badgeFilter) {
            case ShowLocked:
                return !badge.unlocked();
            case ShowUnlocked:
                return badge.unlocked();
            case ShowPublic:
                return badge.unlocked() && badge.visible();
            case ShowHidden:
                return badge.unlocked() && !badge.visible();
            default:
                return true;
            }
        }();
        if (!shown)
            continue;
        rows.push_back({
            { QStringLiteral("badgeID"), static_cast<uint>(badge.badge_id()) },
            { QStringLiteral("badgeIcon"), QString::fromStdString(badge.icon()) },
            { QStringLiteral("badgeName"), QString::fromStdString(badge.name()) },
            { QStringLiteral("badgeDescription"), QString() },
            { QStringLiteral("isBadgeUnlocked"), badge.unlocked() },
            { QStringLiteral("isBadgeVisible"), badge.visible() },
        });
    }
    m_model->setRows(std::move(rows));
}

TFriendsBadgesPageController::EBadgeFilter TFriendsBadgesPageController::badgeFilter() const
{
    return m_badgeFilter;
}

void TFriendsBadgesPageController::setBadgeFilter(const EBadgeFilter filter)
{
    if (filter == m_badgeFilter)
        return;
    cip::tibia::gamewindow::TFriendsBadgesPageController::setBadgeFilter(filter);
    rebuildRows();
}

QObject* TFriendsBadgesPageController::badges() const
{
    return m_badges;
}

void TFriendsBadgesPageController::showDialogTab()
{
    protocol::gameclient::sendFriendSystemAction(kLoadBadges);
}

void TFriendsBadgesPageController::hideDialogTab()
{
}

void TFriendsBadgesPageController::updateBadgeVisibility(const uint BadgeID, const bool ShouldBeVisibleForOthers)
{
    protocol::gameclient::sendFriendSystemBadgeVisibility(BadgeID, ShouldBeVisibleForOthers);
}

}
