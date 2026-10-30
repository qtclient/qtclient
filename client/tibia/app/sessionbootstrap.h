#pragma once

#include "app/launcherenv.h"

#include <QMetaObject>
#include <QObject>
#include <QUrl>

namespace tibia::client {
class TGameClient;
}
namespace tibia::gamewindow {
class TCreateAccountDialogController;
class TCreateCharacterDialogController;
class TGameWindowController;
class TMessageDialogController;
class TWaitDialogController;
}

namespace tibia::app {

/// CIP start-screen session chrome around TGameClient (stock qtclient host):
/// - login / game errors → MessageDialog (showErrorAsPopupAndCriticalLog);
/// - WaitDialog while the character list loads and while entering the world;
/// - CreateAccountDialog from the login mask;
/// - CreateCharacterDialog from the character list / SHOW_CREATE_CHARACTER_DIALOG;
/// - keep_renderer_after_restart after a graphics-engine change restart.
///
/// client_too_old / Restart is wired in main.cpp (ShowClientOutOfDate).
/// Product packs may replace the create-character controller via
/// installCreateCharacter (IdleOT pack QML).
class TSessionBootstrap final : public QObject
{
    Q_OBJECT
public:
    TSessionBootstrap(client::TGameClient* client, const LauncherEnv& env,
                      gamewindow::TGameWindowController* gameWindow, QObject* parent = nullptr);
    ~TSessionBootstrap() override;

    gamewindow::TCreateCharacterDialogController* createCharacter() const { return m_createCharacter; }
    gamewindow::TCreateAccountDialogController* createAccount() const { return m_createAccount; }

    /// Pack skin: replace stock create-character (takes ownership). Keeps the
    /// same portal create-account service URL.
    void installCreateCharacter(gamewindow::TCreateCharacterDialogController* controller);

    void openCreateAccount();

private:
    void showError(const QString& message);
    void openCreateCharacter(bool accountHasNoCharacters);
    void openCreateCharacterIfAccountEmpty();
    void promptKeepGraphicsEngine();
    void bindLoginWait(gamewindow::TGameWindowController* gameWindow);
    void wireCreateCharacter();

    client::TGameClient* m_client;
    gamewindow::TGameWindowController* m_gameWindow = nullptr;
    QUrl m_createAccountService;
    gamewindow::TCreateCharacterDialogController* m_createCharacter = nullptr;
    gamewindow::TCreateAccountDialogController* m_createAccount = nullptr;
    gamewindow::TWaitDialogController* m_loginWait = nullptr;
    gamewindow::TMessageDialogController* m_error = nullptr;
    gamewindow::TMessageDialogController* m_keepRenderer = nullptr;
    QMetaObject::Connection m_createCharacterRequested;
    QMetaObject::Connection m_createCharacterIfEmpty;
    QMetaObject::Connection m_characterCreated;
};

}
