#pragma once

#include <cip_gen.h>

#include <QObject>
#include <QPointer>

namespace tibia {
namespace gamewindow {

class TSocialDialogController;

/// Button-bar friends control (CIP TSocialButtonController, click).
class TSocialButtonController final : public cip::tibia::gamewindow::TSocialButtonController
{
    Q_OBJECT
public:
    explicit TSocialButtonController(TSocialDialogController* dialog, QObject* parent = nullptr);

    bool highlighted() const override;

public Q_SLOTS:
    void onSocialButtonClicked() override;
    void onFriendsInviteReceived() override;
    void onFriendshipEstablishedReceived() override;
    void onOwnTeamUpdateAvailable() override;
    void onTeamLisUpdateAvailable() override;

private:
    enum class EState { None = 0, InviteReceived = 1, FriendshipEstablished = 2, OwnTeamUpdate = 3 };

    void setState(EState state);

    QPointer<TSocialDialogController> m_dialog;
    EState m_state = EState::None;
};

}
}
