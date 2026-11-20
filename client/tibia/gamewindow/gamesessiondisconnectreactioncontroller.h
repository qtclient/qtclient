#pragma once

#include "input/gameaction.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QString>

#include <memory>

namespace tibia::authentication {
class TAuthenticationProcessController;
}
namespace tibia::game {
class IGameSession;
}

namespace tibia::gamewindow {

class TGameWindowController;
class TMessageDialogController;
class TWaitDialogController;

/// CIP gamewindow::TGameSessionDisconnectReactionController: how the client
/// reacts when the current game session cannot connect, is refused or ends.
/// A failed connection is retried (up to nine waits of growing length), a
/// lost connection or a refused login shows its message, a server login wait
/// counts down and logs in again, a ping timeout offers to give up; afterwards
/// the start screen shows and, unless the session ended for good, the
/// character selection.
class TGameSessionDisconnectReactionController final : public cip::tibia::gamewindow::TGameSessionDisconnectReactionController
{
    Q_OBJECT
public:
    TGameSessionDisconnectReactionController(std::shared_ptr<authentication::TAuthenticationProcessController> AuthenticationProcessController,
                                             std::shared_ptr<TGameWindowController> GameWindowController,
                                             QObject* parent = nullptr);
    ~TGameSessionDisconnectReactionController() override;

    /// CIP: follows `GameSession`'s connection, login and end signals (null:
    /// none); the ping-timeout wait of the previous session closes.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// CIP: no reconnection attempts for the session about to connect (a
    /// replay or the tutorial).
    void disableReconnectionAttempts();
    /// CIP: a new login: the next disconnect does not open the character
    /// selection by itself.
    void resetShowCharacterSelectionOnDisconnect();
    /// CIP: the player chose to log out and log in again (QUIT_GAME without
    /// leaving the game); the client then reconnects on disconnect.
    bool reconnectRequested() const { return m_reconnectRequested; }

public Q_SLOTS:
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private Q_SLOTS:
    void onGameSessionLoginSuccessful();
    void onWorldEntered();
    void onDeadMessage();
    void onSessionEndInformation(int Reason);
    void onGameSessionDisconnected();
    void onConnectionError(const QString& ErrorDescription);
    void onGameLoginErrorMessage(const QString& ErrorMessage, int Action);
    void onGameLoginWaitMessage(const QString& WaitMessage, int WaitSeconds);
    void onGameLoginChallengeMessage();
    void onDialogResponseShowCharacterSelection();
    void onDialogResponseShowLoginDialog();
    void onWaitAbortedShowCharacterSelection();
    void onGameSessionPingTimeout();
    void onGameSessionPingTimeoutRecovered();
    void onPingTimeoutWaitTimeElapsed();
    void onPingTimeoutWaitTimeAborted();
    void onFailedConnectionWaitAborted();

private:
    std::shared_ptr<TMessageDialogController> m_loginErrorDialog;
    std::shared_ptr<TMessageDialogController> m_connectionErrorDialog;
    std::shared_ptr<TMessageDialogController> m_clientOutOfDateDialog;
    std::shared_ptr<TWaitDialogController> m_loginWaitDialog;
    std::shared_ptr<TWaitDialogController> m_pingTimeoutWaitDialog;
    std::shared_ptr<TWaitDialogController> m_reconnectionWaitDialog;
    /// Waits for a reconnection so far; nine at most.
    int m_reconnectionAttempts = 0;
    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    shared::TQtConnectionManager m_loginErrorDialogConnections;
    std::shared_ptr<authentication::TAuthenticationProcessController> m_authenticationProcessController;
    std::shared_ptr<TGameWindowController> m_gameWindowController;
    /// The game server answered (connected or sent its login challenge).
    bool m_connected = false;
    bool m_loginSuccessful = false;
    /// The disconnect is expected: no connection error message.
    bool m_ignoreConnectionError = false;
    /// The disconnect opens the character selection.
    bool m_showCharacterSelectionOnDisconnect = true;
    bool m_reconnectRequested = false;
};

}
