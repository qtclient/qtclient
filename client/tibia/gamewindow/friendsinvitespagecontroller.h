#pragma once

#include "friends/friendlistmodels.h"

#include <cip_gen.h>

#include <QMetaObject>
#include <QObject>

namespace tibia {
namespace gamewindow {

/// FriendsInvitesPage.qml (CIP TFriendsInvitesPageController).
class TFriendsInvitesPageController final : public cip::tibia::gamewindow::TFriendsInvitesPageController
{
    Q_OBJECT
public:
    explicit TFriendsInvitesPageController(QObject* parent = nullptr);

    QObject* receivedInvites() const override;
    QObject* sentInvites() const override;
    QObject* blacklist() const override;
    EShownListType shownListType() const override;
    void setShownListType(EShownListType type) override;

    void showDialogTab() override;
    void hideDialogTab() override;

    Q_INVOKABLE void showContextMenuForSentInvite(uint InvitedAccountID) override;
    Q_INVOKABLE void showContextMenuForReceivedInvite(uint InvitedAccountID) override;
    Q_INVOKABLE void showContextMenuForBlacklistedAccount(uint BlacklistedAccountID) override;

private:
    /// Copies the stored invites into the received and sent models.
    void updateInviteLists();
    /// Copies the stored blacklist into its model.
    void updateBlacklist();
    void disconnectStorage();

    TInviteListModel* m_receivedModel = nullptr;
    TInviteListModel* m_sentModel = nullptr;
    TBlacklistModel* m_blacklistModel = nullptr;
    QMetaObject::Connection m_inviteListConnection;
    QMetaObject::Connection m_blacklistConnection;
};

}
}
