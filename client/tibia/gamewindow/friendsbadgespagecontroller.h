#pragma once

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace gamewindow {

/// FriendsBadgesPage.qml (CIP TFriendsBadgesPageController).
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
    cip::TFriendListModel* m_model = nullptr;
};

}
}
