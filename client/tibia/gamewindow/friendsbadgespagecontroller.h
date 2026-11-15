#pragma once

#include "friends/friendlistmodels.h"

#include <cip_gen.h>

#include <QMetaObject>
#include <QObject>

namespace tibia {
namespace gamewindow {

/// FriendsBadgesPage.qml (CIP TFriendsBadgesPageController): the player's
/// badges from friends::TFriendsStorage, filtered by state.
class TFriendsBadgesPageController final : public cip::tibia::gamewindow::TFriendsBadgesPageController
{
    Q_OBJECT
public:
    explicit TFriendsBadgesPageController(QObject* parent = nullptr);

    EBadgeFilter badgeFilter() const override;
    void setBadgeFilter(EBadgeFilter filter) override;
    QObject* badges() const override;

    void showDialogTab() override;
    void hideDialogTab() override;

    Q_INVOKABLE void updateBadgeVisibility(uint BadgeID, bool ShouldBeVisibleForOthers) override;

private:
    /// Copies the stored badges into the list model.
    void updateBadges();

    TAccountBadgesSortProxyModel* m_model = nullptr;
    QMetaObject::Connection m_badgesConnection;
};

}
}
