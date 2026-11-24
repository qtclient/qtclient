// The game client's game window: the start screen, the map, sidebars, chat,
// bars and every dialog controller of a game session, created once the
// client window exists (CIP: TGameClient creates them on its window-setup
// timer and keeps them as its members).
#include "client/gameclient.h"
#include "client/clientciptypes.h"
#include "game/gameactionrouter.h"
#include "game/gamesessionbase.h"
#include "game/sessionhooks.h"

#include "analysers/analyserstorages.h"
#include "analysers/analyserwidgets.h"
#include "analysers/itempricestorage.h"
#include "app/clientoptions.h"
#include "app/sessionbootstrap.h"
#include "bossbestiary/bosstrackerstorage.h"
#include "client/gamecore.h"
#include "config/characterconfigurationmanager.h"
#include "config/optionspages.h"
#include "controllers/characterselectioncontroller.h"
#include "cyclopedia/creaturetrackerstorage.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "game/gameciptypes.h"
#include "qmlcomponents/lenshelpmanager.h"
#include "exivaoptions/exivaoptionsdialogcontroller.h"
#include "game/gameservergamesession.h"
#include "game/sessiondumpgamesession.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"
#include "game/tgamenotify.h"
#include "gamewindow/actionbarmanager.h"
#include "gamewindow/battlelistcontroller.h"
#include "gamewindow/blessingsdialogcontroller.h"
#include "gamewindow/bosstrackerwidgetcontroller.h"
#include "gamewindow/bugreportdialogcontroller.h"
#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/chatcontroller.h"
#include "gamewindow/cipsoftuieventbridge.h"
#include "gamewindow/containerstoragecontroller.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/cooldownbarcontroller.h"
#include "gamewindow/createaccountdialogcontroller.h"
#include "gamewindow/createcharacterdialogcontroller.h"
#include "gamewindow/cyclopediadialogcontroller.h"
#include "gamewindow/cyclopediamapdialogcontroller.h"
#include "cyclopedia/cyclopediamapcharacterconfigurationstorage.h"
#include "quickloot/quicklootblackwhiteliststorage.h"
#include "gamewindow/creaturetrackerwidgetcontroller.h"
#include "gamewindow/deathdialogcontroller.h"
#include "gamewindow/depotsearchwidgetcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/draganddropcontroller.h"
#include "gamewindow/exaltationforgedialogcontroller.h"
#include "gamewindow/fpslatencyindicatorcontroller.h"
#include "gamewindow/gamesessiondisconnectreactioncontroller.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/highscoresdialogcontroller.h"
#include "chat/gamemessagehandler.h"
#include "renderer/worldmaprenderprovider.h"
#include "sound/soundcontroller.h"
#include "worldmap/nextframetimestamppredictor.h"
#include "worldmap/worldmapviewport.h"
#include "gamewindow/imbuementtrackerwidgetcontroller.h"
#include "gamewindow/imbuingdialogcontroller.h"
#include "gamewindow/informationdialogs.h"
#include "gamewindow/killtrackerwidgetcontroller.h"
#include "gamewindow/loottrackingwidgetcontroller.h"
#include "gamewindow/managelootcontainerdialogcontroller.h"
#include "gamewindow/mapwindowcontroller.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/minimapcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/newsdialogcontroller.h"
#include "news/newsciptypes.h"
#include "gamewindow/naggingmessagedialogcontroller.h"
#include "gamewindow/npcdialogcontroller.h"
#include "gamewindow/optionsmenudialogcontroller.h"
#include "gamewindow/playerguidewidgetcontroller.h"
#include "gamewindow/playerinventoryandstatuscontroller.h"
#include "gamewindow/playertradecontroller.h"
#include "gamewindow/preydialogcontroller.h"
#include "gamewindow/premiumpanelcontroller.h"
#include "gamewindow/questlogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/configurecreaturepodiumdialogcontroller.h"
#include "gamewindow/editminimapmarkerdialogcontroller.h"
#include "gamewindow/outfitdialogcontroller.h"
#include "gamewindow/vocationselectiondialogcontroller.h"
#include "gamewindow/whiteblacklistdialogcontroller.h"
#include "minimap/minimapmarkerstorage.h"
#include "gamewindow/edittextcontroller.h"
#include "gamewindow/reportviolationcontroller.h"
#include "gamewindow/rewardwalldialogcontroller.h"
#include "gamewindow/selectobjectamountdialogcontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "gamewindow/sidebarpanelsmanager.h"
#include "gamewindow/sidebarwidgetsmanager.h"
#include "gamewindow/spelllistwidgetcontroller.h"
#include "gamewindow/stashdialogcontroller.h"
#include "gamewindow/statusbarcontroller.h"
#include "gamewindow/storeandresourcebalancehelpercontroller.h"
#include "gamewindow/storedialogcontroller.h"
#include "gamewindow/taskboarddialogcontroller.h"
#include "gamewindow/bossdifficultyselectiondialogcontroller.h"
#include "gamewindow/tutorialhintdialogcontroller.h"
#include "gamewindow/tutorialoverlaycontroller.h"
#include "gamewindow/unjustifiedpointswidgetcontroller.h"
#include "gamewindow/vipwidgetcontroller.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/domainhandlers.h"
#include "input/eventfiltermanager.h"
#include "input/eventfilters.h"
#include "input/gameactionhandlers.h"
#include "input/genericgameactionhandler.h"
#include "input/keyboardshortcuthandler.h"
#include "input/keyboardstatusstorage.h"
#include "input/keyeventhandler.h"
#include "input/playermovementgameactionhandler.h"
#include "input/playermovementinputinterpreter.h"
#include "input/recordkeyboardshortcuthandler.h"
#include "minimap/minimapmanager.h"
#include "mods/pack_host_abi.h"
#include "mods/registry.h"
#include "proficiency/proficiencystorage.h"
#include "proficiency/weaponproficiencydialogcontroller.h"
#include "proficiency/weaponproficiencyshapedialogs.h"
#include "qmlcomponents/soundhelper.h"
#include "screenshot/screenshotciptypes.h"
#include "skillwheel/skillwheelmanager.h"
#include "store/dynamicallyloadedimage.h"

#include "clockservice.h"

#include <array>
#include <QGuiApplication>
#include <qtclient/pack_host.h>
#include <tibia/game/gamesession.h>

#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include "gamewindow/gamewindowciptypes.h"
#include "input/gameaction.h"
#include "input/gameactiondispatch.h"
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>

#include <exception>

