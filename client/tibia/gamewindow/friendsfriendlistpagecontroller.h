#pragma once

#include "friends/friendlistmodels.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// FriendsFriendListPage.qml (CIP TFriendsFriendListPageController): the
/// friend list from friends::TFriendsFriendListStorage, filtered by group.
class TFriendsFriendListPageController final : public cip::tibia::gamewindow::TFriendsFriendListPageController
{
    Q_OBJECT
public:
    explicit TFriendsFriendListPageController(QObject* parent = nullptr);

    QObject* friendList() const override;
    EShowFilter showFilter() const override;
    void setShowFilter(EShowFilter ShowFilter) override;
    void showDialogTab() override;
    void hideDialogTab() override;
    Q_INVOKABLE void showAccountDetails(int AccountIDRaw) override;
    Q_INVOKABLE void showContextMenuForAccountID(int AccountIDRaw) override;

Q_SIGNALS:
    /// Account details live on the account search page (the social dialog switches to it).
    void accountDetailsRequested(const QString& characterName);

public Q_SLOTS:
    /// CIP private slot: the unfriend confirmation was accepted.
    void onConfirmDialogAccepted();

private:
    void rebuildRows();

    TFriendListModel* m_model = nullptr;
    TMessageDialogController* m_confirmDialog = nullptr;
    int m_pendingUnfriend = 0;
};

}
}
