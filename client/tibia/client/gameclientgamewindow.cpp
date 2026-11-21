// The game client's game window: the start screen, the map, sidebars, chat,
// bars and every dialog controller of a game session, created once the
// client window exists (CIP: TGameClient creates them on its window-setup
// timer and keeps them as its members).
#include "client/gameclient.h"
#include "game/gameactionrouter.h"
#include "game/gamesessionbase.h"

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
#include "renderer/nextframeupdater.h"
#include "renderer/worldmaprenderprovider.h"
#include "sound/soundcontroller.h"
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
#include "gamewindow/tutorialhintdialogcontroller.h"
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
#include "quickloot/managedcontainerstorage.h"
#include "screenshot/screenshotciptypes.h"
#include "skillwheel/skillwheelmanager.h"
#include "store/dynamicallyloadedimage.h"

#include "clockservice.h"

#include <QGuiApplication>
#include <qtclient/pack_host.h>
#include <tibia/game/gamesession.h>

#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include "gamewindow/gamewindowciptypes.h"
#include "input/gameactiondispatch.h"
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>

#include <exception>

namespace tibia {
namespace client {

namespace {

/// The quit prompt on window close (Exit / Logout / Cancel).
class QuitCloseFilter final : public QObject
{
public:
    QuitCloseFilter(gamewindow::TGameWindowController* controller, QObject* parent = nullptr)
        : QObject(parent)
        , m_controller(controller)
    {
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::Close || !m_controller)
            return QObject::eventFilter(watched, event);
        // SIGINT/SIGTERM is handled by asking quitPressed, same as this close.
        // Only an intentional relaunch (graphics-engine restart) skips the prompt.
        if (gamewindow::TGameWindowController::processExitAllowed())
            m_controller->setAllowWindowClose(true);
        if (m_controller->allowWindowClose())
            return QObject::eventFilter(watched, event);
        event->ignore();
        m_controller->quitPressed();
        return true;
    }

private:
    QPointer<gamewindow::TGameWindowController> m_controller;
};

}

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
    std::shared_ptr<gamewindow::TGameWindowController> gameWindowController;
    std::unique_ptr<gamewindow::TStatusBarController> statusBarController;
    std::unique_ptr<QObject> gameWindow;
    std::unique_ptr<qmlcomponents::TLenshelpManager> lenshelpManager;
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
    /// CIP +0x418: the dialogs and returns when the current session cannot connect or ends.
    std::shared_ptr<gamewindow::TGameSessionDisconnectReactionController> gameSessionDisconnectReaction;
    std::unique_ptr<app::TSessionBootstrap> sessionBootstrap;
    std::unique_ptr<gamewindow::TSelectObjectAmountDialogController> splitStackDialog;
    std::unique_ptr<analysers::TAnalyserStorages> analyserStorages;
    std::unique_ptr<gamewindow::TInformationDialogs> informationDialogs;
    std::unique_ptr<gamewindow::TDragAndDropController> dragAndDrop;
    std::unique_ptr<store::TStoreStorage> storeStorage;
    std::unique_ptr<store::TStoreProtocolMessageHandler> storeMessages;
    std::unique_ptr<gamewindow::TStoreAndResourceBalanceHelperController> storeBalance;
    std::unique_ptr<gamewindow::TStoreDialogController> storeDialog;
    std::unique_ptr<gamewindow::TStoreButtonController> storeButton;
    std::unique_ptr<market::TMarketStorage> marketStorage;
    std::unique_ptr<market::TMarketProtocolMessageHandler> marketMessages;
    std::unique_ptr<gamewindow::TMarketController> marketController;
    std::unique_ptr<gamewindow::TStashDialogController> stashDialog;
    std::unique_ptr<quickloot::TManagedContainerStorage> managedContainers;
    std::unique_ptr<gamewindow::TManageLootContainerDialogController> manageLootDialog;
    std::unique_ptr<imbuing::TImbuingStorage> imbuingStorage;
    std::unique_ptr<gamewindow::TImbuingDialogController> imbuingDialog;
    std::unique_ptr<exaltation::TExaltationStorage> exaltationStorage;
    std::unique_ptr<gamewindow::TExaltationForgeDialogController> exaltationForgeDialog;
    std::unique_ptr<skillwheel::TSkillWheelManager> skillWheel;
    std::unique_ptr<dailyreward::TDailyRewardStorage> dailyRewardStorage;
    std::unique_ptr<dailyreward::TDailyRewardGameActionHandler> dailyRewardActions;
    std::unique_ptr<screenshot::TScreenshotManager> screenshotManager;
    std::unique_ptr<gamewindow::TSidebarPanelsManager> sidebarPanels;
    std::unique_ptr<gamewindow::TSidebarWidgetsManager> sidebarWidgets;
    gamewindow::TMinimapController* minimapController = nullptr;
    std::unique_ptr<gamewindow::TContainerStorageController> containers;
    std::unique_ptr<gamewindow::TVipWidgetManager> vipWidget;
    std::unique_ptr<exivaoptions::TExivaOptionsDialogController> exivaOptionsDialog;
    std::unique_ptr<gamewindow::TReportViolationController> reportViolation;
    std::unique_ptr<gamewindow::TBugReportDialogController> bugReport;
    std::unique_ptr<gamewindow::TNaggingMessageDialogController> naggingDialog;
    std::unique_ptr<gamewindow::TDeathDialogController> deathDialog;
    std::unique_ptr<gamewindow::TKillTrackerWidgetManager> preyWidget;
    std::unique_ptr<gamewindow::TQuestTrackerWidgetManager> questTrackerWidget;
    std::unique_ptr<gamewindow::TLootTrackingWidgetManager> lootTrackingWidget;
    std::unique_ptr<gamewindow::TSpellListWidgetManager> spellListWidget;
    std::unique_ptr<gamewindow::TUnjustifiedPointsWidgetManager> unjustifiedWidget;
    std::unique_ptr<gamewindow::TImbuementTrackerWidgetManager> imbuementTrackerWidget;
    std::unique_ptr<gamewindow::TCreatureTrackerWidgetManager> bestiaryTrackerWidget;
    std::unique_ptr<gamewindow::TCreatureTrackerWidgetManager> bosstiaryTrackerWidget;
    std::unique_ptr<gamewindow::TBossTrackerWidgetManager> bossCooldownsWidget;
    std::unique_ptr<gamewindow::TDepotSearchWidgetManager> depotSearchWidget;
    std::unique_ptr<gamewindow::TPlayerGuideWidgetManager> playerGuideWidget;
    std::unique_ptr<gamewindow::TCipSoftUiEventBridge> cipSoftUiEventBridge;
    std::unique_ptr<analysers::TAnalyserWidgets> analyserWidgets;
    std::unique_ptr<proficiency::TProficiencyStorage> proficiencyStorage;
    std::unique_ptr<proficiency::TWeaponProficiencyDialogController> weaponProficiencyDialog;
    std::unique_ptr<proficiency::TWeaponProficiencyShapeDialogController> weaponProficiencyShapeDialog;
    std::unique_ptr<proficiency::TWeaponProficiencyReshapeDialogController> weaponProficiencyReshapeDialog;
    std::unique_ptr<proficiency::TWeaponProficiencyShapeOptionsDialogController> weaponProficiencyShapeOptionsDialog;
    std::unique_ptr<gamewindow::TBlessingsDialogController> blessingsDialog;
    std::unique_ptr<gamewindow::TRewardWallDialogController> rewardWallDialog;
    std::unique_ptr<gamewindow::TTaskboardDialogController> taskboardDialog;
    std::unique_ptr<gamewindow::TTutorialShell> tutorialShell;
    std::unique_ptr<gamewindow::TOptionsMenuDialogController> optionsDialog;
    std::unique_ptr<input::TOpenWebsiteGameActionHandler> openWebsiteActions;
    std::unique_ptr<gamewindow::TFPSLatencyIndicatorController> fpsLatency;
    std::unique_ptr<gamewindow::TChatController> chat;
    std::unique_ptr<gamewindow::TActionBarManager> actionBars;
    std::unique_ptr<gamewindow::TCooldownBarController> cooldownBar;
    std::unique_ptr<input::TPlayerMovementGameActionHandler> playerMovementActions;
    std::unique_ptr<input::TClipboardGameActionHandler> clipboardActions;
    std::unique_ptr<input::TUseWithGameActionHandler> useWithActions;
    std::unique_ptr<input::TCrossHairSpellGameActionHandler> crossHairSpellActions;
    std::unique_ptr<input::TGenericGameActionHandler> genericActions;
    std::unique_ptr<input::TGatherAllGameActionHandler> gatherAllActions;
    std::unique_ptr<input::TGatherAllProtocolMessageHandler> gatherAllProtocol;
    std::unique_ptr<trade::TNPCTradeStorage> npcTrade;
    std::unique_ptr<trade::TNPCTradeProtocolMessageHandler> npcTradeMessages;
    std::unique_ptr<gamewindow::TNpcDialogController> npcDialog;
    std::unique_ptr<gamewindow::TPlayerTradeWidgetManager> playerTrade;
    /// Outlives the packs' bootstrap: their deferred slots keep pointers into it.
    std::unique_ptr<qtclient::PackHostWire> packHost;
};