namespace tibia {
namespace client {

/// The game window's objects, declared in construction order (destroyed in
/// reverse: the pack wiring first, the out-of-date dialog last).
struct TGameClient::TGameWindow
{
    app::LauncherEnv env;
    /// The game actions the client handles itself while the game window exists
    /// (store, screenshots, offline training, default minimap tiles).
    input::TGameActionDispatch gameActions;
    /// The keyboard input and the application event filters (CipSoft creates
    /// them with the client, before the game window).
    std::shared_ptr<input::TKeyboardStatusStorage> keyboardStatus;
    std::shared_ptr<input::TKeyEventHandler> keyEventHandler;
    std::shared_ptr<input::TKeyboardShortcutHandler> keyboardShortcuts;
    std::shared_ptr<input::TRecordKeyboardShortcutHandler> recordKeyboardShortcut;
    std::unique_ptr<input::TEventFilterManager> eventFilterManager;
    std::unique_ptr<input::TPlayerMovementInputInterpreter> playerMovement;
    std::unique_ptr<gamewindow::TMessageDialogController> clientOutOfDateDialog;
    /// The exit confirmation (Exit / Logout / Cancel, or Exit / Cancel in the tutorial).
    std::unique_ptr<gamewindow::TMessageDialogController> exitGameDialog;
    std::shared_ptr<gamewindow::TGameWindowController> gameWindowController;
    std::unique_ptr<gamewindow::TStatusBarController> statusBarController;
    std::unique_ptr<QObject> gameWindow;
    QQuickItem* gameWindowItem = nullptr;
    QQuickItem* dialogPlaceholder = nullptr;
    QQuickItem* mapWindowPane = nullptr;
    std::unique_ptr<gamewindow::TSidebarManager> sidebarManager;
    std::unique_ptr<QQmlComponent> dialogComponent;
    std::unique_ptr<QObject> dialog;
    QQuickItem* dialogItem = nullptr;
    std::unique_ptr<QObject> productHost;
    /// Child of the map window pane.
    gamewindow::TMapWindowController* mapWindowController = nullptr;
    std::unique_ptr<gamewindow::TContextMenuController> contextMenu;
    std::unique_ptr<gamewindow::TDialogStackController> dialogStack;
    std::unique_ptr<app::TSessionBootstrap> sessionBootstrap;
    std::unique_ptr<analysers::TAnalyserStorages> analyserStorages;
    std::unique_ptr<gamewindow::TInformationDialogs> informationDialogs;
    std::unique_ptr<gamewindow::TDragAndDropController> dragAndDrop;
    /// CIP: the game window controller's shared image downloader (store, social, news, …).
    std::shared_ptr<store::TDynamicallyLoadedImageManager> dynamicallyLoadedImageManager;
    std::unique_ptr<gamewindow::TStoreButtonController> storeButton;
    std::unique_ptr<gamewindow::TStashDialogController> stashDialog;
    std::unique_ptr<gamewindow::TManageLootContainerDialogController> manageLootDialog;
    std::unique_ptr<gamewindow::TImbuingDialogController> imbuingDialog;
    std::unique_ptr<skillwheel::TSkillWheelManager> skillWheel;
    std::unique_ptr<screenshot::TScreenshotManager> screenshotManager;
    std::unique_ptr<gamewindow::TSidebarPanelsManager> sidebarPanels;
    std::unique_ptr<gamewindow::TSidebarWidgetsManager> sidebarWidgets;
    gamewindow::TMinimapController* minimapController = nullptr;
    std::unique_ptr<gamewindow::TContainerStorageController> containers;
    std::unique_ptr<exivaoptions::TExivaOptionsDialogController> exivaOptionsDialog;
    std::unique_ptr<gamewindow::TCipSoftUiEventBridge> cipSoftUiEventBridge;
    std::unique_ptr<analysers::TAnalyserWidgets> analyserWidgets;
    std::unique_ptr<gamewindow::TTaskboardDialogController> taskboardDialog;
    std::unique_ptr<gamewindow::TTutorialShell> tutorialShell;
    std::unique_ptr<gamewindow::TTutorialOverlayController> tutorialOverlay;
    std::unique_ptr<gamewindow::TOptionsMenuDialogController> optionsDialog;
    std::unique_ptr<input::TOpenWebsiteGameActionHandler> openWebsiteActions;
    std::unique_ptr<gamewindow::TFPSLatencyIndicatorController> fpsLatency;
    std::unique_ptr<gamewindow::TChatController> chat;
    std::unique_ptr<gamewindow::TActionBarManager> actionBars;
    std::unique_ptr<gamewindow::TTargetSelectionController> targetSelection;
    std::unique_ptr<gamewindow::TCooldownBarController> cooldownBar;
    std::unique_ptr<input::TPlayerMovementGameActionHandler> playerMovementActions;
    std::unique_ptr<input::TClipboardGameActionHandler> clipboardActions;
    std::unique_ptr<gamewindow::TNpcDialogController> npcDialog;
    /// Outlives the packs' bootstrap: their deferred slots keep pointers into it.
    std::unique_ptr<qtclient::PackHostWire> packHost;
};

void TGameClient::TGameWindowDeleter::operator()(TGameWindow* gameWindow) const { delete gameWindow; }

void TGameClient::beforeRenderFrameGameWindow()
{
    if (!m_gameWindow)
        return;
    const qint64 timestampMilliseconds = shared::TClockService::frameMillis();
    if (m_gameWindow->sidebarPanels)
        m_gameWindow->sidebarPanels->updateMouseCursorForCurrentSlot();
    if (m_gameWindow->sidebarWidgets)
        m_gameWindow->sidebarWidgets->updateWithTimestamp(timestampMilliseconds);
    if (m_gameWindow->sidebarPanels)
        m_gameWindow->sidebarPanels->refreshClocks();
    if (m_pDeathDialogController)
        m_pDeathDialogController->onBeforeRenderFrame();
    if (m_gameWindow->actionBars)
        m_gameWindow->actionBars->tick(game::TCooldownStorage::now());
}

void TGameClient::requestLogoutOrShowStartScreen(gamewindow::TGameWindowController* startScreen)
{
    if (game::TGameserverGameSession* gameSession = session())
        gameSession->requestLogout();
    else if (game::TSessiondumpGameSession* dump = sessiondump())
        dump->requestLogout();
    else if (startScreen)
        startScreen->showStartScreen();
}

void TGameClient::setSidebarTriggerCheckBoundsEnabled(const bool Enabled)
{
    if (m_gameWindow && m_gameWindow->sidebarManager)
        m_gameWindow->sidebarManager->setTriggerCheckBoundsEnabled(Enabled);
}

void TGameClient::exitClient()
{
    gamewindow::TMessageDialogController* const exitGameDialog =
        m_gameWindow ? m_gameWindow->exitGameDialog.get() : nullptr;
    if (!m_currentGameSession || !exitGameDialog) {
        // The client state is saved, the session ends and the application closes.
        shutdown();
        Q_EMIT clientReadyToClose();
        return;
    }
    using Button = gamewindow::TMessageDialogController::Button;
    using gamewindow::TMessageDialogButton;
    const game::TSessiondumpGameSession* const dump = sessiondump();
    if (dump && dump->isTutorial()) {
        exitGameDialog->setButtons({
            new TMessageDialogButton(Button::OK, qtTrId("quit_game_exit_button"), TMessageDialogButton::NO_KEYS),
            new TMessageDialogButton(Button::CANCEL, qtTrId("cancel"), TMessageDialogButton::ESCAPE_KEY),
        });
        exitGameDialog->showMessage(qtTrId("warning"), qtTrId("quit_game_end_tutorial_warning"));
        return;
    }
    exitGameDialog->setButtons({
        new TMessageDialogButton(Button::OK, qtTrId("quit_game_exit_button"), QKeySequence(QStringLiteral("E"))),
        new TMessageDialogButton(Button::USER_DEFINED, qtTrId("quit_game_logout_button"),
                                 TMessageDialogButton::RETURN_KEYS),
        new TMessageDialogButton(Button::CANCEL, qtTrId("cancel"), TMessageDialogButton::ESCAPE_KEY),
    });
    exitGameDialog->showMessage(qtTrId("warning"), qtTrId("quit_game_logout_warning"));
}

void TGameClient::onExitClientRequested()
{
    exitClient();
}

void TGameClient::onRequestCreateAccount()
{
    m_pCreateAccountDialogController->showDialog();
}

void TGameClient::onRequestCreateCharacter()
{
    m_pCreateCharacterDialogController->showDialog();
}

void TGameClient::setCreateCharacterDialogController(gamewindow::TCreateCharacterDialogController* Controller)
{
    if (!Controller || Controller == m_pCreateCharacterDialogController.get())
        return;
    if (m_pCreateCharacterDialogController)
        m_pCreateCharacterDialogController.release()->deleteLater();
    Controller->setParent(nullptr);
    m_pCreateCharacterDialogController.reset(Controller);
}

void TGameClient::onConfirmOrCancelExitGame(
    const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier)
{
    using Button = gamewindow::TMessageDialogController::Button;
    m_gameWindow->exitGameDialog->dialog()->close();
    if (ButtonIdentifier == Button::USER_DEFINED) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionQuitGame>(true));
    } else if (ButtonIdentifier == Button::OK) {
        shutdown();
        Q_EMIT clientReadyToClose();
    }
}

