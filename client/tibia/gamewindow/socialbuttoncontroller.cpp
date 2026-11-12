#include "socialbuttoncontroller.h"

#include "gamewindow/socialdialogcontroller.h"
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
    return m_highlighted;
}

void TSocialButtonController::setState(const EState state)
{
    m_state = state;
    const bool highlighted = state != EState::None;
    if (highlighted == m_highlighted)
        return;
    m_highlighted = highlighted;
    Q_EMIT changed();
}

void TSocialButtonController::onSocialButtonClicked()
{
    // CIP : invite → Invites (param 1), friendship → friend list (0),
    // else team finder JoinTeam (param 5).
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_SOCIAL_DIALOG));
    action->parameter = m_state == EState::InviteReceived           ? 1
                        : m_state == EState::FriendshipEstablished ? 0
                        : m_state == EState::OwnTeamUpdate         ? 6
                                                                   : 5;
    setState(EState::None);
    input::publish(action);
}

void TSocialButtonController::onFriendsInviteReceived()
{
    // CIP .
    if (m_state != EState::None || !m_dialog || m_dialog->dialog()->isShown())
        return;
    setState(EState::InviteReceived);
}

void TSocialButtonController::onFriendshipEstablishedReceived()
{
    // CIP .
    if (!m_dialog || m_dialog->dialog()->isShown())
        return;
    setState(EState::FriendshipEstablished);
}

void TSocialButtonController::onOwnTeamUpdateAvailable()
{
    // CIP .
    if (m_state != EState::None || !m_dialog || m_dialog->dialog()->isShown())
        return;
    setState(EState::OwnTeamUpdate);
}

void TSocialButtonController::onTeamLisUpdateAvailable()
{
    // CIP — same highlight latch as own-team update.
    onOwnTeamUpdateAvailable();
}

}
