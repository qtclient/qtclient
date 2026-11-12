#pragma once

#include "friends/friendlistmodels.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace gamewindow {

/// FriendsFriendListPage.qml (CIP TFriendsFriendListPageController).
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

public Q_SLOTS:
    /// CIP private slot (moc); confirm unfriend / group change.
    void onConfirmDialogAccepted();

private:
    TFriendListModel* m_model = nullptr;
};

}
}
