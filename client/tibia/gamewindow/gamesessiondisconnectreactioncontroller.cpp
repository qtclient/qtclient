#include "gamewindow/gamesessiondisconnectreactioncontroller.h"

#include "authentication/authenticationprocesscontroller.h"
#include "game/igamesession.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/waitdialogcontroller.h"

namespace tibia::gamewindow {

namespace {

using EButton = TMessageDialogController::Button;

/// The wait before reconnection attempt `attempt`: one second for the first
/// three, then two, four and five seconds.
qint64 reconnectionWaitSeconds(const int attempt)
{
    if (attempt >= 1 && attempt <= 3)
        return 1;
    if (attempt == 4)
        return 2;
    return attempt == 5 ? 4 : 5;
}

}

TGameSessionDisconnectReactionController::TGameSessionDisconnectReactionController(
    std::shared_ptr<authentication::TAuthenticationProcessController> AuthenticationProcessController,
    std::shared_ptr<TGameWindowController> GameWindowController, QObject* parent)
    : cip::tibia::gamewindow::TGameSessionDisconnectReactionController(parent)
    , m_loginErrorDialog(std::make_shared<TMessageDialogController>())
    , m_connectionErrorDialog(std::make_shared<TMessageDialogController>())
    , m_clientOutOfDateDialog(std::make_shared<TMessageDialogController>())
    , m_loginWaitDialog(std::make_shared<TWaitDialogController>())
    , m_pingTimeoutWaitDialog(std::make_shared<TWaitDialogController>())
    , m_reconnectionWaitDialog(std::make_shared<TWaitDialogController>())
    , m_authenticationProcessController(std::move(AuthenticationProcessController))
    , m_gameWindowController(std::move(GameWindowController))
{
    m_loginErrorDialog->setCloseOnButtonClicked(true);
    m_connectionErrorDialog->setCloseOnButtonClicked(true);
    connect(m_connectionErrorDialog.get(), &TMessageDialogController::buttonClicked, this,
            &TGameSessionDisconnectReactionController::onDialogResponseShowCharacterSelection);
    connect(m_loginWaitDialog.get(), &TWaitDialogController::waitTimeElapsed, this,
            &TGameSessionDisconnectReactionController::loginAgainAfterLoginWaitElapsed);
    connect(m_loginWaitDialog.get(), &TWaitDialogController::waitAborted, this,
            &TGameSessionDisconnectReactionController::onWaitAbortedShowCharacterSelection);
    connect(m_pingTimeoutWaitDialog.get(), &TWaitDialogController::waitTimeElapsed, this,
            &TGameSessionDisconnectReactionController::onPingTimeoutWaitTimeElapsed);
    connect(m_pingTimeoutWaitDialog.get(), &TWaitDialogController::waitAborted, this,
            &TGameSessionDisconnectReactionController::onPingTimeoutWaitTimeAborted);
    connect(m_reconnectionWaitDialog.get(), &TWaitDialogController::waitTimeElapsed, this,
            &TGameSessionDisconnectReactionController::loginAgainAfterLoginWaitElapsed);
    connect(m_reconnectionWaitDialog.get(), &TWaitDialogController::waitAborted, this,
            &TGameSessionDisconnectReactionController::onFailedConnectionWaitAborted);
    // Restart patches the client; any other answer returns to the character selection.
    connect(m_clientOutOfDateDialog.get(), &TMessageDialogController::buttonClicked, this,
            [this](const EButton ButtonIdentifier) {
                if (ButtonIdentifier == EButton::YES) {
                    Q_EMIT quitAndPatchClient();
                } else {
                    m_gameWindowController->showStartScreen();
                    m_authenticationProcessController->showCharacterSelection();
                    m_showCharacterSelectionOnDisconnect = true;
                }
                m_clientOutOfDateDialog->dialog()->close();
            });
}

TGameSessionDisconnectReactionController::~TGameSessionDisconnectReactionController() = default;

void TGameSessionDisconnectReactionController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_pingTimeoutWaitDialog->close();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = GameSession;
    if (!m_gameSession)
        return;
    m_connected = false;
    m_loginSuccessful = false;
    m_ignoreConnectionError = false;
    game::IGameSession* session = m_gameSession.get();
    using Self = TGameSessionDisconnectReactionController;
    m_gameSessionConnections.add(connect(session, &game::IGameSession::connectionError, this, &Self::onConnectionError));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::gameLoginErrorMessage, this, &Self::onGameLoginErrorMessage));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::gameLoginWaitMessage, this, &Self::onGameLoginWaitMessage));
    m_gameSessionConnections.add(
        connect(session, &game::IGameSession::gameLoginChallengeMessage, this, &Self::onGameLoginChallengeMessage));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::gameLoginSuccessful, this, &Self::onGameSessionLoginSuccessful));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::worldEntered, this, &Self::onWorldEntered));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::deadMessage, this, &Self::onDeadMessage));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::sessionEndInformation, this, &Self::onSessionEndInformation));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::disconnected, this, &Self::onGameSessionDisconnected));
    m_gameSessionConnections.add(connect(session, &game::IGameSession::pingTimeout, this, &Self::onGameSessionPingTimeout));
    m_gameSessionConnections.add(
        connect(session, &game::IGameSession::pingTimeoutRecovered, this, &Self::onGameSessionPingTimeoutRecovered));
}

