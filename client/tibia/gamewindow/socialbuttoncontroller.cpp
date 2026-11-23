#include "socialbuttoncontroller.h"

#include "friends/friendsciptypes.h"
#include "game/gamesessionbase.h"
#include "gamewindow/socialdialogcontroller.h"
#include "teamfinder/teamfinderciptypes.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

namespace tibia::gamewindow {

TSocialButtonController::TSocialButtonController(TSocialDialogController* dialog, QObject* parent)
    : cip::tibia::gamewindow::TSocialButtonController(parent)
    , m_dialog(dialog)
{
}

bool TSocialButtonController::highlighted() const
{
    return m_state != EState::None;
}

bool TSocialButtonController::isDialogInStack() const
{
    return m_dialog && m_dialog->dialog()->isInStack();
}

void TSocialButtonController::notify(const EState state)
{
    if (m_state == state || isDialogInStack())
        return;
    m_state = state;
    Q_EMIT changed();
}

void TSocialButtonController::onSocialButtonClicked()
{
    // Invite -> invites (1), friendship -> friend list (0), own team -> own
    // team (6), otherwise the team finder (5). The highlight ends unless the
    // dialog is already up.
    int page = 5;
    switch (m_state) {
        case EState::InviteReceived: page = 1; break;
        case EState::FriendshipEstablished: page = 0; break;
        case EState::OwnTeamUpdate: page = 6; break;
        default: page = 5; break;
    }
    if (m_state != EState::None && !isDialogInStack()) {
        m_state = EState::None;
        Q_EMIT changed();
    }
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenSocialDialog>(page));
}

void TSocialButtonController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_state != EState::None && !isDialogInStack()) {
        m_state = EState::None;
        Q_EMIT changed();
    }
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    if (friends::TFriendsProtocolMessageHandler* friendsHandler = session->friendsProtocolMessageHandler().get()) {
        m_gameSessionConnections.push_back(connect(friendsHandler, &friends::TFriendsProtocolMessageHandler::newFriendInviteReceived, this,
                                                   &TSocialButtonController::onFriendsInviteReceived));
        m_gameSessionConnections.push_back(connect(friendsHandler, &friends::TFriendsProtocolMessageHandler::newFriendshipReceived, this,
                                                   &TSocialButtonController::onFriendshipEstablishedReceived));
    }
    if (teamfinder::TTeamFinderProtocolMessageHandler* teamFinder = session->teamFinderProtocolMessageHandler().get()) {
        m_gameSessionConnections.push_back(connect(teamFinder, &teamfinder::TTeamFinderProtocolMessageHandler::ownTeamUpdateAvailable, this,
                                                   &TSocialButtonController::onOwnTeamUpdateAvailable));
        m_gameSessionConnections.push_back(connect(teamFinder, &teamfinder::TTeamFinderProtocolMessageHandler::teamLisUpdateAvailable, this,
                                                   &TSocialButtonController::onTeamLisUpdateAvailable));
    }
}

void TSocialButtonController::onFriendsInviteReceived()
{
    notify(EState::InviteReceived);
}

void TSocialButtonController::onFriendshipEstablishedReceived()
{
    notify(EState::FriendshipEstablished);
}

void TSocialButtonController::onOwnTeamUpdateAvailable()
{
    notify(EState::OwnTeamUpdate);
}

void TSocialButtonController::onTeamLisUpdateAvailable()
{
    notify(EState::TeamListUpdate);
}

}