bool TGameClient::createGameWindow(const TGameWindowSetup& setup)
{
    m_gameWindow.reset(new TGameWindow);
    TGameWindow& g = *m_gameWindow;
    g.env = setup.env;
    QQmlEngine* engine = setup.engine;
    QQuickWindow* window = setup.window;
    const bool preview = setup.preview;
    // CIP: the engine the client is built with; the game window is shown in
    // the client window.
    m_pEngine = engine;
    m_options = setup.options;
    if (preview) {
        // The preview runs no engine: the window is only kept and the QML
        // context gets the client's objects.
        m_pMainWindow = window;
        initializeQmlContext();
    } else {
        // CIP: the application runs the client's setup steps and hands it the
        // client window before the game window is shown.
        if (!startEngine(setup.argv0, setup.workDir))
            return false;
        initialize();
        setMainWindow(window);
    }
    QCoreApplication* application = QCoreApplication::instance();
    // CIP: the authentication process controller is made with the game window.
    createAuthenticationProcessController(g.env.loginUrl());
    // The preview shows its own character list.
    gamewindow::TCharacterSelectionController* const characterSelectionController =
        preview ? setup.characterSelection : characterSelection();

    g.keyboardStatus = std::make_shared<input::TKeyboardStatusStorage>();
    if (auto* guiApplication = qobject_cast<QGuiApplication*>(application))
        connect(guiApplication, &QGuiApplication::applicationStateChanged, g.keyboardStatus.get(),
                &input::TKeyboardStatusStorage::onApplicationStateChanged);
    g.keyEventHandler = std::make_shared<input::TKeyEventHandler>(g.keyboardStatus);
    const auto chatMode = [&g] { return g.chat ? g.chat->chatMode() : 0; };
    g.keyboardShortcuts = std::make_shared<input::TKeyboardShortcutHandler>(chatMode, std::make_shared<shared::TClockService>());
    g.recordKeyboardShortcut = std::make_shared<input::TRecordKeyboardShortcutHandler>();
    g.eventFilterManager = std::make_unique<input::TEventFilterManager>(g.keyboardShortcuts, g.keyEventHandler, g.recordKeyboardShortcut);
    g.playerMovement = std::make_unique<input::TPlayerMovementInputInterpreter>(g.keyboardStatus, chatMode);

    // ShowClientOutOfDate: caption "info", body client_too_old, single Restart
    // (YES/Return). Restart writes QTCLIENT_RESTART_FILE so the launcher updates.
    using OutOfDateBox = gamewindow::TMessageDialogController;
    g.clientOutOfDateDialog = std::make_unique<OutOfDateBox>();
    OutOfDateBox* outOfDate = g.clientOutOfDateDialog.get();
    outOfDate->setCloseOnButtonClicked(true);
    outOfDate->setButtons({ new gamewindow::TMessageDialogButton(qtTrId("restart"), OutOfDateBox::Button::YES,
                                                                 QKeySequence(QStringLiteral("Return"))) });
    connect(this, &TGameClient::clientOutOfDate, outOfDate, [outOfDate] {
        if (!outOfDate->dialog()->isInStack())
            outOfDate->showMessage(qtTrId("info"), qtTrId("client_too_old"));
    });
    // Restart: the launcher updates the client (QTCLIENT_RESTART_FILE) once it quit.
    const auto quitAndPatchClient = [] {
        const QString marker = qEnvironmentVariable("QTCLIENT_RESTART_FILE");
        if (!marker.isEmpty()) {
            QSaveFile file(marker);
            if (file.open(QIODevice::WriteOnly) && file.write("1") == 1)
                file.commit();
        }
        QCoreApplication::quit();
    };
    connect(outOfDate, &OutOfDateBox::acceptClicked, application, quitAndPatchClient);

    // The start screen until the world is entered, then the map, sidebars,
    // chat and bars. Dialogs go into its dialogPlaceholder.
    g.gameWindowController = std::make_shared<gamewindow::TGameWindowController>(setup.placeholder);
    gamewindow::TGameWindowController* gameWindowController = g.gameWindowController.get();
    // kill / SIGTERM / Ctrl+C: same Exit / Logout / Cancel prompt as the window close.
    gamecore::setForceExitHandler([gameWindowController] { gameWindowController->quitPressed(); });
    g.statusBarController = std::make_unique<gamewindow::TStatusBarController>();
    gameWindowController->setStatusBarController(g.statusBarController.get());
    // The game window's Quit asks the client to exit; Manage Clients opens the launcher.
    connect(gameWindowController, &gamewindow::TGameWindowController::gameExitRequested, this,
            &TGameClient::onExitClientRequested);
    connect(gameWindowController, &gamewindow::TGameWindowController::openLauncherRequested, gameWindowController,
            [] { app::LauncherEnv::showLauncher(); });
    // CIP: the exit confirmation; its buttons answer onConfirmOrCancelExitGame.
    g.exitGameDialog = std::make_unique<gamewindow::TMessageDialogController>();
    connect(g.exitGameDialog.get(), &gamewindow::TMessageDialogController::buttonClicked, this,
            &TGameClient::onConfirmOrCancelExitGame);
    g.gameWindow = gameWindowController->createGameWindow(engine);
    g.gameWindowItem = qobject_cast<QQuickItem*>(g.gameWindow.get());
    QQuickItem* gameWindowItem = g.gameWindowItem;
    if (!gameWindowItem)
        return false;
    // Lenshelp mode and its popup over the game window.
    m_pLenshelpManager = std::make_unique<qmlcomponents::TLenshelpManager>(engine, gameWindowItem);
    gameWindowController->configureClientServices(g.env.clientServicesUrl(), g.env.revision);
    // The game screen turns the keyboard shortcuts and walking keys on, the start screen off.
    connect(gameWindowController, &gamewindow::TGameWindowController::gameScreenNowDisplayed, g.eventFilterManager.get(), [&g] {
        g.eventFilterManager->enableKeyboardShortcuts();
        g.eventFilterManager->enableMovementKeys();
    });
    connect(gameWindowController, &gamewindow::TGameWindowController::startScreenNowDisplayed, g.eventFilterManager.get(), [&g] {
        g.eventFilterManager->disableKeyboardShortcuts();
        g.eventFilterManager->disableMovementKeys();
    });
    gameWindowController->showStartScreen();
    g.dialogPlaceholder = gameWindowController->gameWindowChild(QStringLiteral("dialogPlaceholder"));
    g.mapWindowPane = gameWindowController->gameWindowChild(QStringLiteral("mapWindowPane"));
    QQuickItem* mapWindowPane = g.mapWindowPane;
    g.sidebarManager = std::make_unique<gamewindow::TSidebarManager>(
        gameWindowController->gameWindowChild(QStringLiteral("rightSidebarPlaceholder")),
        gameWindowController->gameWindowChild(QStringLiteral("rightSidebarManager")),
        gameWindowController->gameWindowChild(QStringLiteral("leftSidebarPlaceholder")),
        gameWindowController->gameWindowChild(QStringLiteral("leftSidebarManager")));
    gamewindow::TSidebarManager* sidebarManager = g.sidebarManager.get();
    // CIP: the application hears the sidebar count through the client.
    connect(sidebarManager, &gamewindow::TSidebarManager::numberOfSidebarsChanged, this,
            &TGameClient::numberOfSidebarsChanged);
    // Host: the manager already emitted before this connection, and the split
    // view lays out again once the minimum width changed.
    const auto correctLayout = [window] {
        if (QObject* split = window->findChild<QObject*>(QStringLiteral("mainFrame")))
            QTimer::singleShot(0, split, [split] { QMetaObject::invokeMethod(split, "correctLayout"); });
    };
    Q_EMIT numberOfSidebarsChanged(sidebarManager->sidebarIds().size());
    correctLayout();
    connect(sidebarManager, &gamewindow::TSidebarManager::numberOfSidebarsChanged, window, correctLayout);

    g.dialogComponent = std::make_unique<QQmlComponent>(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/CharacterSelection.qml")));
    g.dialog.reset(g.dialogComponent->beginCreate(engine->rootContext()));
    g.dialogItem = qobject_cast<QQuickItem*>(g.dialog.get());
    QQuickItem* dialogItem = g.dialogItem;
    if (!dialogItem) {
        return false;
    }
    g.dialogComponent->setInitialProperties(
        dialogItem, { { QStringLiteral("controller"), QVariant::fromValue<QObject*>(characterSelectionController) } });
    dialogItem->setParentItem(g.dialogPlaceholder);

    g.productHost = std::make_unique<QObject>();
    {
        mods::ModContext earlyCtx;
        earlyCtx.engine = engine;
        earlyCtx.rootItem = window->contentItem();
        earlyCtx.gameClient = this;
        earlyCtx.gameHost = gameHost();
        earlyCtx.host = g.productHost.get();
        earlyCtx.launcherEnv = &g.env;
        mods::earlyBootstrapAll(earlyCtx);
    }
    g.dialogComponent->completeCreate();
    dialogItem->setVisible(preview);

    if (!preview) {
        connect(this, &TGameClient::characterSelectionRequested, dialogItem, [dialogItem, gameWindowController] {
            gameWindowController->setAuthenticated(true);
            dialogItem->setVisible(true);
            if (game::isOnline()) {
                if (auto* stack = gamewindow::TDialogStackController::instance())
                    stack->setCharacterListOpen(true);
            }
            // initialFocusItem is the character table: Up/Down move the row, Return confirms.
            QTimer::singleShot(0, dialogItem, [dialogItem] {
                QObject* focus = dialogItem->property("initialFocusItem").value<QObject*>();
                if (auto* item = qobject_cast<QQuickItem*>(focus))
                    item->forceActiveFocus(Qt::PopupFocusReason);
                else
                    dialogItem->forceActiveFocus(Qt::PopupFocusReason);
            });
        });
        connect(this, &TGameClient::characterSelectionClosed, dialogItem, [dialogItem] {
            dialogItem->setVisible(false);
            if (auto* stack = gamewindow::TDialogStackController::instance())
                stack->setCharacterListOpen(false);
        });
        connect(this, &TGameClient::returnToLoginRequested, gameWindowController,
                [gameWindowController] { gameWindowController->setAuthenticated(false); });
        // Start screen with the account still "authenticated" hides the login mask.
        // If the character list is also closed (dropped switch, refused reconnect),
        // put one of those surfaces back. Deferred so a logout that opens the list
        // in the same turn is left alone.
        connect(gameWindowController, &gamewindow::TGameWindowController::gameWindowStateChanged, dialogItem,
                [this, dialogItem, gameWindowController] {
                    if (!gameWindowController->property("gameWindowState").toString().isEmpty())
                        return;
                    QTimer::singleShot(0, dialogItem, [this, dialogItem, gameWindowController] {
                        if (game::isOnline() || game::isSwitchingCharacter() || dialogItem->isVisible())
                            return;
                        if (!gameWindowController->property("isAuthenticated").toBool())
                            return;
                        ensureLoginSurface();
                    });
                });
        connect(gameWindowController, &gamewindow::TGameWindowController::loginRequested, this,
                &TGameClient::submitCredentials);
        // CLI credentials / session replay: hide the start-screen login mask.
        if (!setup.options.account.isEmpty() || !setup.options.replay.isEmpty())
            gameWindowController->setAuthenticated(true);
        // start runs after the dialog stack + bootstrap so login errors can show.
    } else {
        gameWindowController->setAuthenticated(true);
    }
    const bool softwareRenderer = window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
    g.mapWindowController = new gamewindow::TMapWindowController(mapWindowPane, softwareRenderer, mapWindowPane);
    gamewindow::TMapWindowController* mapWindowController = g.mapWindowController;
    mapWindowController->syncFov();
    connect(mapWindowController, &gamewindow::TMapWindowController::unusedWidthChanged, sidebarManager,
            &gamewindow::TSidebarManager::onUnusedWidthChanged);
    mapWindowController->onUnusedWidthChanged();

    g.contextMenu = std::make_unique<gamewindow::TContextMenuController>(engine, window->contentItem());
    g.dialogStack = std::make_unique<gamewindow::TDialogStackController>(engine, g.dialogPlaceholder);
    gamewindow::TDialogStackController* dialogStack = g.dialogStack.get();
    // A crash dump of the last run offers a crash report (the dialogs need the
    // dialog stack).
    if (!preview)
        checkForCrashDump();
    // CIP: the create-account and create-character dialogs (both with the
    // account service of the login's portal); the start screen's Create
    // Account opens the first.
    m_pCreateAccountDialogController = std::make_unique<gamewindow::TCreateAccountDialogController>();
    m_pCreateAccountDialogController->setServiceUrl(g.env.createAccountUrl());
    m_pCreateAccountDialogController->setTooltipContainer(g.dialogPlaceholder);
    m_pCreateCharacterDialogController = std::make_unique<gamewindow::TCreateCharacterDialogController>();
    m_pCreateCharacterDialogController->setServiceUrl(g.env.createAccountUrl());
    connect(gameWindowController, &gamewindow::TGameWindowController::createAccountRequested, this,
            &TGameClient::onRequestCreateAccount);
    // Session chrome (WaitDialog, errors, create account/character, keep_renderer).
    // Must run after the dialog stack so MessageDialog / WaitDialog can show.
    if (!preview)
        g.sessionBootstrap = std::make_unique<app::TSessionBootstrap>(this, g.env, gameWindowController, g.productHost.get());
    // Before the first session: it reacts to the session's connection and login.
    m_pGameSessionDisconnectReactionController =
        std::make_shared<gamewindow::TGameSessionDisconnectReactionController>(m_authentication, g.gameWindowController);
    connect(m_pGameSessionDisconnectReactionController.get(),
            &gamewindow::TGameSessionDisconnectReactionController::loginAgainAfterLoginWaitElapsed, this,
            &TGameClient::loginAgainWithLastCharacter, Qt::QueuedConnection);
    connect(m_pGameSessionDisconnectReactionController.get(), &gamewindow::TGameSessionDisconnectReactionController::quitAndPatchClient,
            application, quitAndPatchClient);
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::quitAndPatchClient, application,
            quitAndPatchClient);
    connect(gameActionRouter()->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal,
            m_pGameSessionDisconnectReactionController.get(), &gamewindow::TGameSessionDisconnectReactionController::handleGameAction);
    connect(gameActionRouter()->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal,
            m_authentication.get(), &authentication::TAuthenticationProcessController::handleGameAction);
    connect(this, &TGameClient::sessionEmailAvailable, gameWindowController,
            &gamewindow::TGameWindowController::rememberLoginEmailIfEnabled);
    if (!preview)
        start();
    // TEventFilterManager emits the chat-input focus signals while a dialog is up, so dialog inputs get the focus.
    connect(dialogStack, &gamewindow::TDialogStackController::dialogVisible, gameWindowController,
            [gameWindowController](bool visible) {
                if (visible)
                    Q_EMIT gameWindowController->releaseFocusFromChatInput();
                else
                    Q_EMIT gameWindowController->bindFocusToChatInput();
            });
    // An element asking for the keyboard takes it from the chat input; letting go gives it back.
    connect(g.eventFilterManager.get(), &input::TEventFilterManager::requestElementFocus, gameWindowController,
            &gamewindow::TGameWindowController::releaseFocusFromChatInput);
    connect(g.eventFilterManager.get(), &input::TEventFilterManager::releaseElementFocus, gameWindowController,
            &gamewindow::TGameWindowController::bindFocusToChatInput);
    // Before the dialogs: the Cyclopedia item page and the loot widgets read its item storages.
    g.analyserStorages = std::make_unique<analysers::TAnalyserStorages>();
    g.informationDialogs = std::make_unique<gamewindow::TInformationDialogs>();
    gamewindow::TInformationDialogs* informationDialogs = g.informationDialogs.get();
    g.dragAndDrop = std::make_unique<gamewindow::TDragAndDropController>(
        window->contentItem(), gameWindowController->gameWindowChild(QStringLiteral("dragApperanceInstanceIcon")));
    g.dragAndDrop->registerContextProperty(engine->rootContext());
    g.dynamicallyLoadedImageManager = std::make_shared<store::TDynamicallyLoadedImageManager>();
    // CIP: the client's dialog controllers, in showGameWindow's order (each
    // with the engine and the game window controller's dialog parent item).
    QQuickItem* const dialogParentItem = g.dialogPlaceholder;
    m_pMarketController = std::make_unique<gamewindow::TMarketController>(clientOptions(), engine, dialogParentItem);
    m_pRewardWallDialogController =
        std::make_unique<gamewindow::TRewardWallDialogController>(shared::TClockService::process(), engine, dialogParentItem);
    m_pExaltationForgeDialogController =
        std::make_unique<gamewindow::TExaltationForgeDialogController>(clientOptions(), engine, dialogParentItem);
    m_pWeaponProficiencyDialogController =
        std::make_unique<proficiency::TWeaponProficiencyDialogController>(clientOptions(), engine, dialogParentItem);
    m_pWeaponProficiencyShapeDialogController =
        std::make_unique<proficiency::TWeaponProficiencyShapeDialogController>(engine, dialogParentItem);
    m_pWeaponProficiencyShapeOptionsDialogController =
        std::make_unique<proficiency::TWeaponProficiencyShapeOptionsDialogController>(engine, dialogParentItem);
    m_pWeaponProficiencyReshapeDialogController = std::make_unique<proficiency::TWeaponProficiencyReshapeDialogController>(
        engine, dialogParentItem, m_pWeaponProficiencyShapeDialogController.get());
    m_pReportViolationController =
        std::make_unique<gamewindow::TReportViolationController>(shared::TClockService::process(), engine, dialogParentItem);
    m_pDeathDialogController = std::make_unique<gamewindow::TDeathDialogController>(engine, dialogParentItem);
    m_pStoreDialogController = std::make_unique<gamewindow::TStoreDialogController>(g.dynamicallyLoadedImageManager,
                                                                                    clientOptions(), engine, dialogParentItem);
    m_pBugReportDialogController = std::make_unique<gamewindow::TBugReportDialogController>(engine, dialogParentItem);
    m_pBlessingsDialogController = std::make_unique<gamewindow::TBlessingsDialogController>(engine, dialogParentItem);
    m_pSoulSealsDialogController = std::make_unique<gamewindow::TSoulSealsDialogController>(engine, dialogParentItem);
    m_pBossDifficultySelectionDialogController =
        std::make_unique<gamewindow::TBossDifficultySelectionDialogController>(engine, dialogParentItem);
    m_pSelectObjectAmountDialogController =
        std::make_unique<gamewindow::TSelectObjectAmountDialogController>(engine, dialogParentItem);
    m_pNaggingMessageDialogController = std::make_unique<gamewindow::TNaggingMessageDialogController>(engine, dialogParentItem);
    gamewindow::TStoreDialogController* storeDialog = m_pStoreDialogController.get();
    g.storeButton = std::make_unique<gamewindow::TStoreButtonController>(storeDialog);
    g.stashDialog = std::make_unique<gamewindow::TStashDialogController>();
    connect(m_pMarketController.get(), &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed, g.stashDialog.get(),
            &gamewindow::TStashDialogController::onMarketDialogDialogClosed);
    g.manageLootDialog = std::make_unique<gamewindow::TManageLootContainerDialogController>();
    connect(g.manageLootDialog.get(), &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed, g.stashDialog.get(),
            &gamewindow::TStashDialogController::onManageLootContainerDialogClosed);
    g.imbuingDialog = std::make_unique<gamewindow::TImbuingDialogController>();
    g.skillWheel = std::make_unique<skillwheel::TSkillWheelManager>();
    g.screenshotManager = std::make_unique<screenshot::TScreenshotManager>();
    g.screenshotManager->configure(window, mapWindowPane);
    screenshot::TScreenshotManager* screenshotManager = g.screenshotManager.get();
    const auto takeScreenshot = [screenshotManager](const input::IGameAction& action) {
        return screenshotManager->handleGameAction(action);
    };
    g.gameActions.on(input::EGameAction::TAKE_MANUAL_SCREENSHOT, takeScreenshot);
    g.gameActions.on(input::EGameAction::TAKE_AUTOMATIC_SCREENSHOT, takeScreenshot);
    g.gameActions.on(input::EGameAction::SHOW_OFFLINE_TRAINING_SKILLS, [](const input::IGameAction&) {
        if (auto* offline = gamewindow::TMultiOfflineTrainingDialogController::instance())
            offline->showSkillsDialog();
        return true;
    });
    qmlcomponents::followSoundOptions();

    g.sidebarPanels = std::make_unique<gamewindow::TSidebarPanelsManager>(sidebarManager, engine);
    gamewindow::TSidebarPanelsManager* sidebarPanels = g.sidebarPanels.get();
    g.sidebarWidgets = std::make_unique<gamewindow::TSidebarWidgetsManager>(
        sidebarManager, gameWindowItem, engine, g.analyserStorages.get());
    gamewindow::TSidebarWidgetsManager* sidebarWidgets = g.sidebarWidgets.get();
    const QJsonObject storedOptions = app::ClientOptions::load().options();
    const int controlScheme = storedOptions.value(QLatin1String("controlSchemeIndex")).toInt(0);
    const int lootScheme = storedOptions.value(QLatin1String("lootSchemeIndex")).toInt(0);
    g.minimapController = sidebarPanels->minimap();
    if (g.minimapController)
        g.minimapController->setControlScheme(controlScheme);
    if (gamewindow::TPlayerInventoryAndStatusController* inventory = sidebarPanels->inventory()) {
        inventory->setControlScheme(controlScheme);
        inventory->setLootScheme(lootScheme);
    }
    g.containers = std::make_unique<gamewindow::TContainerStorageController>(sidebarManager, gameWindowItem, engine);
    gamewindow::TContainerStorageController* containers = g.containers.get();
    containers->setControlScheme(controlScheme);
    containers->setLootScheme(lootScheme);
    sidebarWidgets->setContainerStorage(containers);
    sidebarWidgets->setControlScheme(controlScheme);
    informationDialogs->setControlScheme(controlScheme);
    g.exivaOptionsDialog = std::make_unique<exivaoptions::TExivaOptionsDialogController>();
    g.exivaOptionsDialog->setWorldType([this] { return worldType(); });
    // Subscribers for gem atelier / helpers / RV channel / virtues yellow border.
    g.cipSoftUiEventBridge = std::make_unique<gamewindow::TCipSoftUiEventBridge>();
    g.analyserWidgets = std::make_unique<analysers::TAnalyserWidgets>(g.analyserStorages.get(), sidebarWidgets, gameWindowItem, engine,
                                                                      [this] { return sidebarOptions(); });
    g.taskboardDialog = std::make_unique<gamewindow::TTaskboardDialogController>();
    g.tutorialShell = std::make_unique<gamewindow::TTutorialShell>(engine, g.dialogPlaceholder, mapWindowPane,
                                                                   cyclopediaMapStaticDataStorage());
    connect(g.tutorialShell.get(), &gamewindow::TTutorialShell::tutorialFinished, this, &TGameClient::finishTutorialCinematic);
    g.tutorialOverlay = std::make_unique<gamewindow::TTutorialOverlayController>(engine, g.dialogPlaceholder, setup.placeholder,
                                                                                 g.eventFilterManager.get());
    if (window)
        connect(window, &QQuickWindow::afterAnimating, g.tutorialOverlay.get(),
                &gamewindow::TTutorialOverlayController::update);
    sidebarPanels->connectToSidebarWidgetsManager(sidebarWidgets);
    if (gamewindow::TButtonBarController* buttonBar = sidebarPanels->buttonBar()) {
        buttonBar->setStoreButtonController(g.storeButton.get());
        buttonBar->setNewsButtonController(gameWindowController->newsButton());
        buttonBar->setCyclopediaButtonController(informationDialogs->cyclopediaButton());
        buttonBar->setBosstiaryButtonController(informationDialogs->bosstiaryButton());
        if (QQuickItem* buttonBarItem = sidebarPanels->buttonBarItem()) {
            // storeButtonController is constant: rebinding panelController re-reads it.
            buttonBarItem->setProperty("panelController", QVariant::fromValue<QObject*>(nullptr));
            buttonBarItem->setProperty("panelController", QVariant::fromValue<QObject*>(buttonBar));
            connect(buttonBar, &gamewindow::TButtonBarController::buttonControllersChanged, buttonBarItem,
                    [buttonBarItem, buttonBar] {
                        buttonBarItem->setProperty("panelController", QVariant::fromValue<QObject*>(nullptr));
                        buttonBarItem->setProperty("panelController", QVariant::fromValue<QObject*>(buttonBar));
                    });
        }
    }
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, gameWindowItem,
            [&g](const QString& name) {
                const QJsonObject opts = app::ClientOptions::load().options();
                if (name == QLatin1String("controlSchemeIndex")) {
                    const int scheme = opts.value(name).toInt(0);
                    if (g.minimapController)
                        g.minimapController->setControlScheme(scheme);
                    if (gamewindow::TPlayerInventoryAndStatusController* inventory = g.sidebarPanels->inventory())
                        inventory->setControlScheme(scheme);
                    g.containers->setControlScheme(scheme);
                    g.sidebarWidgets->setControlScheme(scheme);
                } else if (name == QLatin1String("lootSchemeIndex")) {
                    const int scheme = opts.value(name).toInt(0);
                    if (gamewindow::TPlayerInventoryAndStatusController* inventory = g.sidebarPanels->inventory())
                        inventory->setLootScheme(scheme);
                    g.containers->setLootScheme(scheme);
                }
            });

    g.optionsDialog = std::make_unique<gamewindow::TOptionsMenuDialogController>();
    gamewindow::TOptionsMenuDialogController* optionsDialog = g.optionsDialog.get();
    optionsDialog->setWindow(window);
    optionsDialog->setFrameDurations(frameDurations());
    optionsDialog->setGameClient(this);
    optionsDialog->setPortalBase(g.env.loginBase);
    g.openWebsiteActions = std::make_unique<input::TOpenWebsiteGameActionHandler>(g.env);
    g.fpsLatency = std::make_unique<gamewindow::TFPSLatencyIndicatorController>();
    g.fpsLatency->setWindow(window);
    g.fpsLatency->setFrameDurations(frameDurations());
    g.fpsLatency->setWidgetItem(gameWindowController->gameWindowChild(QStringLiteral("tibiaFpsLatencyIndicator")));

    g.chat = std::make_unique<gamewindow::TChatController>(gameWindowItem->findChild<QQuickItem*>(QStringLiteral("chat")), *this);
    gamewindow::TChatController* chat = g.chat.get();
    setMessageIgnoreList(&chat->ignoreList());

    // The action bar manager takes the client options, the premium time
    // storage and the spell table; the three groups (bottom, left, right)
    // are made here and added to it.
    g.actionBars = std::make_unique<gamewindow::TActionBarManager>(config::TClientOptions::instance(),
                                                                   game::TPremiumTimeStorage::liveInstance(), spellInfoStorage());
    {
        const std::array<std::pair<gamewindow::EActionBarPosition, const char*>, 3> actionBarContainers{
            { { gamewindow::EActionBarPosition::ActionBarPositionBottom, "bottomActionBars" },
              { gamewindow::EActionBarPosition::ActionBarPositionLeft, "leftActionBars" },
              { gamewindow::EActionBarPosition::ActionBarPositionRight, "rightActionBars" } }
        };
        for (const auto& [position, name] : actionBarContainers) {
            QQuickItem* container = gameWindowItem ? gameWindowItem->findChild<QQuickItem*>(QLatin1String(name)) : nullptr;
            g.actionBars->addGroup(std::make_shared<gamewindow::TActionBarGroup>(position, container, engine, gameWindowItem, mapWindowPane,
                                                                                 config::TClientOptions::instance(),
                                                                                 &chat->chatModeManager()));
        }
    }
    g.targetSelection = std::make_unique<gamewindow::TTargetSelectionController>(gameWindowItem);
    {
        // Host shim: the session's character configuration manager takes the
        // storages the game window still owns.
        config::TCharacterConfigurationManager::TStorages& storages = m_characterConfigurationStorages;
        storages.itemPrice = analysers::TItemPriceStorage::instance();
        // The analysers and the Cyclopedia map configuration are the session's
        // (CipSoft hands them to the session's configuration manager).
        storages.quickLootBlackWhitelist = g.analyserStorages->lootBlackWhitelist();
        storages.skillWheel = g.skillWheel->storage();
        storages.outfitDialog = outfitDialogStorage();
    }
    g.cooldownBar = std::make_unique<gamewindow::TCooldownBarController>(
        spellInfoStorage(), gameWindowItem ? gameWindowItem->findChild<QQuickItem*>(QStringLiteral("cooldownBar")) : nullptr);
    // The bindings and walk keys follow the chat mode.
    connect(chat, &gamewindow::TChatController::chatModeChanged, g.keyboardShortcuts.get(),
            &input::TKeyboardShortcutHandler::onHotkeyOptionsChanged);
    connect(chat, &gamewindow::TChatController::chatModeChanged, g.playerMovement.get(),
            &input::TPlayerMovementInputInterpreter::onHotkeyOptionsChanged);
    g.keyboardShortcuts->onHotkeyOptionsChanged();
    g.playerMovement->onHotkeyOptionsChanged();
    g.playerMovementActions = std::make_unique<input::TPlayerMovementGameActionHandler>();
    connect(gameActionRouter()->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, g.keyboardShortcuts.get(),
            qOverload<const input::IGameActionPtr&>(&input::TKeyboardShortcutHandler::handleGameAction));
    connect(g.keyboardShortcuts.get(), &input::TKeyboardShortcutHandler::publishGameAction, gameActionRouter()->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(g.playerMovement.get(), &input::TPlayerMovementInputInterpreter::publishGameAction, gameActionRouter()->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    g.clipboardActions = std::make_unique<input::TClipboardGameActionHandler>();
    game::TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.playerMovementActions.get(),
            &input::TPlayerMovementGameActionHandler::handleGameAction);
    connect(g.playerMovementActions.get(), &input::TPlayerMovementGameActionHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.clipboardActions.get(),
            &input::TClipboardGameActionHandler::handleGameAction);
    connect(g.clipboardActions.get(), &input::TClipboardGameActionHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
    if (gamewindow::TInspectObjectDialogController* inspectObject = gamewindow::TInspectObjectDialogController::instance())
        connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, inspectObject,
                &gamewindow::TInspectObjectDialogController::handleGameAction);
    // The controllers' publishGameAction reach the router.
    const auto publishToRouter = [router](auto* publisher) {
        if (publisher)
            connect(publisher, &std::remove_pointer_t<decltype(publisher)>::publishGameAction, router,
                    &game::TInternalGameActionRouter::handleGameActionInternal);
    };
    publishToRouter(gameWindowController);
    publishToRouter(g.statusBarController.get());
    publishToRouter(g.sidebarManager.get());
    publishToRouter(g.mapWindowController);
    publishToRouter(g.contextMenu.get());
    publishToRouter(m_pSelectObjectAmountDialogController.get());
    publishToRouter(m_pStoreDialogController.get());
    publishToRouter(g.storeButton.get());
    publishToRouter(m_pMarketController.get());
    publishToRouter(g.stashDialog.get());
    publishToRouter(g.manageLootDialog.get());
    publishToRouter(m_pExaltationForgeDialogController.get());
    publishToRouter(g.screenshotManager.get());
    publishToRouter(g.minimapController);
    publishToRouter(m_pDeathDialogController.get());
    publishToRouter(m_pReportViolationController.get());
    publishToRouter(m_pWeaponProficiencyDialogController.get());
    publishToRouter(m_pWeaponProficiencyShapeDialogController.get());
    publishToRouter(m_pBlessingsDialogController.get());
    publishToRouter(m_pRewardWallDialogController.get());
    publishToRouter(g.optionsDialog.get());
    publishToRouter(g.chat.get());
    publishToRouter(g.actionBars.get());
    g.actionBars->connectToGameActionRouter(gameActionRouter());
    publishToRouter(g.targetSelection.get());
    publishToRouter(g.sidebarWidgets.get());
    publishToRouter(g.sidebarPanels.get());
    publishToRouter(g.containers.get());
    publishToRouter(g.informationDialogs.get());
    publishToRouter(g.imbuingDialog.get());
    publishToRouter(g.exivaOptionsDialog.get());
    publishToRouter(g.taskboardDialog.get());
    publishToRouter(g.skillWheel->dialog());
    publishToRouter(g.analyserStorages.get());
    publishToRouter(g.analyserWidgets.get());
    // The engine's text copy controller outlives the game windows.
    if (auto* textCopy = qobject_cast<gamewindow::TTextCopyController*>(
            engine->rootContext()->contextProperty(QStringLiteral("tibiaTextCopyController")).value<QObject*>()))
        connect(textCopy, &gamewindow::TTextCopyController::publishGameAction, router,
                &game::TInternalGameActionRouter::handleGameActionInternal, Qt::UniqueConnection);
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.openWebsiteActions.get(),
            &input::TOpenWebsiteGameActionHandler::handleGameAction);
    g.eventFilterManager->enableDiscardingUnhandledMouseButtons();

    g.npcDialog = std::make_unique<gamewindow::TNpcDialogController>();
    g.npcDialog->setChatController(chat);
    publishToRouter(g.npcDialog.get());

    // The handlers' handleGameAction slots get the router's actions (CIP
    // TClipboardHelper). The map window goes first: a battle list taking an
    // at-mouse-cursor action clears the map window's tracking of it.
    const auto handleFromRouter = [router](auto* handler) {
        if (handler)
            connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, handler,
                    qOverload<const input::IGameActionPtr&>(&std::remove_pointer_t<decltype(handler)>::handleGameAction));
    };
    handleFromRouter(g.mapWindowController);
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.productHost.get(),
            [&g](const input::IGameActionPtr& action) { g.gameActions.handle(action); });
    handleFromRouter(gameWindowController);
    handleFromRouter(g.sidebarWidgets.get());
    if (g.mapWindowController)
        connect(g.sidebarWidgets->battleLists(), &gamewindow::TBattleListControllerStorage::atMouseCursorGameActionTaken,
                g.mapWindowController, &gamewindow::TMapWindowController::clearTrackedUseAtMouseCursorGameAction);
    handleFromRouter(g.sidebarPanels.get());
    handleFromRouter(g.containers.get());
    handleFromRouter(g.contextMenu.get());
    handleFromRouter(m_pSelectObjectAmountDialogController.get());
    handleFromRouter(m_pStoreDialogController.get());
    handleFromRouter(m_pMarketController.get());
    handleFromRouter(g.stashDialog.get());
    handleFromRouter(g.manageLootDialog.get());
    handleFromRouter(m_pExaltationForgeDialogController.get());
    handleFromRouter(m_pReportViolationController.get());
    handleFromRouter(m_pBugReportDialogController.get());
    handleFromRouter(m_pNaggingMessageDialogController.get());
    handleFromRouter(m_pRewardWallDialogController.get());
    handleFromRouter(g.optionsDialog.get());
    handleFromRouter(g.chat.get());
    handleFromRouter(g.actionBars.get());
    handleFromRouter(g.targetSelection.get());
    g.actionBars->restoreLastAktionBarConfiguration();
    handleFromRouter(g.informationDialogs.get());
    handleFromRouter(g.exivaOptionsDialog.get());
    handleFromRouter(g.taskboardDialog.get());
    handleFromRouter(g.tutorialShell.get());
    handleFromRouter(g.tutorialOverlay.get());
    handleFromRouter(m_pWeaponProficiencyDialogController.get());
    handleFromRouter(m_pWeaponProficiencyShapeDialogController.get());
    handleFromRouter(m_pWeaponProficiencyShapeOptionsDialogController.get());
    handleFromRouter(m_pLenshelpManager.get());
    handleFromRouter(g.skillWheel.get());
    handleFromRouter(g.npcDialog.get());
    handleFromRouter(g.sessionBootstrap.get());

    // Must outlive bootstrapAll: SuiteRuntime keeps PackHostWire* for deferred
    // slots (HUD chat switch, store/options/logout).
    g.packHost = std::make_unique<qtclient::PackHostWire>();
    qtclient::PackHostWire& packHost = *g.packHost;
    packHost.bindQuestTracker = [&g](mods::MissionBackend* backend, qtclient::BrowseOpenerFn browse) {
        g.sidebarWidgets->questTrackerWidget()->setMissionBackend(backend);
        g.sidebarWidgets->questTrackerWidget()->setBrowseOpener(std::move(browse));
    };
    packHost.bindQuestLog = [&g](mods::MissionBackend* backend, qtclient::BrowseOpenerFn browse) {
        if (auto* log = g.informationDialogs->questLog()) {
            log->setMissionBackend(backend);
            log->setBrowseOpener(std::move(browse));
        }
    };
    packHost.bindInstanceToInfoDialogs = [&g](mods::InstanceBackend* instance) {
        g.informationDialogs->setInstanceStorage(instance);
    };
    packHost.bindAttentionToButtonBar = [&g](mods::AttentionBackend* attention) {
        if (auto* buttonBar = g.sidebarPanels->buttonBar())
            buttonBar->setAttentionStorage(attention);
    };
    packHost.addProductOptionsPage = [optionsDialog](const int category, void* page) {
        optionsDialog->addProductPage(category, static_cast<config::TOptionsPage*>(page));
    };
    packHost.openOptionsPage = [optionsDialog](const int page) { optionsDialog->open(page); };
    packHost.openStore = [storeDialog] { storeDialog->open(); };
    packHost.requestLogoutViaButtonBar = [this, &g] {
        if (auto* buttonBar = g.sidebarPanels->buttonBar()) {
            buttonBar->onLogoutButtonClicked();
            return;
        }
        requestLogoutOrShowStartScreen(nullptr);
    };
    packHost.setSocialButtonOnButtonBar = [&g](QObject* controller) {
        if (auto* buttonBar = g.sidebarPanels->buttonBar())
            buttonBar->setSocialButtonController(controller);
    };
    packHost.setActiveChatItem = [&g](QQuickItem* item) {
        if (!item)
            return;
        g.chat->setChatItem(item);
    };
    packHost.installCreateCharacterController = [&g](QObject* controller) {
        if (!g.sessionBootstrap || !controller)
            return;
        g.sessionBootstrap->installCreateCharacter(qobject_cast<gamewindow::TCreateCharacterDialogController*>(controller));
    };
    packHost.blessingsDialog = [this]() -> QObject* { return m_pBlessingsDialogController.get(); };
    packHost.npcDialog = [&g]() -> QObject* { return g.npcDialog.get(); };
    packHost.chatController = [chat]() -> QObject* { return chat; };
    packHost.chatStorage = [chat]() -> QObject* { return chat->storage(); };
    // Pack ABI shim: the current game session's store handler.
    packHost.storeMessages = [this]() -> QObject* {
        const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_currentGameSession.get());
        return session ? session->storeProtocolMessageHandler().get() : nullptr;
    };
    packHost.storeImageManager = [storeDialog]() -> QObject* {
        return storeDialog->property("dynamicallyLoadedImageManager").value<QObject*>();
    };
    packHost.minimapController = [&g]() -> QObject* { return g.minimapController; };
    packHost.sidebarManager = [sidebarManager]() -> QObject* { return sidebarManager; };
    packHost.sidebarWidgets = [sidebarWidgets]() -> QObject* { return sidebarWidgets; };
    packHost.containers = [containers]() -> QObject* { return containers; };
    packHost.buttonBar = [sidebarPanels]() -> QObject* { return sidebarPanels->buttonBar(); };
    packHost.imbuingDialog = [&g]() -> QObject* { return g.imbuingDialog.get(); };
    // Pack ABI shim: the current game session's imbuing storage.
    packHost.imbuingStorage = [this]() -> QObject* {
        const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_currentGameSession.get());
        return session ? session->imbuingStorage().get() : nullptr;
    };
    packHost.sessionBootstrap = [&g]() -> QObject* { return g.sessionBootstrap.get(); };
    packHost.gameActionRouter = [router]() -> QObject* { return router; };
    packHost.minimapPanelItem = [sidebarPanels]() -> QQuickItem* { return sidebarPanels->minimapItem(); };
    packHost.inventoryPanelItem = [sidebarPanels]() -> QQuickItem* { return sidebarPanels->inventoryItem(); };
    packHost.buttonBarPanelItem = [sidebarPanels]() -> QQuickItem* { return sidebarPanels->buttonBarItem(); };

    mods::fillPackHostAbiFromPackHostWire(&packHost);

    {
        mods::ModContext ctx;
        ctx.engine = engine;
        ctx.rootItem = window->contentItem();
        ctx.mapWindowPane = mapWindowPane;
        ctx.gameWindowItem = gameWindowItem;
        ctx.gameClient = this;
        ctx.gameHost = gameHost();
        ctx.dialogStack = dialogStack;
        ctx.host = g.productHost.get();
        ctx.launcherEnv = &g.env;
        ctx.packHost = &packHost;
        mods::bootstrapAll(ctx);
    }

    connect(this, &TGameClient::worldEntered, gameWindowItem, [this, &g] {
        // Show the game window before the heavier binds. A throw later must not
        // leave the login mask hidden and the game window still off.
        g.gameWindowController->showGameScreen();
        try {
            g.sidebarWidgets->vipWidget()->setWorldType(worldType());
            g.sidebarWidgets->setSidebarOptions(sidebarOptions());
            g.sidebarPanels->setSidebarOptions(sidebarOptions());
            if (gamewindow::TPremiumPanelController* premiumPanel = g.sidebarPanels->premiumPanel()) {
                premiumPanel->setPremiumTimeStorage(game::TPremiumTimeStorage::liveInstance());
                premiumPanel->onSessionStarted();
                if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_currentGameSession.get()))
                    connect(session->storeProtocolMessageHandler().get(), &store::TStoreProtocolMessageHandler::storeButtonDeeplinkChanged,
                            premiumPanel, &gamewindow::TPremiumPanelController::onStoreDeeplinkChanged, Qt::UniqueConnection);
            }
            if (game::TGameserverGameSession* live = session())
                live->skillWheelGameActionHandler()->setSessionPremium(account().session.isPremium);
            g.containers->setSidebarOptions(sidebarOptions());
            g.sidebarWidgets->onLoginRampUpFinished();
            g.statusBarController->setStorage(statusbarStorage());
            g.informationDialogs->setAmbientLightStorage(ambientLight());
            g.informationDialogs->setMinimapTileStorage(minimap() ? minimap()->tileStorage() : nullptr);
            if (g.minimapController) {
                g.minimapController->setAmbientLightStorage(ambientLight());
                g.minimapController->setMarkerStorage(minimap() ? minimap()->markerStorage() : nullptr);
            }
            g.sidebarPanels->setTileStorage(minimap() ? minimap()->tileStorage() : nullptr);
            g.optionsDialog->setInGame(true);
            g.actionBars->onWorldEntered();
            qmlcomponents::applyOptionVolumes(); // stop login anthem; keep channel gains
        } catch (...) {
        }
    });
    connect(this, &TGameClient::worldLeft, gameWindowItem, [&g] {
        if (g.minimapController) {
            g.minimapController->setMarkerStorage(nullptr);
            g.minimapController->setTileStorage(nullptr);
        }
        g.informationDialogs->setMinimapTileStorage(nullptr);
        g.sidebarPanels->setSidebarOptions(nullptr);
        g.sidebarWidgets->setSidebarOptions(nullptr);
        g.containers->setSidebarOptions(nullptr);
        g.statusBarController->setStorage(nullptr);
        g.optionsDialog->setInGame(false);
        g.dialogStack->hideAll();
        g.gameWindowController->showStartScreen();
        qmlcomponents::applyOptionVolumes(); // resume login anthem when offline
    });
    connect(application, &QCoreApplication::aboutToQuit, this, [this, &g] {
        g.mapWindowController->unbind();
        if (g.minimapController) {
            g.minimapController->setMarkerStorage(nullptr);
            g.minimapController->setTileStorage(nullptr);
        }
        g.informationDialogs->setMinimapTileStorage(nullptr);
        shutdown();
    });
    // start() activated the session before the controllers above created their
    // protocol message handlers (chat, store, market, trades, ...): link them now,
    // and connect the controllers made after it to it.
    if (auto* session = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get()))
        session->relinkProtocolMessageHandlers();
    // A replay's session starts before the game window: the controllers made
    // here connect to it now.
    if (m_currentGameSession)
        connectGameWindowControllersToGameSession(m_currentGameSession);
    return true;
}