void TGameClient::TGameWindowDeleter::operator()(TGameWindow* gameWindow) const { delete gameWindow; }

void TGameClient::requestLogoutOrShowStartScreen(gamewindow::TGameWindowController* startScreen)
{
    if (game::TGameserverGameSession* gameSession = session())
        gameSession->requestLogout();
    else if (game::TSessiondumpGameSession* dump = sessiondump())
        dump->requestLogout();
    else if (startScreen)
        startScreen->showStartScreen();
}

bool TGameClient::createGameWindow(const TGameWindowSetup& setup)
{
    m_gameWindow.reset(new TGameWindow);
    TGameWindow& g = *m_gameWindow;
    g.env = setup.env;
    QQmlEngine* engine = setup.engine;
    QQuickWindow* window = setup.window;
    const bool preview = setup.preview;
    QCoreApplication* application = QCoreApplication::instance();

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
    // The start screen's Quit asks the client to exit; Manage Clients opens the launcher.
    connect(gameWindowController, &gamewindow::TGameWindowController::gameExitRequested, gameWindowController,
            &gamewindow::TGameWindowController::handleGameExitRequest);
    connect(gameWindowController, &gamewindow::TGameWindowController::openLauncherRequested, gameWindowController,
            [] { app::LauncherEnv::showLauncher(); });
    window->installEventFilter(new QuitCloseFilter(gameWindowController, window));
    connect(gameWindowController, &gamewindow::TGameWindowController::quitGameExitRequested, window,
            [this, window, gameWindowController] {
                // Close of the last window can exit from AppKit before aboutToQuit.
                shutdown();
                gameWindowController->setAllowWindowClose(true);
                window->close();
                QCoreApplication::quit();
            });
    connect(gameWindowController, &gamewindow::TGameWindowController::quitGameLogoutRequested, this,
            [this, gameWindowController] { requestLogoutOrShowStartScreen(gameWindowController); });
    g.gameWindow = gameWindowController->createGameWindow(engine);
    g.gameWindowItem = qobject_cast<QQuickItem*>(g.gameWindow.get());
    QQuickItem* gameWindowItem = g.gameWindowItem;
    if (!gameWindowItem)
        return false;
    // Lenshelp mode and its popup over the game window.
    g.lenshelpManager = std::make_unique<qmlcomponents::TLenshelpManager>(engine, gameWindowItem);
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
    // clientwindow.qml: the minimum width grows with each sidebar so the 15×11
    // map is not crushed. The manager already emitted before this connection.
    const auto applySidebarCount = [window, sidebarManager] {
        window->setProperty("numberOfSidebars", static_cast<int>(sidebarManager->sidebarIds().size()));
        if (QObject* split = window->findChild<QObject*>(QStringLiteral("mainFrame")))
            QTimer::singleShot(0, split, [split] { QMetaObject::invokeMethod(split, "correctLayout"); });
    };
    applySidebarCount();
    connect(sidebarManager, &gamewindow::TSidebarManager::numberOfSidebarsChanged, window, applySidebarCount);

    g.dialogComponent = std::make_unique<QQmlComponent>(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/CharacterSelection.qml")));
    g.dialog.reset(g.dialogComponent->beginCreate(engine->rootContext()));
    g.dialogItem = qobject_cast<QQuickItem*>(g.dialog.get());
    QQuickItem* dialogItem = g.dialogItem;
    if (!dialogItem) {
        return false;
    }
    g.dialogComponent->setInitialProperties(
        dialogItem, { { QStringLiteral("controller"), QVariant::fromValue<QObject*>(setup.characterSelection) } });
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
    mapWindowController->setRenderProvider(worldMapRenderProvider());
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
    // Session chrome (WaitDialog, errors, create account/character, keep_renderer).
    // Must run after the dialog stack so MessageDialog / WaitDialog can show.
    if (!preview)
        g.sessionBootstrap = std::make_unique<app::TSessionBootstrap>(this, g.env, gameWindowController, g.productHost.get());
    // Before the first session: it reacts to the session's connection and login.
    g.gameSessionDisconnectReaction =
        std::make_shared<gamewindow::TGameSessionDisconnectReactionController>(m_authentication, g.gameWindowController);
    connect(g.gameSessionDisconnectReaction.get(),
            &gamewindow::TGameSessionDisconnectReactionController::loginAgainAfterLoginWaitElapsed, this,
            &TGameClient::loginAgainWithLastCharacter, Qt::QueuedConnection);
    connect(g.gameSessionDisconnectReaction.get(), &gamewindow::TGameSessionDisconnectReactionController::quitAndPatchClient,
            application, quitAndPatchClient);
    connect(gameActionRouter()->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal,
            g.gameSessionDisconnectReaction.get(), &gamewindow::TGameSessionDisconnectReactionController::handleGameAction);
    connect(this, &TGameClient::sessionEmailAvailable, gameWindowController,
            &gamewindow::TGameWindowController::rememberLoginEmailIfEnabled);
    if (!preview) {
        if (!start(setup.argv0, setup.workDir, setup.options))
            return false;
    }
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
    g.splitStackDialog = std::make_unique<gamewindow::TSelectObjectAmountDialogController>();
    // Before the dialogs: the Cyclopedia item page and the loot widgets read its item storages.
    g.analyserStorages = std::make_unique<analysers::TAnalyserStorages>();
    g.informationDialogs = std::make_unique<gamewindow::TInformationDialogs>();
    gamewindow::TInformationDialogs* informationDialogs = g.informationDialogs.get();
    g.dragAndDrop = std::make_unique<gamewindow::TDragAndDropController>(
        window->contentItem(), gameWindowController->gameWindowChild(QStringLiteral("dragApperanceInstanceIcon")));
    engine->rootContext()->setContextProperty(QStringLiteral("tibiaDragAndDropController"), g.dragAndDrop.get());
    g.storeStorage = std::make_unique<store::TStoreStorage>();
    g.storeMessages = std::make_unique<store::TStoreProtocolMessageHandler>();
    g.storeMessages->setStorage(g.storeStorage.get());
    g.storeBalance = std::make_unique<gamewindow::TStoreAndResourceBalanceHelperController>(g.storeStorage.get());
    engine->rootContext()->setContextProperty(QStringLiteral("storeAndResourceBalanceHelper"), g.storeBalance.get());
    g.storeDialog = std::make_unique<gamewindow::TStoreDialogController>(g.storeStorage.get(), g.storeMessages.get());
    gamewindow::TStoreDialogController* storeDialog = g.storeDialog.get();
    g.storeButton = std::make_unique<gamewindow::TStoreButtonController>(storeDialog);
    g.marketStorage = std::make_unique<market::TMarketStorage>();
    g.marketMessages = std::make_unique<market::TMarketProtocolMessageHandler>(g.marketStorage.get());
    g.storeMessages->setMarketStorage(g.marketStorage.get());
    g.marketController = std::make_unique<gamewindow::TMarketController>(g.marketStorage.get(), g.marketMessages.get());
    g.stashDialog = std::make_unique<gamewindow::TStashDialogController>();
    connect(g.marketController.get(), &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed, g.stashDialog.get(),
            &gamewindow::TStashDialogController::onMarketDialogDialogClosed);
    g.managedContainers = std::make_unique<quickloot::TManagedContainerStorage>();
    g.manageLootDialog = std::make_unique<gamewindow::TManageLootContainerDialogController>();
    connect(g.manageLootDialog.get(), &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed, g.stashDialog.get(),
            &gamewindow::TStashDialogController::onManageLootContainerDialogClosed);
    g.imbuingStorage = std::make_unique<imbuing::TImbuingStorage>();
    g.imbuingDialog = std::make_unique<gamewindow::TImbuingDialogController>(g.imbuingStorage.get());
    g.exaltationStorage = std::make_unique<exaltation::TExaltationStorage>();
    g.exaltationForgeDialog = std::make_unique<gamewindow::TExaltationForgeDialogController>(g.exaltationStorage.get());
    g.storeBalance->setExaltationStorage(g.exaltationStorage.get());
    g.skillWheel = std::make_unique<skillwheel::TSkillWheelManager>();
    g.gameActions.on(input::EGameAction::OPEN_STORE, [storeDialog](const input::TGameActionOpenStore& action) {
        const input::TGameActionOpenStore::TStoreOpenParameters& parameters = action.storeOpenParameters();
        storeDialog->openWith(parameters.openAction, parameters.detail, parameters.text);
        return true;
    });
    // The daily reward: the session's storage and its game actions (238, 243, 244).
    g.dailyRewardStorage = std::make_unique<dailyreward::TDailyRewardStorage>();
    g.dailyRewardActions = std::make_unique<dailyreward::TDailyRewardGameActionHandler>(g.dailyRewardStorage.get());
    g.gameActions.on(input::EGameAction::LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES, [this](const input::IGameAction&) {
        if (minimap::TMinimapManager* manager = minimap())
            manager->loadDefaultTilesFromResources();
        return true;
    });
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
    g.sidebarWidgets = std::make_unique<gamewindow::TSidebarWidgetsManager>(sidebarManager, gameWindowItem, engine);
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
    sidebarWidgets->battleLists()->setWorldMapRenderProvider(worldMapRenderProvider());
    g.vipWidget = std::make_unique<gamewindow::TVipWidgetManager>(gameWindowItem, engine);
    gamewindow::TVipWidgetManager* vipWidget = g.vipWidget.get();
    vipWidget->setControlScheme(controlScheme);
    informationDialogs->setControlScheme(controlScheme);
    g.exivaOptionsDialog = std::make_unique<exivaoptions::TExivaOptionsDialogController>();
    g.exivaOptionsDialog->setStorage(vipWidget->exivaOptions(), [this] { return worldType(); });
    g.reportViolation = std::make_unique<gamewindow::TReportViolationController>();
    g.bugReport = std::make_unique<gamewindow::TBugReportDialogController>();
    g.naggingDialog = std::make_unique<gamewindow::TNaggingMessageDialogController>();
    g.deathDialog = std::make_unique<gamewindow::TDeathDialogController>();
    using Widgets = gamewindow::TSidebarWidgetsManager;
    sidebarWidgets->registerWidget(Widgets::Vip, Widgets::hooksFor(*vipWidget, [vipWidget] { return vipWidget->open(); }));
    g.preyWidget = std::make_unique<gamewindow::TKillTrackerWidgetManager>(informationDialogs->prey()->storage(), gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::Prey, Widgets::hooksFor(*g.preyWidget, [this, w = g.preyWidget.get()] { return w->open(sidebarOptions()); }));
    g.questTrackerWidget = std::make_unique<gamewindow::TQuestTrackerWidgetManager>(informationDialogs->questLog()->tracker(), gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::QuestTracker, Widgets::hooksFor(*g.questTrackerWidget, [this, w = g.questTrackerWidget.get()] { return w->open(sidebarOptions()); }));
    g.lootTrackingWidget = std::make_unique<gamewindow::TLootTrackingWidgetManager>(g.analyserStorages.get(), gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::LootTracker, Widgets::hooksFor(*g.lootTrackingWidget, [this, w = g.lootTrackingWidget.get()] { return w->open(sidebarOptions()); }));
    g.spellListWidget = std::make_unique<gamewindow::TSpellListWidgetManager>(gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::SpellList, Widgets::hooksFor(*g.spellListWidget, [this, w = g.spellListWidget.get()] { return w->open(sidebarOptions()); }));
    g.unjustifiedWidget = std::make_unique<gamewindow::TUnjustifiedPointsWidgetManager>(gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::UnjustifiedPoints, Widgets::hooksFor(*g.unjustifiedWidget, [this, w = g.unjustifiedWidget.get()] { return w->open(sidebarOptions()); }));
    g.imbuementTrackerWidget = std::make_unique<gamewindow::TImbuementTrackerWidgetManager>(gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::ImbuementTracker, Widgets::hooksFor(*g.imbuementTrackerWidget, [this, w = g.imbuementTrackerWidget.get()] { return w->open(sidebarOptions()); }));
    // The tracker widgets take the session's tracker storages (TSidebarWidgetsManager's connector).
    using CreatureMode = gamewindow::TCreatureTrackerWidgetController::EWidgetMode;
    g.bestiaryTrackerWidget = std::make_unique<gamewindow::TCreatureTrackerWidgetManager>(
        CreatureMode::Bestiary, nullptr, nullptr, gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::BestiaryTracker, Widgets::hooksFor(*g.bestiaryTrackerWidget, [this, w = g.bestiaryTrackerWidget.get()] { return w->open(sidebarOptions()); }));
    g.bosstiaryTrackerWidget = std::make_unique<gamewindow::TCreatureTrackerWidgetManager>(
        CreatureMode::Bosstiary, nullptr, nullptr, gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::BosstiaryTracker, Widgets::hooksFor(*g.bosstiaryTrackerWidget, [this, w = g.bosstiaryTrackerWidget.get()] { return w->open(sidebarOptions()); }));
    g.bossCooldownsWidget = std::make_unique<gamewindow::TBossTrackerWidgetManager>(nullptr, gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::BossCooldowns, Widgets::hooksFor(*g.bossCooldownsWidget, [this, w = g.bossCooldownsWidget.get()] { return w->open(sidebarOptions()); }));
    // DepotSearchWidget (sidebar); owns the onDepotSearch* hooks.
    g.depotSearchWidget = std::make_unique<gamewindow::TDepotSearchWidgetManager>(sidebarManager, gameWindowItem, engine);
    g.playerGuideWidget = std::make_unique<gamewindow::TPlayerGuideWidgetManager>(gameWindowItem, engine);
    sidebarWidgets->registerWidget(Widgets::PlayerGuide, Widgets::hooksFor(*g.playerGuideWidget, [this, w = g.playerGuideWidget.get()] { return w->open(sidebarOptions()); }));
    // Subscribers for gem atelier / helpers / RV channel / virtues yellow border.
    g.cipSoftUiEventBridge = std::make_unique<gamewindow::TCipSoftUiEventBridge>();
    g.analyserWidgets = std::make_unique<analysers::TAnalyserWidgets>(g.analyserStorages.get(), sidebarWidgets, gameWindowItem, engine,
                                                                      [this] { return sidebarOptions(); });
    g.proficiencyStorage = std::make_unique<proficiency::TProficiencyStorage>();
    g.weaponProficiencyDialog = std::make_unique<proficiency::TWeaponProficiencyDialogController>(g.proficiencyStorage.get());
    g.weaponProficiencyShapeDialog = std::make_unique<proficiency::TWeaponProficiencyShapeDialogController>(g.proficiencyStorage.get());
    g.weaponProficiencyReshapeDialog = std::make_unique<proficiency::TWeaponProficiencyReshapeDialogController>(
        g.proficiencyStorage.get(), g.weaponProficiencyShapeDialog.get());
    g.weaponProficiencyShapeOptionsDialog = std::make_unique<proficiency::TWeaponProficiencyShapeOptionsDialogController>();
    g.blessingsDialog = std::make_unique<gamewindow::TBlessingsDialogController>();
    g.rewardWallDialog = std::make_unique<gamewindow::TRewardWallDialogController>(g.dailyRewardStorage.get());
    g.taskboardDialog = std::make_unique<gamewindow::TTaskboardDialogController>();
    g.tutorialShell = std::make_unique<gamewindow::TTutorialShell>();
    connect(g.tutorialShell.get(), &gamewindow::TTutorialShell::tutorialFinished, this, &TGameClient::finishTutorialCinematic);
    if (gamewindow::TButtonBarController* buttonBar = sidebarPanels->buttonBar()) {
        connect(sidebarWidgets, &gamewindow::TSidebarWidgetsManager::widgetVisible, buttonBar,
                &gamewindow::TButtonBarController::onWidgetVisible);
        connect(sidebarWidgets, &gamewindow::TSidebarWidgetsManager::widgetHidden, buttonBar,
                &gamewindow::TButtonBarController::onWidgetHidden);
        connect(buttonBar, &gamewindow::TButtonBarController::logoutRequested, this,
                [this] { requestLogoutOrShowStartScreen(nullptr); });
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
                    g.vipWidget->setControlScheme(scheme);
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
    g.fpsLatency->setRenderProvider(worldMapRenderProvider());
    g.fpsLatency->setWidgetItem(gameWindowController->gameWindowChild(QStringLiteral("tibiaFpsLatencyIndicator")));

    g.chat = std::make_unique<gamewindow::TChatController>(gameWindowItem->findChild<QQuickItem*>(QStringLiteral("chat")), *this);
    gamewindow::TChatController* chat = g.chat.get();
    setMessageIgnoreList(&chat->ignoreList());

    g.actionBars = std::make_unique<gamewindow::TActionBarManager>(gameWindowItem, mapWindowPane, engine, [chat] { return chat->chatMode(); });
    gamewindow::TActionBarManager* actionBars = g.actionBars.get();
    connect(chat, &gamewindow::TChatController::chatModeChanged, actionBars, &gamewindow::TActionBarManager::onChatModeChanged);
    actionBars->setActionBarOptions(actionBarOptions());
    {
        config::TCharacterConfigurationManager::TStorages storages;
        storages.itemPrice = analysers::TItemPriceStorage::instance();
        storages.questLog = informationDialogs->questLog() ? informationDialogs->questLog()->questLogStorage() : nullptr;
        // The analysers and the Cyclopedia map configuration are the session's
        // (CipSoft hands them to the session's configuration manager).
        storages.quickLootBlackWhitelist = g.analyserStorages->lootBlackWhitelist();
        storages.skillWheel = g.skillWheel->storage();
        createCharacterConfigurationManager(storages);
        config::TCharacterConfigurationManager* manager = characterConfigurationManager();
        connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, actionBars,
                &gamewindow::TActionBarManager::onCharacterConfigurationLoaded);
        connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, g.analyserStorages.get(),
                &analysers::TAnalyserStorages::onCharacterConfigurationLoaded);
        if (gamewindow::TCyclopediaDialogController* cyclopedia = informationDialogs->cyclopedia())
            connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, cyclopedia->mapDialog(),
                    &gamewindow::TCyclopediaMapDialogController::showDialogTab);
        if (gamewindow::TQuestLogController* questLog = informationDialogs->questLog())
            connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, questLog,
                    &gamewindow::TQuestLogController::onCharacterConfigurationLoaded);
    }
    // After the action bars, so TCooldownStorage::instance exists.
    g.cooldownBar = std::make_unique<gamewindow::TCooldownBarController>(gameWindowItem);
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
    g.genericActions = std::make_unique<input::TGenericGameActionHandler>();
    input::TGenericGameActionHandler* genericActions = g.genericActions.get();
    connect(genericActions, &input::TGenericGameActionHandler::publishGameAction,
            gameActionRouter()->internalRouter(), &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(gameActionRouter()->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal,
            genericActions, &input::TGenericGameActionHandler::handleGameAction);
    game::TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.playerMovementActions.get(),
            &input::TPlayerMovementGameActionHandler::handleGameAction);
    connect(g.playerMovementActions.get(), &input::TPlayerMovementGameActionHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.clipboardActions.get(),
            &input::TClipboardGameActionHandler::handleGameAction);
    connect(g.clipboardActions.get(), &input::TClipboardGameActionHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
    g.useWithActions = std::make_unique<input::TUseWithGameActionHandler>();
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.useWithActions.get(),
            &input::TUseWithGameActionHandler::handleGameAction);
    connect(g.useWithActions.get(), &input::TUseWithGameActionHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
    // CIP connects the messages to the session's protocol queue.
    connect(g.useWithActions.get(), &input::TUseWithGameActionHandler::sendUseTwoObjects, this,
            [](const tibia::protobuf::protocol::GameclientMessageUseTwoObjects& message) {
                if (protocol::TProtocolMessageQueue* queue = protocol::protocolMessageQueue();
                    queue && tibia::game::canPerformGameAction())
                    queue->sendUseTwoObjects(message);
            });
    connect(g.useWithActions.get(), &input::TUseWithGameActionHandler::sendUseOnCreature, this,
            [](const tibia::protobuf::protocol::GameclientMessageUseOnCreature& message) {
                if (protocol::TProtocolMessageQueue* queue = protocol::protocolMessageQueue();
                    queue && tibia::game::canPerformGameAction())
                    queue->sendUseOnCreature(message);
            });
    g.crossHairSpellActions = std::make_unique<input::TCrossHairSpellGameActionHandler>();
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.crossHairSpellActions.get(),
            &input::TCrossHairSpellGameActionHandler::handleGameAction);
    connect(g.crossHairSpellActions.get(), &input::TCrossHairSpellGameActionHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
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
    publishToRouter(g.splitStackDialog.get());
    publishToRouter(g.storeDialog.get());
    publishToRouter(g.storeButton.get());
    publishToRouter(g.marketController.get());
    publishToRouter(g.stashDialog.get());
    publishToRouter(g.manageLootDialog.get());
    publishToRouter(g.exaltationForgeDialog.get());
    publishToRouter(g.screenshotManager.get());
    publishToRouter(g.minimapController);
    publishToRouter(g.deathDialog.get());
    publishToRouter(g.reportViolation.get());
    publishToRouter(g.weaponProficiencyDialog.get());
    publishToRouter(g.weaponProficiencyShapeDialog.get());
    publishToRouter(g.blessingsDialog.get());
    publishToRouter(g.rewardWallDialog.get());
    publishToRouter(g.optionsDialog.get());
    publishToRouter(g.chat.get());
    publishToRouter(g.actionBars.get());
    publishToRouter(g.sidebarWidgets.get());
    publishToRouter(g.vipWidget.get());
    publishToRouter(g.preyWidget.get());
    publishToRouter(g.questTrackerWidget.get());
    publishToRouter(g.lootTrackingWidget.get());
    publishToRouter(g.spellListWidget.get());
    publishToRouter(g.bestiaryTrackerWidget.get());
    publishToRouter(g.bosstiaryTrackerWidget.get());
    publishToRouter(g.bossCooldownsWidget.get());
    publishToRouter(g.depotSearchWidget.get());
    publishToRouter(g.playerGuideWidget.get());
    publishToRouter(g.sidebarPanels.get());
    publishToRouter(g.containers.get());
    publishToRouter(g.informationDialogs.get());
    publishToRouter(g.imbuingDialog.get());
    publishToRouter(g.exivaOptionsDialog.get());
    publishToRouter(g.taskboardDialog.get());
    publishToRouter(g.skillWheel->dialog());
    publishToRouter(g.skillWheel->actions());
    publishToRouter(g.exaltationStorage.get());
    publishToRouter(g.dailyRewardActions.get());
    publishToRouter(g.analyserStorages.get());
    publishToRouter(g.analyserWidgets.get());
    // The engine's text copy controller outlives the game windows.
    if (auto* textCopy = qobject_cast<gamewindow::TTextCopyController*>(
            engine->rootContext()->contextProperty(QStringLiteral("tibiaTextCopyController")).value<QObject*>()))
        connect(textCopy, &gamewindow::TTextCopyController::publishGameAction, router,
                &game::TInternalGameActionRouter::handleGameActionInternal, Qt::UniqueConnection);
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.openWebsiteActions.get(),
            &input::TOpenWebsiteGameActionHandler::handleGameAction);
    connect(genericActions, &input::TGenericGameActionHandler::sendQuitGame, this,
            [this] { requestLogoutOrShowStartScreen(nullptr); });
    connect(genericActions, &input::TGenericGameActionHandler::showCharacterSelectionDialog, this,
            [this] { openCharacterSelectionForSwitch(); });
    g.gatherAllActions = std::make_unique<input::TGatherAllGameActionHandler>();
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.gatherAllActions.get(),
            &input::TGatherAllGameActionHandler::handleGameAction);
    g.gatherAllProtocol = std::make_unique<input::TGatherAllProtocolMessageHandler>();
    g.eventFilterManager->enableDiscardingUnhandledMouseButtons();

    g.npcTrade = std::make_unique<trade::TNPCTradeStorage>();
    g.npcTradeMessages = std::make_unique<trade::TNPCTradeProtocolMessageHandler>(g.npcTrade.get());
    g.npcDialog = std::make_unique<gamewindow::TNpcDialogController>(g.npcTrade.get());
    g.npcDialog->setChatController(chat);
    publishToRouter(g.npcDialog.get());
    g.playerTrade = std::make_unique<gamewindow::TPlayerTradeWidgetManager>(sidebarManager, gameWindowItem, engine);

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
    handleFromRouter(g.splitStackDialog.get());
    handleFromRouter(g.storeDialog.get());
    handleFromRouter(g.marketController.get());
    handleFromRouter(g.stashDialog.get());
    handleFromRouter(g.manageLootDialog.get());
    handleFromRouter(g.exaltationForgeDialog.get());
    handleFromRouter(g.exaltationStorage.get());
    handleFromRouter(g.dailyRewardActions.get());
    handleFromRouter(g.minimapController);
    handleFromRouter(g.reportViolation.get());
    handleFromRouter(g.bugReport.get());
    handleFromRouter(g.naggingDialog.get());
    handleFromRouter(g.rewardWallDialog.get());
    handleFromRouter(g.optionsDialog.get());
    handleFromRouter(g.chat.get());
    handleFromRouter(g.actionBars.get());
    handleFromRouter(g.informationDialogs.get());
    handleFromRouter(g.exivaOptionsDialog.get());
    handleFromRouter(g.taskboardDialog.get());
    handleFromRouter(g.tutorialShell.get());
    handleFromRouter(g.vipWidget.get());
    handleFromRouter(g.weaponProficiencyDialog.get());
    handleFromRouter(g.weaponProficiencyShapeDialog.get());
    handleFromRouter(g.weaponProficiencyShapeOptionsDialog.get());
    handleFromRouter(g.lenshelpManager.get());
    handleFromRouter(g.skillWheel.get());
    handleFromRouter(g.npcDialog.get());
    handleFromRouter(g.playerTrade.get());
    handleFromRouter(g.sessionBootstrap.get());

    // Must outlive bootstrapAll: SuiteRuntime keeps PackHostWire* for deferred
    // slots (HUD chat switch, store/options/logout).
    g.packHost = std::make_unique<qtclient::PackHostWire>();
    qtclient::PackHostWire& packHost = *g.packHost;
    packHost.bindQuestTracker = [&g](mods::MissionBackend* backend, qtclient::BrowseOpenerFn browse) {
        g.questTrackerWidget->setMissionBackend(backend);
        g.questTrackerWidget->setBrowseOpener(std::move(browse));
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
    packHost.blessingsDialog = [&g]() -> QObject* { return g.blessingsDialog.get(); };
    packHost.npcDialog = [&g]() -> QObject* { return g.npcDialog.get(); };
    packHost.chatController = [chat]() -> QObject* { return chat; };
    packHost.chatStorage = [chat]() -> QObject* { return &chat->storage(); };
    packHost.storeMessages = [&g]() -> QObject* { return g.storeMessages.get(); };
    packHost.storeImageManager = [storeDialog]() -> QObject* {
        return storeDialog->property("dynamicallyLoadedImageManager").value<QObject*>();
    };
    packHost.minimapController = [&g]() -> QObject* { return g.minimapController; };
    packHost.sidebarManager = [sidebarManager]() -> QObject* { return sidebarManager; };
    packHost.sidebarWidgets = [sidebarWidgets]() -> QObject* { return sidebarWidgets; };
    packHost.containers = [containers]() -> QObject* { return containers; };
    packHost.buttonBar = [sidebarPanels]() -> QObject* { return sidebarPanels->buttonBar(); };
    packHost.imbuingDialog = [&g]() -> QObject* { return g.imbuingDialog.get(); };
    packHost.imbuingStorage = [&g]() -> QObject* { return g.imbuingStorage.get(); };
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

    connect(this, &TGameClient::mainWindowInitialized, gameWindowItem, [this, &g] {
        // Show the game window before the heavier binds. A throw later must not
        // leave the login mask hidden and the game window still off.
        g.gameWindowController->showGameScreen();
        try {
            g.mapWindowController->bindSession(textStorage(), effectStorage());
            g.vipWidget->setSidebarOptions(sidebarOptions());
            g.vipWidget->setWorldType(worldType());
            g.sidebarWidgets->setSidebarOptions(sidebarOptions());
            g.depotSearchWidget->setSidebarOptions(sidebarOptions());
            g.sidebarPanels->setSidebarOptions(sidebarOptions());
            if (gamewindow::TPremiumPanelController* premiumPanel = g.sidebarPanels->premiumPanel()) {
                premiumPanel->onSessionStarted(account().session.isPremium, account().session.premiumUntil);
                connect(g.storeMessages.get(), &store::TStoreProtocolMessageHandler::storeButtonDeeplinkChanged, premiumPanel,
                        &gamewindow::TPremiumPanelController::onStoreDeeplinkChanged, Qt::UniqueConnection);
            }
            g.skillWheel->setSessionPremium(account().session.isPremium);
            g.containers->setSidebarOptions(sidebarOptions());
            g.containers->onLoginRampUpFinished();
            g.sidebarWidgets->onLoginRampUpFinished();
            g.statusBarController->setStorage(statusbarStorage());
            g.informationDialogs->setAmbientLightStorage(ambientLight());
            g.informationDialogs->setMinimapTileStorage(minimap() ? minimap()->tileStorage() : nullptr);
            if (g.minimapController) {
                g.minimapController->setAmbientLightStorage(ambientLight());
                g.minimapController->setTileStorage(minimap() ? minimap()->tileStorage() : nullptr);
            }
            g.optionsDialog->setInGame(true);
            g.actionBars->onWorldEntered();
            qmlcomponents::applyOptionVolumes(); // stop login anthem; keep channel gains
        } catch (...) {
        }
    });
    connect(this, &TGameClient::worldLeft, gameWindowItem, [&g] {
        if (g.minimapController)
            g.minimapController->setTileStorage(nullptr);
        g.informationDialogs->setMinimapTileStorage(nullptr);
        g.sidebarPanels->setSidebarOptions(nullptr);
        g.sidebarWidgets->setSidebarOptions(nullptr);
        g.vipWidget->setSidebarOptions(nullptr);
        g.containers->setSidebarOptions(nullptr);
        g.statusBarController->setStorage(nullptr);
        g.optionsDialog->setInGame(false);
        g.actionBars->onWorldLeft();
        g.dialogStack->hideAll();
        g.gameWindowController->showStartScreen();
        qmlcomponents::applyOptionVolumes(); // resume login anthem when offline
    });
    connect(application, &QCoreApplication::aboutToQuit, this, [this, &g] {
        g.mapWindowController->unbind();
        if (g.minimapController)
            g.minimapController->setTileStorage(nullptr);
        g.informationDialogs->setMinimapTileStorage(nullptr);
        shutdown();
    });
    // start() activated the session before the controllers above created their
    // protocol message handlers (chat, store, market, trades, ...): link them now,
    // and connect the controllers made after it to it.
    if (game::TGameSessionBase* session = game::TGameSessionBase::active())
        session->relinkProtocolMessageHandlers();
    // Host: a replay's session starts before the game window; only the
    // controllers made here still need connecting (the client's world map
    // updater keeps following the running session).
    if (m_currentGameSession)
        connectGameWindowControllersToGameSession(m_currentGameSession);
    return true;
}

