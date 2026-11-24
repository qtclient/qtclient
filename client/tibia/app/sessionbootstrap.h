#pragma once

#include "input/gameactiondispatch.h"
#include "app/launcherenv.h"

#include <QMetaObject>
#include <QObject>
#include <QUrl>

namespace tibia {
namespace client {
class TGameClient;
}
}
namespace tibia {
namespace gamewindow {
class TCreateAccountDialogController;
class TCreateCharacterDialogController;
class TGameWindowController;
class TMessageDialogController;
}
}

namespace tibia {
namespace app {
class TLoginWaitDialogController;

/// CIP start-screen session chrome around TGameClient (stock qtclient host):
/// - login / game errors close the wait dialog (the application shows them);
/// - WaitDialog while the character list loads and while entering the world;
/// - CreateAccountDialog from the login mask;
/// - CreateCharacterDialog from the character list / SHOW_CREATE_CHARACTER_DIALOG;
/// - keep_renderer_after_restart after a graphics-engine change restart.
///
/// client_too_old / Restart is wired in main.cpp (ShowClientOutOfDate).
/// Product packs may replace the create-character controller via
/// installCreateCharacter (product pack pack QML).
class TSessionBootstrap final : public QObject
{
    Q_OBJECT
public:
    TSessionBootstrap(client::TGameClient* client, const LauncherEnv& env,
                      gamewindow::TGameWindowController* gameWindow, QObject* parent = nullptr);
    ~TSessionBootstrap() override;

    /// The game client's create-character dialog.
    gamewindow::TCreateCharacterDialogController* createCharacter() const;
    /// The game client's create-account dialog.
    gamewindow::TCreateAccountDialogController* createAccount() const;

    /// Pack skin: replace stock create-character (the game client takes it
    /// over). Keeps the same portal create-account service URL.
    void installCreateCharacter(gamewindow::TCreateCharacterDialogController* controller);

    void openCreateAccount();
    /// CIP's connecting dialog closes (the game session disconnected).
    void closeConnectingDialog();

private:
    void openCreateCharacter(bool accountHasNoCharacters);
    void openCreateCharacterIfAccountEmpty();
    void promptKeepGraphicsEngine();
    void bindLoginWait(gamewindow::TGameWindowController* gameWindow);
    void wireCreateCharacter();

    client::TGameClient* m_client;
    gamewindow::TGameWindowController* m_gameWindow = nullptr;
    QUrl m_createAccountService;
    TLoginWaitDialogController* m_loginWait = nullptr;
    gamewindow::TMessageDialogController* m_keepRenderer = nullptr;
    QMetaObject::Connection m_createCharacterIfEmpty;
    QMetaObject::Connection m_characterCreated;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