gamewindow::TGameSessionDisconnectReactionController* TGameClient::gameSessionDisconnectReactionController() const
{
    return m_pGameSessionDisconnectReactionController.get();
}

void TGameClient::connectGameWindowControllersToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    // CipSoft's order of the game window controllers' connectors.
    if (m_gameWindow) {
        TGameWindow& g = *m_gameWindow;
        if (session && session->newsProtocolMessageHandler() && g.gameWindowController)
            session->newsProtocolMessageHandler()->setNewsStorage(g.gameWindowController->newsStorage());
        if (g.mapWindowController)
            g.mapWindowController->connectToGameSession(GameSession);
        // Host: the battle lists' mouse target and the FPS indicator's build
        // times are the session's world map frame's.
        if (g.sidebarWidgets)
            g.sidebarWidgets->battleLists()->setWorldMapRenderProvider(session ? session->worldMapRenderProvider().get() : nullptr);
        if (g.fpsLatency)
            g.fpsLatency->setRenderProvider(session ? session->worldMapRenderProvider().get() : nullptr);
        if (g.containers)
            g.containers->connectToGameSession(GameSession);
        if (m_pSelectObjectAmountDialogController)
            m_pSelectObjectAmountDialogController->connectToGameSession(GameSession);
        if (g.chat)
            g.chat->connectToGameSession(GameSession);
        if (m_pMarketController)
            m_pMarketController->connectToGameSession(GameSession);
        // Host shim: the analysers' aggregate follows the session's storages;
        // the drop tracker reports on the chat's channels.
        if (g.analyserStorages)
            g.analyserStorages->connectToGameSession(GameSession);
        if (session)
            session->itemTrackingStorage()->setChatStorage(session->chatChannelStorage().get());
        // CIP: the sidebar widgets manager connects every widget it owns and
        // opens and closes the player trade and depot search widgets from
        // the session's protocol handlers.
        if (g.sidebarWidgets)
            g.sidebarWidgets->connectToGameSession(GameSession);
        if (g.sidebarPanels)
            g.sidebarPanels->connectToGameSession(GameSession);
        if (g.cooldownBar)
            g.cooldownBar->connectToGameSession(GameSession);
        if (m_pGameSessionDisconnectReactionController)
            m_pGameSessionDisconnectReactionController->connectToGameSession(GameSession);
        if (m_messageIgnoreList)
            m_messageIgnoreList->setGameSession(GameSession);
        if (m_pReportViolationController)
            m_pReportViolationController->connectToGameSession(GameSession);
        if (g.informationDialogs)
            g.informationDialogs->editText()->connectToGameSession(GameSession);
        if (m_pStoreDialogController)
            m_pStoreDialogController->connectToGameSession(GameSession);
        // CIP: the button bar connects its store button.
        if (g.storeButton)
            g.storeButton->connectToGameSession(GameSession);
        if (m_pBugReportDialogController)
            m_pBugReportDialogController->connectToGameSession(GameSession);
        if (g.informationDialogs)
            if (gamewindow::TPreyDialogController* prey = g.informationDialogs->prey())
                prey->connectToGameSession(GameSession);
        if (g.imbuingDialog) {
            g.imbuingDialog->connectToGameSession(GameSession);
            g.imbuingDialog->connectToPremiumTimeStorage(game::TPremiumTimeStorage::liveInstance());
        }
        if (gamewindow::TInspectObjectDialogController* inspectObject = gamewindow::TInspectObjectDialogController::instance())
            inspectObject->connectToGameSession(GameSession);
        if (gamewindow::TInspectPlayerDialogController* inspectPlayer = gamewindow::TInspectPlayerDialogController::instance())
            inspectPlayer->connectToGameSession(GameSession);
        if (m_pDeathDialogController)
            m_pDeathDialogController->connectToGameSession(GameSession);
        if (gamewindow::TOutfitMemorialDialogController* memorial = gamewindow::TOutfitMemorialDialogController::instance())
            memorial->connectToGameSession(GameSession);
        if (gamewindow::TMultiOfflineTrainingDialogController* offlineTraining = gamewindow::TMultiOfflineTrainingDialogController::instance())
            offlineTraining->connectToGameSession(GameSession);
        if (g.actionBars)
            g.actionBars->connectToGameSession(GameSession);
        if (m_pBlessingsDialogController) {
            m_pBlessingsDialogController->connectToGameSession(GameSession);
            m_pBlessingsDialogController->connectToPremiumTimeStorage(game::TPremiumTimeStorage::liveInstance());
        }
        if (m_pRewardWallDialogController) {
            m_pRewardWallDialogController->connectToGameSession(GameSession);
            m_pRewardWallDialogController->connectToPremiumTimeStorage(game::TPremiumTimeStorage::liveInstance());
        }
        if (g.manageLootDialog) {
            g.manageLootDialog->connectToGameSession(GameSession);
            g.manageLootDialog->connectToPremiumTimeStorage(game::TPremiumTimeStorage::liveInstance());
        }
        if (g.informationDialogs) {
            if (gamewindow::TCyclopediaDialogController* cyclopedia = g.informationDialogs->cyclopedia()) {
                cyclopedia->connectToGameSession(GameSession);
                cyclopedia->setPremiumTimeStorage(game::TPremiumTimeStorage::liveInstance());
            }
            // CIP: the button bar's connector (unported) connects these buttons.
            g.informationDialogs->cyclopediaButton()->connectToGameSession(GameSession);
            g.informationDialogs->bosstiaryButton()->connectToGameSession(GameSession);
            // Host: the Cyclopedia's houses and map pages draw the session's light.
            g.informationDialogs->setAmbientLightStorage(session ? session->ambientLightStorage().get() : nullptr);
        }
        if (g.taskboardDialog)
            g.taskboardDialog->connectToGameSession(GameSession);
        if (m_pSoulSealsDialogController)
            m_pSoulSealsDialogController->connectToGameSession(GameSession);
        if (g.informationDialogs)
            if (auto* spellBook = g.informationDialogs->findChild<gamewindow::TShowSpellBookDialogController*>())
                spellBook->connectToGameSession(GameSession);
        if (g.stashDialog)
            g.stashDialog->connectToGameSession(GameSession);
        if (g.exivaOptionsDialog)
            g.exivaOptionsDialog->connectToGameSession(GameSession);
        if (g.screenshotManager)
            g.screenshotManager->connectToGameSession(GameSession);
        m_predictor->connectToGameSession(GameSession);
        if (g.informationDialogs)
            if (gamewindow::THighscoresDialogController* highscores = g.informationDialogs->highscores())
                highscores->connectToGameSession(GameSession);
        // The quest log and the quest tracker show the session's quest log storage.
        if (g.informationDialogs)
            if (gamewindow::TQuestLogController* questLog = g.informationDialogs->questLog())
                questLog->connectToGameSession(GameSession);
        if (gamewindow::TCharacterTradeDialogController* characterTrade = gamewindow::TCharacterTradeDialogController::instance())
            characterTrade->connectToGameSession(GameSession);
        if (m_pExaltationForgeDialogController)
            m_pExaltationForgeDialogController->connectToGameSession(GameSession);
        if (m_pStoreAndResourceBalanceHelperController) {
            if (g.skillWheel)
                m_pStoreAndResourceBalanceHelperController->setSkillWheelStorage(g.skillWheel->storage());
            m_pStoreAndResourceBalanceHelperController->connectToGameSession(GameSession);
        }
        if (g.skillWheel)
            g.skillWheel->connectToGameSession(GameSession);
        if (m_pWeaponProficiencyDialogController)
            m_pWeaponProficiencyDialogController->connectToGameSession(GameSession);
        if (m_pWeaponProficiencyShapeDialogController)
            m_pWeaponProficiencyShapeDialogController->connectToGameSession(GameSession);
        if (m_pWeaponProficiencyReshapeDialogController)
            m_pWeaponProficiencyReshapeDialogController->connectToGameSession(GameSession);
        if (g.npcDialog)
            g.npcDialog->connectToGameSession(GameSession);
        // Host shim until these controllers connect to the session themselves:
        // they follow the session's character configuration being loaded.
        if (g.containers)
            g.containers->setCharacterConfigurationManager(session ? session->characterConfigurationManager().get() : nullptr);
        if (config::TCharacterConfigurationManager* manager = session ? session->characterConfigurationManager().get() : nullptr) {
            if (g.analyserStorages)
                connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, g.analyserStorages.get(),
                        &analysers::TAnalyserStorages::onCharacterConfigurationLoaded, Qt::UniqueConnection);
            if (gamewindow::TCyclopediaDialogController* cyclopedia = g.informationDialogs ? g.informationDialogs->cyclopedia() : nullptr)
                connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, cyclopedia->mapDialog(),
                        &gamewindow::TCyclopediaMapDialogController::showDialogTab, Qt::UniqueConnection);
            if (gamewindow::TQuestLogController* questLog = g.informationDialogs ? g.informationDialogs->questLog() : nullptr)
                connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, questLog,
                        &gamewindow::TQuestLogController::onCharacterConfigurationLoaded, Qt::UniqueConnection);
        }
    }
    if (m_pSoundController)
        m_pSoundController->connectToGameSession(GameSession);
    if (m_pBossDifficultySelectionDialogController)
        m_pBossDifficultySelectionDialogController->connectToGameSession(GameSession);
    // CIP: the proficiency storage is emptied last.
    m_pProficiencyStorage->clear();
}