void TGameSessionDisconnectReactionController::disableReconnectionAttempts()
{
    m_reconnectionAttempts = 10;
}

void TGameSessionDisconnectReactionController::resetShowCharacterSelectionOnDisconnect()
{
    m_showCharacterSelectionOnDisconnect = false;
}

void TGameSessionDisconnectReactionController::ignoreNextDisconnect()
{
    m_ignoreConnectionError = true;
    m_showCharacterSelectionOnDisconnect = false;
}

void TGameSessionDisconnectReactionController::resetReconnectRequested()
{
    m_reconnectRequested = false;
}

void TGameSessionDisconnectReactionController::onGameSessionConnected()
{
    m_connected = true;
    m_ignoreConnectionError = false;
    m_showCharacterSelectionOnDisconnect = true;
    m_reconnectionAttempts = 0;
}

void TGameSessionDisconnectReactionController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != input::EGameAction::QUIT_GAME)
        return;
    const auto* quitGame = input::gameActionAs<input::TGameActionQuitGame>(pGameAction);
    if (!quitGame)
        return;
    // Quitting without leaving the game logs in again once disconnected.
    m_reconnectRequested = !quitGame->flag();
    if (m_reconnectRequested)
        m_ignoreConnectionError = true;
}

void TGameSessionDisconnectReactionController::onGameSessionLoginSuccessful()
{
    m_loginSuccessful = true;
}

void TGameSessionDisconnectReactionController::onWorldEntered()
{
    m_ignoreConnectionError = false;
    m_showCharacterSelectionOnDisconnect = true;
    m_reconnectRequested = false;
}

void TGameSessionDisconnectReactionController::onDeadMessage()
{
    m_ignoreConnectionError = true;
    m_showCharacterSelectionOnDisconnect = true;
}

void TGameSessionDisconnectReactionController::onSessionEndInformation(const int Reason)
{
    switch (Reason) {
    case 0:
        // Logged out: back to the character selection unless logging in again.
        m_ignoreConnectionError = true;
        m_showCharacterSelectionOnDisconnect = !m_reconnectRequested;
        return;
    case 1:
    case 3:
    case 4:
    case 101:
        m_ignoreConnectionError = true;
        m_showCharacterSelectionOnDisconnect = true;
        return;
    case 2:
    case 99:
        m_ignoreConnectionError = false;
        m_showCharacterSelectionOnDisconnect = true;
        return;
    case 100:
        m_reconnectRequested = false;
        return;
    default:
        return;
    }
}

void TGameSessionDisconnectReactionController::onGameSessionDisconnected()
{
    if (m_showCharacterSelectionOnDisconnect) {
        m_authenticationProcessController->showCharacterSelection();
        m_showCharacterSelectionOnDisconnect = true;
    }
    m_pingTimeoutWaitDialog->close();
    m_gameWindowController->showStartScreen();
}

void TGameSessionDisconnectReactionController::onConnectionError(const QString& ErrorDescription)
{
    if (!m_connected && m_reconnectionAttempts < 9) {
        ++m_reconnectionAttempts;
        m_reconnectionWaitDialog->startWait(qtTrId("login_wait_please_wait"),
                                            qtTrId("gameserver_reconnection_attempt").arg(m_reconnectionAttempts),
                                            reconnectionWaitSeconds(m_reconnectionAttempts), qtTrId("login_wait_delay"));
        return;
    }
    if (!m_ignoreConnectionError) {
        m_showCharacterSelectionOnDisconnect = false;
        m_reconnectionAttempts = 0;
        const QString caption = qtTrId(m_loginSuccessful ? "connection_lost_caption" : "connection_failed_caption");
        const QString text = m_loginSuccessful ? qtTrId("connection_lost_with_reason").arg(ErrorDescription)
                                               : qtTrId("connection_failed_with_reason").arg(ErrorDescription);
        m_connectionErrorDialog->setButtons(okDialogButtons());
        m_connectionErrorDialog->showMessage(caption, text);
    }
    m_gameWindowController->showStartScreen();
}