gamewindow::TGameSessionDisconnectReactionController* TGameClient::gameSessionDisconnectReactionController() const
{
    return m_gameWindow ? m_gameWindow->gameSessionDisconnectReaction.get() : nullptr;
}

void TGameClient::connectGameWindowToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    // Host: the client still holds the world map's render provider, frame
    // updater and message handler; they follow the session here until the
    // session owns them.
    m_updater.reset();
    m_messageHandler.reset();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_worldMapRenderProvider->setViewport(session ? session->worldMapViewport().get() : nullptr);
    if (session) {
        m_messageHandler = std::make_unique<chat::TGameMessageHandler>(*session->textStorage());
        m_messageHandler->setOptions(app::ClientOptions::load().options());
        m_updater = std::make_unique<renderer::TNextFrameUpdater>(*m_worldMapRenderProvider, *session->textStorage());
    }
    connectGameWindowControllersToGameSession(GameSession);
}

void TGameClient::connectGameWindowControllersToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    // CipSoft's order of the game window controllers' connectors.
    if (m_gameWindow) {
        TGameWindow& g = *m_gameWindow;
        if (g.mapWindowController)
            g.mapWindowController->connectToGameSession(GameSession);
        if (g.containers)
            g.containers->connectToGameSession(GameSession);
        if (g.chat)
            g.chat->connectToGameSession(GameSession);
        // Host shim: the analysers' aggregate follows the session's storages;
        // the drop tracker reports on the chat's channels.
        if (g.analyserStorages)
            g.analyserStorages->connectToGameSession(GameSession);
        if (session && g.chat)
            session->itemTrackingStorage()->setChatStorage(&g.chat->storage());
        if (g.sidebarWidgets)
            g.sidebarWidgets->connectToGameSession(GameSession);
        if (g.sidebarPanels)
            g.sidebarPanels->connectToGameSession(GameSession);
        if (g.gameSessionDisconnectReaction)
            g.gameSessionDisconnectReaction->connectToGameSession(GameSession);
        if (g.actionBars)
            g.actionBars->connectToGameSession(GameSession);
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
        if (g.stashDialog)
            g.stashDialog->connectToGameSession(GameSession);
        if (g.informationDialogs)
            if (gamewindow::THighscoresDialogController* highscores = g.informationDialogs->highscores())
                highscores->connectToGameSession(GameSession);
    }
    if (m_soundController)
        m_soundController->connectToGameSession(GameSession);
}

void TGameClient::onGameSessionDisconnected()
{
    if (!m_currentGameSession)
        return;
    // CIP: the dialogs that only make sense during the session close.
    if (gamewindow::TMultiOfflineTrainingDialogController* offlineTraining = gamewindow::TMultiOfflineTrainingDialogController::instance())
        offlineTraining->dialog()->close();
    if (m_gameWindow && m_gameWindow->weaponProficiencyShapeOptionsDialog)
        m_gameWindow->weaponProficiencyShapeOptionsDialog->dialog()->close();
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