void TGameClient::onGameSessionDisconnected()
{
    if (!m_currentGameSession)
        return;
    // Host: the packs' session-end hook (notify_api subscribe_session_end), while the
    // disconnected session is still the current one (CIP currentGamesessionDisconnected).
    tibia::world::notifySessionEnd();
    // CIP: the dialogs that only make sense during the session close, in
    // CipSoft's order: connecting, minimap marker, outfit, creature podium,
    // white/blacklist, move to stash, tutorial hint, vocation selection,
    // offline training and the proficiency shape options.
    if (m_gameWindow) {
        TGameWindow& g = *m_gameWindow;
        if (g.sessionBootstrap)
            g.sessionBootstrap->closeConnectingDialog();
        if (m_minimap)
            if (gamewindow::TEditMinimapMarkerDialogController* marker = m_minimap->markerStorage()->editMinimapMarkerDialog())
                marker->dialog()->close();
        if (g.informationDialogs) {
            g.informationDialogs->outfitDialog()->dialog()->close();
            g.informationDialogs->podiumDialog()->dialog()->close();
        }
        if (g.chat)
            g.chat->whiteBlacklistDialog().close();
        if (g.stashDialog)
            g.stashDialog->confirmMoveObjectToStashDialog()->closeDialog();
        if (g.tutorialShell) {
            g.tutorialShell->hint()->dialog()->close();
            g.tutorialShell->vocation()->dialog()->close();
        }
        if (gamewindow::TMultiOfflineTrainingDialogController* offlineTraining = gamewindow::TMultiOfflineTrainingDialogController::instance())
            offlineTraining->closeDialog();
        if (m_pWeaponProficiencyShapeOptionsDialogController)
            m_pWeaponProficiencyShapeOptionsDialogController->closeDialog();
    }
    // Host: the world is left (minimap and character options written, start screen).
    onWorldLeft();
    if (!m_gameSessionMetaInfoQueue.empty()) {
        autoconnectNextGameSessionInQueue();
        return;
    }
    setCurrentGameSession(nullptr);
    if (gamewindow::TGameSessionDisconnectReactionController* reaction = gameSessionDisconnectReactionController();
        reaction && reaction->reconnectRequested())
        loginAgainWithLastCharacter();
}

}
}
