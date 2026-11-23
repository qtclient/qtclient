#pragma once

#include <cip_gen.h>

#include <QObject>
#include <QPointer>

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

class TSocialDialogController;

/// Button-bar friends control (CIP TSocialButtonController, click).
class TSocialButtonController final : public cip::tibia::gamewindow::TSocialButtonController
{
    Q_OBJECT
public:
    explicit TSocialButtonController(TSocialDialogController* dialog, QObject* parent = nullptr);

    bool highlighted() const override;

    /// The highlight ends (unless the dialog is up); the session's friend
    /// invites and friendships, and its team finder updates, highlight it.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    void onSocialButtonClicked() override;
    void onFriendsInviteReceived() override;
    void onFriendshipEstablishedReceived() override;
    void onOwnTeamUpdateAvailable() override;
    void onTeamLisUpdateAvailable() override;

private:
    enum class EState { None = 0, InviteReceived = 1, FriendshipEstablished = 2, OwnTeamUpdate = 3, TeamListUpdate = 4 };

    /// A notification highlights the button unless the social dialog is up.
    void notify(EState state);
    bool isDialogInStack() const;

    QPointer<TSocialDialogController> m_dialog;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    EState m_state = EState::None;
};

}
}
