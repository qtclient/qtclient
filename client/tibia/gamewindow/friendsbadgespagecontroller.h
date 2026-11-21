#pragma once

#include "friends/friendlistmodels.h"

#include "friends/friendsciptypes.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>

#include <QMetaObject>
#include <QObject>

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

/// FriendsBadgesPage.qml (CIP TFriendsBadgesPageController): the player's
/// badges from friends::TFriendsStorage, filtered by state.
class TFriendsBadgesPageController final : public cip::tibia::gamewindow::TFriendsBadgesPageController
{
    Q_OBJECT
public:
    explicit TFriendsBadgesPageController(QObject* parent = nullptr);
    /// CIP v12: the page reads the session's friends storages while shown.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    EBadgeFilter badgeFilter() const override;
    void setBadgeFilter(EBadgeFilter filter) override;
    QObject* badges() const override;

    void showDialogTab() override;
    void hideDialogTab() override;

    Q_INVOKABLE void updateBadgeVisibility(uint BadgeID, bool ShouldBeVisibleForOthers) override;

private:
    /// The current session's friends storage, or null.
    friends::TFriendsStorage* friendsStorage() const;

    std::shared_ptr<game::IGameSession> m_gameSession;
    /// Copies the stored badges into the list model.
    void updateBadges();

    TAccountBadgesSortProxyModel* m_model = nullptr;
    QMetaObject::Connection m_badgesConnection;
};

}
}
