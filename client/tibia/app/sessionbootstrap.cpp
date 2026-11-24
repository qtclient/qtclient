#include "app/sessionbootstrap.h"

#include "platform/platform.h"

#include "app/clientoptions.h"
#include "app/graphicsrestart.h"
#include "client/gameclient.h"
#include "controllers/characterselectioncontroller.h"
#include "gamewindow/createaccountdialogcontroller.h"
#include "gamewindow/createcharacterdialogcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "app/loginwaitdialogcontroller.h"
#include "input/gameactionhandlers.h"

#include <QCoreApplication>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <tibia/game/gamesession.h>

namespace tibia::app {

namespace {

using Box = gamewindow::TMessageDialogController;
using Button = Box::Button;

Box* makeBox(QObject* parent, const QString& accept, const QString& discard)
{
    auto* box = new Box(parent);
    box->setCloseOnButtonClicked(true);
    box->setTextCentered(true);
    QList<gamewindow::TMessageDialogButton*> buttons;
    if (!accept.isEmpty())
        buttons.append(new gamewindow::TMessageDialogButton(accept, Button::OK, QKeySequence(QStringLiteral("Return"))));
    if (!discard.isEmpty())
        buttons.append(new gamewindow::TMessageDialogButton(discard, Button::CANCEL, QKeySequence(QStringLiteral("Esc"))));
    box->setButtons(buttons);
    return box;
}

}

TSessionBootstrap::TSessionBootstrap(client::TGameClient* client, const LauncherEnv& env,
                                     gamewindow::TGameWindowController* gameWindow, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_gameWindow(gameWindow)
    , m_loginWait(new TLoginWaitDialogController(this))
    , m_keepRenderer(makeBox(this, {}, {}))
{
    m_keepRenderer->setYesNoButtons();

    m_createAccountService = env.createAccountUrl();

    connect(m_keepRenderer, &Box::acceptClicked, this, [] { graphicsrestart::clearPendingConfirm(); });
    connect(m_keepRenderer, &Box::discardClicked, this, [] {
        graphicsrestart::revertPending();
        tibia::platform::restartApplication();
    });
    QTimer::singleShot(0, this, &TSessionBootstrap::promptKeepGraphicsEngine);

    wireCreateCharacter();

    connect(createAccount(), &gamewindow::TCreateAccountDialogController::accountCreated, this,
            [this](const QString& email, const QString& password) {
                if (m_gameWindow) {
                    m_gameWindow->setAuthenticated(true);
                    m_loginWait->showCharacterList();
                }
                m_client->submitCredentials(email, password);
            });
    connect(m_loginWait, &TLoginWaitDialogController::aborted, this, [this] {
        m_client->abortPendingEnter();
        if (m_loginWait->abortedPhase() == TLoginWaitDialogController::Phase::GameWorld) {
            // CIP: Abort while entering the world returns to the character list.
            m_client->ensureLoginSurface();
            return;
        }
        if (m_gameWindow)
            m_gameWindow->setAuthenticated(false);
    });

    if (m_gameWindow)
        bindLoginWait(m_gameWindow);
}

TSessionBootstrap::~TSessionBootstrap()
{
}

gamewindow::TCreateCharacterDialogController* TSessionBootstrap::createCharacter() const
{
    return m_client->createCharacterDialogController();
}

gamewindow::TCreateAccountDialogController* TSessionBootstrap::createAccount() const
{
    return m_client->createAccountDialogController();
}

void TSessionBootstrap::installCreateCharacter(gamewindow::TCreateCharacterDialogController* controller)
{
    if (!controller || controller == createCharacter())
        return;
    m_client->setCreateCharacterDialogController(controller);
    controller->setServiceUrl(m_createAccountService);
    wireCreateCharacter();
}

void TSessionBootstrap::wireCreateCharacter()
{
    QObject::disconnect(m_createCharacterIfEmpty);
    QObject::disconnect(m_characterCreated);

    m_createCharacterIfEmpty =
        connect(m_client, &client::TGameClient::characterSelectionRequested, this,
                &TSessionBootstrap::openCreateCharacterIfAccountEmpty);
    m_characterCreated =
        connect(createCharacter(), &gamewindow::TCreateCharacterDialogController::characterCreated, this,
                [this](const QString& name, const QJsonObject& reply) {
                    m_client->refreshAccount(reply);
                });
    // CIP : SHOW_CREATE_CHARACTER_DIALOG (parameter: account has no characters).
    m_gameActions.on(input::EGameAction::SHOW_CREATE_CHARACTER_DIALOG,
                            [self = QPointer<TSessionBootstrap>(this)](const input::gameactions::TGameActionShowCreateCharacterDialog& action) {
                                if (!self)
                                    return true;
                                self->openCreateCharacter(action.accountIsEmpty());
                                return true;
                            });
}

void TSessionBootstrap::openCreateCharacterIfAccountEmpty()
{
    if (m_client->characterSelection()->characters().empty())
        openCreateCharacter(true);
}

void TSessionBootstrap::bindLoginWait(gamewindow::TGameWindowController* gameWindow)
{
    connect(gameWindow, &gamewindow::TGameWindowController::loginRequested, this,
            [this, gameWindow](const QString& email, const QString& password) {
                if (email.trimmed().isEmpty() || password.isEmpty())
                    return;
                // CIP hides the login mask while the account service answers.
                gameWindow->setAuthenticated(true);
                m_loginWait->showCharacterList();
            });
    connect(m_client, &client::TGameClient::loginStarted, this, [this] { m_loginWait->showCharacterList(); });
    connect(m_client, &client::TGameClient::characterSelectionRequested, this, [this] { m_loginWait->close(); });
    // Ok hides the character list before the game window exists.
    connect(m_client, &client::TGameClient::characterSelectionClosed, this, [this] {
        if (tibia::game::isOnline() || tibia::game::isSwitchingCharacter())
            return;
        if (m_client->account().session.sessionKey.isEmpty())
            return;
        m_loginWait->showGameWorld();
    });
    connect(m_client, &client::TGameClient::worldEntered, this, [this] { m_loginWait->close(); });
    // The game session's disconnect reaction returns to the start screen when a login fails.
    connect(gameWindow, &gamewindow::TGameWindowController::startScreenNowDisplayed, this, [this] { m_loginWait->close(); });
    connect(m_client, &client::TGameClient::returnToLoginRequested, this, [this] { m_loginWait->close(); });
    connect(m_client, &client::TGameClient::showErrorAsPopupAndCriticalLog, this, [this, gameWindow](const QString&) {
        m_loginWait->close();
        // Password login failed before a session exists: show the mask again.
        if (m_client->account().session.sessionKey.isEmpty())
            gameWindow->setAuthenticated(false);
    });
}

void TSessionBootstrap::closeConnectingDialog()
{
    m_loginWait->close();
}

void TSessionBootstrap::openCreateAccount()
{
    m_client->onRequestCreateAccount();
}

void TSessionBootstrap::promptKeepGraphicsEngine()
{
    if (!graphicsrestart::pendingConfirm())
        return;
    const int renderer = ClientOptions::load().rendererIndex();
    m_keepRenderer->showMessage(qtTrId("info"),
                                qtTrId("keep_renderer_after_restart").arg(graphicsrestart::rendererDisplayName(renderer)));
}

void TSessionBootstrap::openCreateCharacter(const bool accountHasNoCharacters)
{
    createCharacter()->setSessionKey(m_client->account().session.sessionKey);
    createCharacter()->open(accountHasNoCharacters);
}

}
