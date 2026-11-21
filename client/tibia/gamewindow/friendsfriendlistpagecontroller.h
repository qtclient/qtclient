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

class TMessageDialogController;

/// FriendsFriendListPage.qml (CIP TFriendsFriendListPageController): the
/// friend list from friends::TFriendsFriendListStorage, sorted by name and
/// filtered by group.
class TFriendsFriendListPageController final : public cip::tibia::gamewindow::TFriendsFriendListPageController
{
    Q_OBJECT
public:
    explicit TFriendsFriendListPageController(QObject* parent = nullptr);
    /// CIP v12: the page reads the session's friends storages while shown.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QObject* friendList() const override;
    EShowFilter showFilter() const override;
    void setShowFilter(EShowFilter ShowFilter) override;
    void showDialogTab() override;
    void hideDialogTab() override;
    Q_INVOKABLE void showAccountDetails(int AccountIDRaw) override;
    Q_INVOKABLE void showContextMenuForAccountID(int AccountIDRaw) override;

public Q_SLOTS:
    /// CIP private slot: the unfriend confirmation was accepted.
    void onConfirmDialogAccepted();

private:
    /// The current session's friends storage, or null.
    friends::TFriendsStorage* friendsStorage() const;
    friends::TFriendsFriendListStorage* friendListStorage() const;

    std::shared_ptr<game::IGameSession> m_gameSession;
    /// Copies the stored friend list into the list model.
    void updateFriendList();
    /// The unfriend confirmation for the pending account.
    void showUnfriendConfirmation();
    /// Unfriends on the server and drops the account from the list.
    void removeFriend(uint32_t accountId);
    /// Moves the account into another group on the server and in the list.
    void changeFriendGroup(uint32_t accountId, int friendGroup);

    TFriendListSortProxyModel* m_model = nullptr;
    TMessageDialogController* m_confirmDialog = nullptr;
    QMetaObject::Connection m_friendListConnection;
    int m_pendingUnfriend = 0;
};

}
}