void TGameSessionDisconnectReactionController::onGameLoginErrorMessage(const QString& ErrorMessage, const int Action)
{
    m_ignoreConnectionError = true;
    m_showCharacterSelectionOnDisconnect = false;
    // CIP's ELoginErrorAction: 0 back to the character selection, 1 back to
    // the login dialog, 2 the client is too old.
    if (Action == 2) {
        m_clientOutOfDateDialog->setButtons(
            { new TMessageDialogButton(EButton::YES, qtTrId("restart"), TMessageDialogButton::RETURN_KEYS) });
        m_clientOutOfDateDialog->showMessage(qtTrId("info"), qtTrId("client_too_old"));
        return;
    }
    if (Action != 0 && Action != 1)
        return;
    m_loginErrorDialog->setButtons(okDialogButtons());
    m_loginErrorDialog->showMessage(qtTrId("loginerror_caption"), ErrorMessage);
    m_loginErrorDialogConnections.disconnectAll();
    if (Action == 1)
        m_loginErrorDialogConnections.add(connect(m_loginErrorDialog.get(), &TMessageDialogController::buttonClicked, this,
                                                  &TGameSessionDisconnectReactionController::onDialogResponseShowLoginDialog));
    else
        m_loginErrorDialogConnections.add(connect(m_loginErrorDialog.get(), &TMessageDialogController::buttonClicked, this,
                                                  &TGameSessionDisconnectReactionController::onDialogResponseShowCharacterSelection));
    m_gameWindowController->showStartScreen();
}

void TGameSessionDisconnectReactionController::onGameLoginWaitMessage(const QString& WaitMessage, const int WaitSeconds)
{
    m_ignoreConnectionError = true;
    m_showCharacterSelectionOnDisconnect = false;
    m_loginWaitDialog->startWait(qtTrId("login_wait_please_wait"), WaitMessage, WaitSeconds, qtTrId("login_wait_delay"));
    m_gameWindowController->showStartScreen();
}

void TGameSessionDisconnectReactionController::onGameLoginChallengeMessage()
{
    m_connected = true;
    m_ignoreConnectionError = false;
    m_showCharacterSelectionOnDisconnect = true;
    m_reconnectionAttempts = 0;
}

void TGameSessionDisconnectReactionController::onDialogResponseShowCharacterSelection()
{
    m_authenticationProcessController->showCharacterSelection();
    m_showCharacterSelectionOnDisconnect = true;
}

void TGameSessionDisconnectReactionController::onDialogResponseShowLoginDialog()
{
    m_authenticationProcessController->resetSessionData();
}

void TGameSessionDisconnectReactionController::onWaitAbortedShowCharacterSelection()
{
    m_authenticationProcessController->showCharacterSelection();
    m_showCharacterSelectionOnDisconnect = true;
}

void TGameSessionDisconnectReactionController::onGameSessionPingTimeout()
{
    m_pingTimeoutWaitDialog->startWait(qtTrId("warning"), qtTrId("game_server_not_responding"), 60,
                                       qtTrId("game_server_not_responding_delay"));
}

void TGameSessionDisconnectReactionController::onGameSessionPingTimeoutRecovered()
{
    m_pingTimeoutWaitDialog->close();
}

void TGameSessionDisconnectReactionController::onPingTimeoutWaitTimeElapsed()
{
    if (!m_gameSession)
        return;
    m_ignoreConnectionError = true;
    m_showCharacterSelectionOnDisconnect = true;
    m_gameSession->disconnectNow();
}

void TGameSessionDisconnectReactionController::onPingTimeoutWaitTimeAborted()
{
    if (!m_gameSession)
        return;
    m_ignoreConnectionError = true;
    m_showCharacterSelectionOnDisconnect = true;
    m_gameSession->disconnectNow();
}

void TGameSessionDisconnectReactionController::onFailedConnectionWaitAborted()
{
    m_reconnectionAttempts = 0;
    m_gameWindowController->showStartScreen();
    m_authenticationProcessController->resetSessionData();
}

}
