// The game client's game window: the start screen, the map, sidebars, chat,
// bars and every dialog controller of a game session, created once the
// client window exists (CIP: TGameClient creates them on its window-setup
// timer and keeps them as its members).
#include "client/gameclient.h"
#include "game/gameactionrouter.h"
#include "game/gamesessionbase.h"
#include "game/sessionhooks.h"

#include "analysers/analyserstorages.h"
#include "analysers/analyserwidgets.h"
#include "analysers/itempricestorage.h"
#include "app/clientoptions.h"
#include "app/sessionbootstrap.h"
#include "bossbestiary/bosstrackerstorage.h"
#include "client/gameapplication.h"
#include "config/tibiaconfig.h"
#include "config/characterconfigurationmanager.h"
#include "config/clientoptions.h"
#include "config/clientsettings.h"
#include "config/clientoptionsmodeswitcher.h"
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
#include "gamewindow/calendardialogcontroller.h"
#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/chatcontroller.h"
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
#include "gamewindow/hirelingnamechangecontroller.h"
#include "chat/gamemessagehandler.h"
#include "renderer/worldmaprenderprovider.h"
#include "sound/soundcontroller.h"
#include "worldmap/nextframetimestamppredictor.h"
#include "worldmap/worldmapviewport.h"
#include "gamewindow/imbuementtrackerwidgetcontroller.h"
#include "gamewindow/imbuingdialogcontroller.h"
#include "gamewindow/infobannercontroller.h"
#include "gamewindow/linkcopywarningdialogcontroller.h"
#include "gamewindow/servermodaldialogcontroller.h"
#include "prey/preygameactionhandler.h"
#include "prey/preyprotocolmessagehandler.h"
#include "qmlcomponents/minimaprendererquickitem.h"
#include "gamewindow/killtrackerwidgetcontroller.h"
#include "gamewindow/loottrackingwidgetcontroller.h"
#include "gamewindow/managelootcontainerdialogcontroller.h"
#include "gamewindow/mapwindowcontroller.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/minimapcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/newsdialogcontroller.h"
#include "news/eventsuploader.h"
#include "news/newsciptypes.h"
#include "news/newsstorage.h"
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
#include "sound/soundconfiguration.h"
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
#include "gamewindow/socialdialogcontroller.h"
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
#include "imageproviders/imagestripimageprovider.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/domainhandlers.h"
#include "input/eventfiltermanager.h"
#include "input/eventfilters.h"
#include "input/gameactionhandlers.h"
#include "input/gameactionmapper.h"
#include "input/genericgameactionhandler.h"
#include "input/keyboardshortcuthandler.h"
#include "input/keyboardstatusstorage.h"
#include "input/keyboardinputtogameactionconverter.h"
#include "input/keyeventhandler.h"
#include "input/playermovementinputinterpreter.h"
#include "input/recordkeyboardshortcuthandler.h"
#include "mods/pack_host_abi.h"
#include "mods/registry.h"
#include "platform/capabilities.h"
#include "proficiency/proficiencystorage.h"
#include "proficiency/weaponproficiencydialogcontroller.h"
#include "proficiency/weaponproficiencyshapedialogs.h"
#include "qmlcomponents/soundhelper.h"
#include "screenshot/screenshotciptypes.h"
#include "skillwheel/skillwheelciptypes.h"
#include "skillwheel/skillwheelcontrollers.h"
#include "spells/spellinfostorage.h"
#include "store/dynamicallyloadedimage.h"

#include "clockservice.h"

#include <array>
#include <QGuiApplication>
#include <qtclient/pack_host.h>
#include <tibia/game/gamenotify.h>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/mapsession.h"
#include <client/outfit.h>
#include <client/position.h>
#include <tibia/assets/things.h>

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

#include <algorithm>
#include <any>
#include <exception>

namespace tibia {
namespace client {

namespace {

/// Host: the game's end empties the quest log.
void clearQuestLogOnGameEnd(gamewindow::TQuestLogController* questLog)
{
    QObject::connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, questLog,
                     [questLog] { questLog->clear(); });
}

}

/// The game window's objects (destroyed in reverse of their declaration: the
/// pack wiring first, the out-of-date dialog last).
struct TGameClient::TGameWindow
{
    app::LauncherEnv env;
    /// Host: the game actions the game window handles until their CipSoft
    /// handler hears the router (the quest log).
    input::TGameActionDispatch gameActions;
    std::shared_ptr<gamewindow::TGameWindowController> gameWindowController;
    std::unique_ptr<QObject> gameWindow;
    QQuickItem* gameWindowItem = nullptr;
    QQuickItem* dialogPlaceholder = nullptr;
    QQuickItem* mapWindowPane = nullptr;
    std::unique_ptr<gamewindow::TSidebarManager> sidebarManager;
    std::unique_ptr<QObject> productHost;
    /// Child of the map window pane.
    gamewindow::TMapWindowController* mapWindowController = nullptr;
    std::unique_ptr<app::TSessionBootstrap> sessionBootstrap;
    std::unique_ptr<analysers::TAnalyserStorages> analyserStorages;
    /// CIP: the button bar's Cyclopedia and Bosstiary buttons (held here
    /// until the button bar makes them).
    std::unique_ptr<gamewindow::TDragAndDropController> dragAndDrop;
    /// CIP: the game window controller's shared image downloader (store, social, news, …).
    std::shared_ptr<store::TDynamicallyLoadedImageManager> dynamicallyLoadedImageManager;
    std::unique_ptr<gamewindow::TManageLootContainerDialogController> manageLootDialog;
    std::unique_ptr<gamewindow::TImbuingDialogController> imbuingDialog;
    std::unique_ptr<skillwheel::TSkillWheelDialogController> skillWheelDialog;
    std::unique_ptr<screenshot::TScreenshotManager> screenshotManager;
    std::unique_ptr<gamewindow::TSidebarPanelsManager> sidebarPanels;
    std::unique_ptr<gamewindow::TSidebarWidgetsManager> sidebarWidgets;
    gamewindow::TMinimapController* minimapController = nullptr;
    std::unique_ptr<gamewindow::TContainerStorageController> containers;
    std::unique_ptr<exivaoptions::TExivaOptionsDialogController> exivaOptionsDialog;
    std::unique_ptr<analysers::TAnalyserWidgets> analyserWidgets;
    std::unique_ptr<gamewindow::TTutorialShell> tutorialShell;
    std::unique_ptr<gamewindow::TTutorialOverlayController> tutorialOverlay;
    std::unique_ptr<gamewindow::TOptionsMenuDialogController> optionsDialog;
    std::unique_ptr<gamewindow::TFPSLatencyIndicatorController> fpsLatency;
    std::unique_ptr<gamewindow::TChatController> chat;
    std::unique_ptr<gamewindow::TActionBarManager> actionBars;
    std::unique_ptr<gamewindow::TTargetSelectionController> targetSelection;
    std::unique_ptr<gamewindow::TCooldownBarController> cooldownBar;
    std::unique_ptr<gamewindow::TNpcDialogController> npcDialog;
    /// CIP: the minimap marker, hireling name, character trade and inspection
    /// dialogs (CipSoft's client members, made by showGameWindow).
    std::unique_ptr<gamewindow::TEditMinimapMarkerDialogController> editMinimapMarkerDialog;
    std::unique_ptr<gamewindow::THirelingNameChangeController> hirelingNameChange;
    std::unique_ptr<gamewindow::TCharacterTradeDialogController> characterTradeDialog;
    std::unique_ptr<gamewindow::TInspectObjectDialogController> inspectObjectDialog;
    std::unique_ptr<gamewindow::TInspectPlayerDialogController> inspectPlayerDialog;
    /// CIP: the social dialog (friends and teams; its button is the button bar's).
    std::unique_ptr<gamewindow::TSocialDialogController> socialDialog;
    std::unique_ptr<gamewindow::TWhiteBlacklistDialogController> whiteBlacklistDialog;
    /// CIP: the event schedule (its uploader and storage), the calendar and the news dialog.
    std::shared_ptr<news::TEventsUploader> eventsUploader;
    std::shared_ptr<news::TEventsStorage> eventsStorage;
    std::unique_ptr<gamewindow::TCalendarDialogController> calendarDialog;
    std::unique_ptr<gamewindow::TNewsDialogController> newsDialog;
    /// CIP: the client options mode switcher (the tutorial's options and back).
    std::unique_ptr<config::TClientOptionsModeSwitcher> clientOptionsModeSwitcher;
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
    if (m_gameWindow->actionBars && m_currentGameSession)
        m_gameWindow->actionBars->tick(m_currentGameSession->clockService()->nowMillis());
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
    gamewindow::TMessageDialogController* const exitGameDialog = m_pExitGameDialogController.get();
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
    m_pExitGameDialogController->dialog()->close();
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
    // The preview runs no engine: the window is only kept and the QML context
    // gets the client's objects (with the client's own objects). Otherwise
    // initializeWithStartMode ran the client's setup steps and the appearances
    // have arrived.
    if (preview) {
        m_pEngine = engine;
        m_options = setup.options;
        m_pMainWindow = window;
        imageproviders::registerImageProviders(engine);
        initializeQmlContext();
    }
    QCoreApplication* application = QCoreApplication::instance();


    // CIP: the game window controller: the start screen until the world is
    // entered, then the map, sidebars, chat and bars. Dialogs go into its
    // dialogPlaceholder.
    // CIP: the [URLS] of config.ini. Host: the launcher/portal environment's
    // urls win (as the login url).
    const auto launcherOr = [](const QUrl& launcher, const QUrl& configured) { return launcher.isEmpty() ? configured : launcher; };
    const QUrl clientWebService = launcherOr(g.env.clientServicesUrl(), tibiaConfig()->clientWebServiceUrl());
    const QUrl accountCreationClientServices = launcherOr(g.env.createAccountUrl(), tibiaConfig()->accountCreationClientServicesUrl());
    const QUrl hints = g.env.assetBase.isEmpty() ? tibiaConfig()->hintsUrl() : QUrl(g.env.assetBase.toString() + QStringLiteral("/hints.json"));
    g.gameWindowController = std::make_shared<gamewindow::TGameWindowController>(
        gameActionRouter(), appearanceInstanceFactory(), engine, clientOptions(), setup.placeholder, m_pEventFilterManager,
        hints, clientWebService,
        tibiaFileSystemHelper(), shared::TClockService::process());
    gamewindow::TGameWindowController* gameWindowController = g.gameWindowController.get();
    // Host: kill / SIGTERM / Ctrl+C: same Exit / Logout / Cancel prompt as the window close.
    setForceExitHandler([gameWindowController] { gameWindowController->quitPressed(); });
    m_gameWindowPlaceholder = setup.placeholder;
    g.gameWindow = gameWindowController->createGameWindow(engine);
    g.gameWindowItem = qobject_cast<QQuickItem*>(g.gameWindow.get());
    QQuickItem* gameWindowItem = g.gameWindowItem;
    if (!gameWindowItem)
        return false;
    // CIP: the client window becomes the main window once the game window is built
    // (sprite texture providers, the game action router, mainWindowInitialized).
    if (!preview)
        setMainWindow(window);
    g.dialogPlaceholder = gameWindowController->gameWindowChild(QStringLiteral("dialogPlaceholder"));
    g.mapWindowPane = gameWindowController->gameWindowChild(QStringLiteral("mapWindowPane"));
    QQuickItem* mapWindowPane = g.mapWindowPane;
    // CIP: the dialog controllers' parent item is the game window's dialog placeholder.
    QQuickItem* const dialogParentItem = g.dialogPlaceholder;
    // CIP: an element asking for the keyboard takes it from the chat input;
    // letting go gives it back.
    connect(m_pEventFilterManager.get(), &input::TEventFilterManager::requestElementFocus, gameWindowController,
            &gamewindow::TGameWindowController::releaseFocusFromChatInput);
    connect(m_pEventFilterManager.get(), &input::TEventFilterManager::releaseElementFocus, gameWindowController,
            &gamewindow::TGameWindowController::bindFocusToChatInput);
    // CIP: the game window's Create Account and Quit go to the client; Manage
    // Clients opens the launcher.
    connect(gameWindowController, &gamewindow::TGameWindowController::createAccountRequested, this,
            &TGameClient::onRequestCreateAccount);
    connect(gameWindowController, &gamewindow::TGameWindowController::gameExitRequested, this,
            &TGameClient::onExitClientRequested);
    connect(gameWindowController, &gamewindow::TGameWindowController::openLauncherRequested, this,
            &TGameClient::onOpenLauncherRequested);

    // Host: the game window's own wiring (client services, the start screen
    // and game screen key handling, the drag icon, the dialog stack, the
    // context menu, the analysers' storages) and the packs' early bootstrap,
    // before the game window controllers.
    gameWindowController->configureClientServices(clientWebService, g.env.revision);
    gameWindowController->showStartScreen();
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
    // CIP: the process-wide context menu over the window, with the event
    // filter manager (released by the client's destructor).
    gamewindow::TContextMenuController::createInstance(engine, window->contentItem(), m_pEventFilterManager);
    // Host: the client's dialog stack makes its dialogs in the game window's
    // dialog placeholder.
    gamewindow::TDialogStackController* dialogStack = m_pDialogStackController.get();
    dialogStack->setDialogPlaceholder(engine, g.dialogPlaceholder);
    // TEventFilterManager emits the chat-input focus signals while a dialog is up, so dialog inputs get the focus.
    connect(dialogStack, &gamewindow::TDialogStackController::dialogVisible, gameWindowController,
            [gameWindowController](bool visible) {
                if (visible)
                    Q_EMIT gameWindowController->releaseFocusFromChatInput();
                else
                    Q_EMIT gameWindowController->bindFocusToChatInput();
            });
    // Before the dialogs: the Cyclopedia item page and the loot widgets read its item storages.
    g.analyserStorages = std::make_unique<analysers::TAnalyserStorages>(m_pItemPriceStorage, m_pPremiumTimeStorage);
    g.dragAndDrop = std::make_unique<gamewindow::TDragAndDropController>(
        window->contentItem(), gameWindowController->gameWindowChild(QStringLiteral("dragApperanceInstanceIcon")),
        appearanceInstanceFactory());
    g.dragAndDrop->registerContextProperty(engine->rootContext());

    // CIP: lenshelp mode and its popup over the game window's main frame.
    m_pLenshelpManager = std::make_unique<qmlcomponents::TLenshelpManager>(
        engine, gameWindowController->gameWindowChild(QStringLiteral("mainFrame")));
    // CIP: the map window controller.
    const bool softwareRenderer = window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
    g.mapWindowController = new gamewindow::TMapWindowController(engine, mapWindowPane, clientOptions(), m_pGameActionMapper,
                                                                 m_pLenshelpManager.get(), softwareRenderer, mapWindowPane);
    gamewindow::TMapWindowController* mapWindowController = g.mapWindowController;
    mapWindowController->syncFov();
    // CIP: the chat controller.
    g.chat = std::make_unique<gamewindow::TChatController>(
        static_cast<uint>(config::clientSettingAtLeast(QStringLiteral("CHAT"), QStringLiteral("maxChatInputHistoryLength"), 1000, 1)),
        dialogParentItem, gameWindowItem->findChild<QQuickItem*>(QStringLiteral("chat")), engine, sharedClientOptions(),
        m_pGameActionMapper, tibiaFileSystemHelper(), shared::TClockService::process(), m_pTibiaUrlTools, m_pChatModeManager);
    gamewindow::TChatController* chat = g.chat.get();
    // CIP: the client's white/blacklist tells the chat about filtered messages.
    connect(whiteBlacklist().get(), &chat::TWhiteBlacklist::messageFiltered, chat, &gamewindow::TChatController::onMessageFiltered);
    // CIP: the action bar manager takes the client options, the client's
    // premium time storage and the spell table; the three groups (bottom,
    // left, right) are made here and added to it.
    g.actionBars = std::make_unique<gamewindow::TActionBarManager>(clientOptions(),
                                                                   m_pPremiumTimeStorage.get(), spellInfoStorage());
    {
        const std::array<std::pair<gamewindow::EActionBarPosition, const char*>, 3> actionBarContainers{
            { { gamewindow::EActionBarPosition::ActionBarPositionBottom, "bottomActionBars" },
              { gamewindow::EActionBarPosition::ActionBarPositionLeft, "leftActionBars" },
              { gamewindow::EActionBarPosition::ActionBarPositionRight, "rightActionBars" } }
        };
        for (const auto& [position, name] : actionBarContainers) {
            QQuickItem* container = gameWindowItem ? gameWindowItem->findChild<QQuickItem*>(QLatin1String(name)) : nullptr;
            g.actionBars->addGroup(std::make_shared<gamewindow::TActionBarGroup>(container, engine, dialogParentItem, mapWindowPane,
                                                                                 clientOptions(),
                                                                                 &chat->chatModeManager(), spellInfoStorage(),
                                                                                 passiveAbilityInfoStorage(), position));
        }
    }
    // CIP: the target selection controller, published in the root context.
    g.targetSelection = std::make_unique<gamewindow::TTargetSelectionController>(gameWindowItem);
    g.targetSelection->registerInQmlContext(engine->rootContext());
    // CIP: the cooldown bar.
    g.cooldownBar = std::make_unique<gamewindow::TCooldownBarController>(
        spellInfoStorage(), gameWindowItem ? gameWindowItem->findChild<QQuickItem*>(QStringLiteral("cooldownBar")) : nullptr);
    // CIP: the sidebar manager; the map window's unused width sizes the
    // sidebars and the client hears the sidebar count.
    g.sidebarManager = std::make_unique<gamewindow::TSidebarManager>(
        engine, gameWindowController->gameWindowChild(QStringLiteral("rightSidebarPlaceholder")),
        gameWindowController->gameWindowChild(QStringLiteral("rightSidebarManager")),
        gameWindowController->gameWindowChild(QStringLiteral("leftSidebarPlaceholder")),
        gameWindowController->gameWindowChild(QStringLiteral("leftSidebarManager")));
    gamewindow::TSidebarManager* sidebarManager = g.sidebarManager.get();
    connect(mapWindowController, &gamewindow::TMapWindowController::unusedWidthChanged, sidebarManager,
            &gamewindow::TSidebarManager::onUnusedWidthChanged);
    connect(sidebarManager, &gamewindow::TSidebarManager::numberOfSidebarsChanged, this,
            &TGameClient::numberOfSidebarsChanged);
    mapWindowController->onUnusedWidthChanged();
    // Host: the manager already emitted before this connection, and the split
    // view lays out again once the minimum width changed.
    const auto correctLayout = [window] {
        if (QObject* split = window->findChild<QObject*>(QStringLiteral("mainFrame")))
            QTimer::singleShot(0, split, [split] { QMetaObject::invokeMethod(split, "correctLayout"); });
    };
    Q_EMIT numberOfSidebarsChanged(sidebarManager->sidebarIds().size());
    correctLayout();
    connect(sidebarManager, &gamewindow::TSidebarManager::numberOfSidebarsChanged, window, correctLayout);

    // CIP: the authentication process controller; its login state reaches
    // the game window controller.
    createAuthenticationProcessController(g.env.loginUrl());
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::authenticatedChanged,
            gameWindowController, &gamewindow::TGameWindowController::setAuthenticated);
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::quitAndPatchClient, this,
            &TGameClient::onTryToPatchClient);
    // The preview shows its own character list.
    gamewindow::TCharacterSelectionController* const characterSelectionController =
        preview ? setup.characterSelection : characterSelection();
    // Host: the start screen's login mask follows the account.
    if (!preview) {
        connect(this, &TGameClient::characterSelectionRequested, gameWindowController,
                [gameWindowController] { gameWindowController->setAuthenticated(true); });
        connect(this, &TGameClient::returnToLoginRequested, gameWindowController,
                [gameWindowController] { gameWindowController->setAuthenticated(false); });
        // Start screen with the account still "authenticated" hides the login mask.
        // If the character list is also closed (dropped switch, refused reconnect),
        // put one of those surfaces back. Deferred so a logout that opens the list
        // in the same turn is left alone.
        connect(gameWindowController, &gamewindow::TGameWindowController::gameWindowStateChanged, gameWindowController,
                [this, characterSelectionController, gameWindowController] {
                    if (!gameWindowController->property("gameWindowState").toString().isEmpty())
                        return;
                    QTimer::singleShot(0, gameWindowController, [this, characterSelectionController, gameWindowController] {
                        const game::TGameSessionBase* session = game::currentGameSession();
                        if ((session && (session->isOnline() || session->isSwitchingCharacter()))
                            || characterSelectionController->dialog()->isShown())
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
    } else {
        gameWindowController->setAuthenticated(true);
        // The preview shows its sample characters (without the auto-close of open()).
        characterSelectionController->dialog()->show();
    }
    connect(this, &TGameClient::sessionEmailAvailable, gameWindowController,
            &gamewindow::TGameWindowController::rememberLoginEmailIfEnabled);

    // CIP: the options menu.
    g.optionsDialog = std::make_unique<gamewindow::TOptionsMenuDialogController>(
        clientOptions(), slidingFPSCounter(), tibiaFileSystemHelper(), m_pEventFilterManager.get(), appearanceInstanceFactory(),
        minimapTileDiskIO(), minimapTileStorage(), spellInfoStorage(), engine, dialogParentItem);
    gamewindow::TOptionsMenuDialogController* optionsDialog = g.optionsDialog.get();
    optionsDialog->setGameClient(this);
    optionsDialog->setPortalBase(g.env.loginBase);
    // CIP: the client's dialog controllers, in showGameWindow's order (each
    // with the engine and the game window controller's dialog parent item).
    m_pOutfitMemorialDialogController = std::make_unique<gamewindow::TOutfitMemorialDialogController>(engine, dialogParentItem);
    m_pMarketController = std::make_unique<gamewindow::TMarketController>(clientOptions(), engine, dialogParentItem);
    g.editMinimapMarkerDialog =
        std::make_unique<gamewindow::TEditMinimapMarkerDialogController>(minimapMarkerStorage().get(), engine, dialogParentItem);
    m_pSelectOutfitDialogController = std::make_unique<gamewindow::TSelectOutfitDialogController>(
        appearanceInstanceFactory(), clientOptions(), engine, dialogParentItem);
    m_pConfigureCreaturePodiumDialogController = std::make_unique<gamewindow::TConfigureCreaturePodiumDialogController>(
        appearanceInstanceFactory(), clientOptions(), engine, dialogParentItem);
    g.hirelingNameChange = std::make_unique<gamewindow::THirelingNameChangeController>(engine, dialogParentItem);
    m_pPreyDialogController = std::make_unique<gamewindow::TPreyDialogController>(clientOptions(), engine, dialogParentItem);
    m_pRewardWallDialogController =
        std::make_unique<gamewindow::TRewardWallDialogController>(shared::TClockService::process(), engine, dialogParentItem);
    g.manageLootDialog = std::make_unique<gamewindow::TManageLootContainerDialogController>(engine, dialogParentItem, clientOptions());
    g.exivaOptionsDialog = std::make_unique<exivaoptions::TExivaOptionsDialogController>(engine, g.dialogPlaceholder);
    g.exivaOptionsDialog->setWorldType([this] { return worldType(); });
    g.characterTradeDialog = std::make_unique<gamewindow::TCharacterTradeDialogController>(engine, dialogParentItem);
    m_pExaltationForgeDialogController =
        std::make_unique<gamewindow::TExaltationForgeDialogController>(clientOptions(), engine, dialogParentItem);
    // CIP: the skill wheel dialog with the client's planner url ([URLS] wheelOfDestinyPlannerDirectUrl; the
    // portal url handler maps the tibia.com hosts, www.test.tibia.com among them).
    g.skillWheelDialog = std::make_unique<skillwheel::TSkillWheelDialogController>(
        tibiaConfig()->wheelOfDestinyPlannerDirectUrl().toString(), engine, dialogParentItem);
    m_pWeaponProficiencyDialogController = std::make_unique<proficiency::TWeaponProficiencyDialogController>(
        clientOptions(), proficiencyStorage(), engine, dialogParentItem);
    m_pWeaponProficiencyShapeDialogController =
        std::make_unique<proficiency::TWeaponProficiencyShapeDialogController>(proficiencyStorage(), engine, dialogParentItem);
    m_pWeaponProficiencyShapeOptionsDialogController =
        std::make_unique<proficiency::TWeaponProficiencyShapeOptionsDialogController>(engine, dialogParentItem);
    m_pWeaponProficiencyReshapeDialogController = std::make_unique<proficiency::TWeaponProficiencyReshapeDialogController>(
        proficiencyStorage(), engine, dialogParentItem, m_pWeaponProficiencyShapeDialogController.get());
    // CIP: the event schedule from the client services, its storage and the
    // calendar that shows it.
    g.eventsUploader = news::TEventsUploader::create(clientWebService, tibiaFileSystemHelper(),
                                                     shared::TClockService::process());
    g.eventsStorage = std::make_shared<news::TEventsStorage>();
    g.calendarDialog = std::make_unique<gamewindow::TCalendarDialogController>(shared::TClockService::process(), g.eventsStorage,
                                                                               engine, dialogParentItem);
    m_pHighscoresDialogController = std::make_unique<gamewindow::THighscoresDialogController>(
        clientOptions(), shared::TClockService::process(), engine, dialogParentItem);
    // CIP: the create-account and create-character dialogs (both with the
    // account service of the login's portal).
    m_pCreateAccountDialogController = std::make_unique<gamewindow::TCreateAccountDialogController>(
        engine, dialogParentItem, tibiaFileSystemHelper(), accountCreationClientServices,
        tibiaConfig()->readUrl(QStringLiteral("accountCreationClientServicesRecaptchaV2Content"), QUrl()),
        tibiaConfig()->readUrl(QStringLiteral("accountCreationClientServicesRecaptchaV3Content"), QUrl()));
    m_pCreateAccountDialogController->setTooltipContainer(g.dialogPlaceholder);
    m_pCreateCharacterDialogController = std::make_unique<gamewindow::TCreateCharacterDialogController>(
        engine, dialogParentItem, tibiaFileSystemHelper(), m_authentication, accountCreationClientServices);
    // CIP: the start screen asks for the event schedule; a new schedule goes
    // to the events storage and the start screen's preview.
    // Host: the browser asks the client services only once the account is
    // logged in (the start screen of a fresh page stays offline).
    if (tibia::platform::kStartScreenServicesBeforeLogin) {
        connect(gameWindowController, &gamewindow::TGameWindowController::startScreenNowDisplayed, g.eventsUploader.get(),
                &news::TEventsUploader::requestUpdate);
    } else {
        news::TEventsUploader* const eventsUploader = g.eventsUploader.get();
        const auto requestWhenAuthenticated = [gameWindowController, eventsUploader] {
            if (gameWindowController->property("isAuthenticated").toBool())
                eventsUploader->requestUpdate();
        };
        connect(gameWindowController, &gamewindow::TGameWindowController::startScreenNowDisplayed, eventsUploader,
                requestWhenAuthenticated);
        connect(gameWindowController, &gamewindow::TGameWindowController::authenticatedStateChanged, eventsUploader,
                requestWhenAuthenticated);
    }
    connect(g.eventsUploader.get(), &news::TEventsUploader::updated, g.eventsStorage.get(),
            &news::TEventsStorage::onEventsUpdated);
    connect(g.eventsUploader.get(), &news::TEventsUploader::updated, gameWindowController,
            &gamewindow::TGameWindowController::onEventScheduleUpdated);
    // CIP: the client's message boxes, after the create-character dialog: the
    // connecting dialog (Cancel aborts the connection), the exit confirmation
    // (its buttons answer onConfirmOrCancelExitGame) and the login advice.
    m_pConnectingDialogController = std::make_unique<gamewindow::TMessageDialogController>(engine, g.dialogPlaceholder);
    connect(m_pConnectingDialogController.get(), &gamewindow::TMessageDialogController::discardClicked, this,
            &TGameClient::onAbortConnectClientToGameserver);
    // CIP: a message box whose OK goes back to the character selection.
    m_pCharacterSelectionMessageDialogController = std::make_unique<gamewindow::TMessageDialogController>(engine, g.dialogPlaceholder);
    m_pCharacterSelectionMessageDialogController->setCloseOnButtonClicked(true);
    connect(m_pCharacterSelectionMessageDialogController.get(), &gamewindow::TMessageDialogController::acceptClicked, this,
            &TGameClient::onShowCharacterSelectionDialogRequested);
    m_pExitGameDialogController = std::make_unique<gamewindow::TMessageDialogController>(engine, g.dialogPlaceholder);
    connect(m_pExitGameDialogController.get(), &gamewindow::TMessageDialogController::buttonClicked, this,
            &TGameClient::onConfirmOrCancelExitGame);
    m_pLoginAdviceDialogController = std::make_unique<gamewindow::TMessageDialogController>(engine, g.dialogPlaceholder);
    m_pLoginAdviceDialogController->setCloseOnButtonClicked(true);
    // CIP: the invalid configuration notice, the client check (its buttons
    // reach the client) and a plain message box.
    m_pConfigurationInvalidDialogController = std::make_unique<gamewindow::TMessageDialogController>(engine, g.dialogPlaceholder);
    m_pConfigurationInvalidDialogController->setCloseOnButtonClicked(true);
    m_pClientCheckDialogController = std::make_unique<gamewindow::TMessageDialogController>(engine, g.dialogPlaceholder);
    m_pClientCheckDialogController->setCloseOnButtonClicked(true);
    m_pMessageDialogController = std::make_unique<gamewindow::TMessageDialogController>(engine, g.dialogPlaceholder);
    // Host: session chrome (WaitDialog, errors, create account/character,
    // keep_renderer), once the message boxes exist.
    if (!preview)
        g.sessionBootstrap = std::make_unique<app::TSessionBootstrap>(this, g.env, gameWindowController, g.productHost.get());
    m_pQuestLogController = std::make_unique<gamewindow::TQuestLogController>(engine, dialogParentItem);
    m_pServerModalDialogController = std::make_unique<gamewindow::TServerModalDialogController>(engine, dialogParentItem);
    // CIP: the disconnect reaction, before the first session: it reacts to
    // the session's connection and login; its login again reconnects.
    m_pGameSessionDisconnectReactionController =
        std::make_shared<gamewindow::TGameSessionDisconnectReactionController>(m_authentication, g.gameWindowController, engine,
                                                                               dialogParentItem);
    connect(m_pGameSessionDisconnectReactionController.get(),
            &gamewindow::TGameSessionDisconnectReactionController::loginAgainAfterLoginWaitElapsed, this,
            &TGameClient::connectClientToGameserverWithExistingCredentials, Qt::QueuedConnection);
    connect(m_pGameSessionDisconnectReactionController.get(), &gamewindow::TGameSessionDisconnectReactionController::quitAndPatchClient,
            this, &TGameClient::onTryToPatchClient);
    m_pReportViolationController =
        std::make_unique<gamewindow::TReportViolationController>(shared::TClockService::process(), engine, dialogParentItem);
    // CIP: the white/blacklist dialog over the client's list.
    g.whiteBlacklistDialog =
        std::make_unique<gamewindow::TWhiteBlacklistDialogController>(engine, dialogParentItem, whiteBlacklist());
    // CIP: the tutorial hint and the vocation selection dialogs (host: in the
    // tutorial shell).
    g.tutorialShell = std::make_unique<gamewindow::TTutorialShell>(engine, g.dialogPlaceholder, mapWindowPane,
                                                                   cyclopediaMapStaticDataStorage());
    connect(g.tutorialShell.get(), &gamewindow::TTutorialShell::tutorialFinished, this, &TGameClient::finishTutorialCinematic);
    m_pInfoBannerController = std::make_unique<gamewindow::TInfoBannerController>(clientOptions(), engine, mapWindowPane, mapWindowPane);
    m_pMultiOfflineTrainingDialogController =
        std::make_unique<gamewindow::TMultiOfflineTrainingDialogController>(engine, dialogParentItem);
    // CIP: the FPS and latency indicator shows in the game window's indicator item.
    g.fpsLatency = std::make_unique<gamewindow::TFPSLatencyIndicatorController>(clientOptions());
    g.fpsLatency->setWindow(window);
    g.fpsLatency->setFrameDurations(frameDurations());
    g.fpsLatency->setWidgetItem(gameWindowController->gameWindowChild(QStringLiteral("tibiaFpsLatencyIndicator")));
    m_pEditTextController = std::make_unique<gamewindow::TEditTextController>(engine, dialogParentItem);
    m_pDeathDialogController = std::make_unique<gamewindow::TDeathDialogController>(engine, dialogParentItem);
    g.inspectObjectDialog = std::make_unique<gamewindow::TInspectObjectDialogController>(clientOptions(), engine, dialogParentItem);
    g.inspectPlayerDialog = std::make_unique<gamewindow::TInspectPlayerDialogController>(clientOptions(), engine, dialogParentItem);
    m_pLinkCopyWarningDialogController = std::make_unique<gamewindow::TLinkCopyWarningDialogController>(engine, dialogParentItem);
    // CIP: the image downloader the store (and the news) load their images with.
    g.dynamicallyLoadedImageManager = std::make_shared<store::TDynamicallyLoadedImageManager>();
    // CIP: the news dialog over the client's news storage; it asks the
    // client services for the news itself.
    g.newsDialog = std::make_unique<gamewindow::TNewsDialogController>(
        clientWebService, newsStorage(), clientOptions(), m_authentication, tibiaFileSystemHelper(),
        shared::TClockService::process(), engine, dialogParentItem);
    m_pStoreDialogController = std::make_unique<gamewindow::TStoreDialogController>(g.dynamicallyLoadedImageManager,
                                                                                    clientOptions(), engine, dialogParentItem);
    gamewindow::TStoreDialogController* storeDialog = m_pStoreDialogController.get();
    m_pBugReportDialogController = std::make_unique<gamewindow::TBugReportDialogController>(engine, dialogParentItem);
    // CIP: the crash report dialog; its Send reaches the client.
    m_crashReportDialog = std::make_unique<gamewindow::TCrashReportDialogController>(engine, dialogParentItem);
    connect(m_crashReportDialog.get(), &gamewindow::TCrashReportDialogController::requestSendCrashReport, this,
            &TGameClient::sendCrashReport);
    g.tutorialOverlay = std::make_unique<gamewindow::TTutorialOverlayController>(
        engine, gameWindowController->gameWindowChild(QStringLiteral("tutorialOverlay")), setup.placeholder,
        m_pEventFilterManager.get());
    if (window)
        connect(window, &QQuickWindow::afterAnimating, g.tutorialOverlay.get(),
                &gamewindow::TTutorialOverlayController::update);
    g.imbuingDialog = std::make_unique<gamewindow::TImbuingDialogController>(
        appearanceInstanceFactory(), clientOptions(), engine, dialogParentItem);
    m_pBlessingsDialogController = std::make_unique<gamewindow::TBlessingsDialogController>(engine, dialogParentItem);
    m_pCyclopediaDialogController = std::make_unique<gamewindow::TCyclopediaDialogController>(
        appearancesManager(), clientOptions(), m_pGameActionMapper, engine, dialogParentItem);
    // CIP: the social dialog (the sidebar panels put its button on the button
    // bar) and the task board.
    g.socialDialog = std::make_unique<gamewindow::TSocialDialogController>(clientOptions(), engine, dialogParentItem);
    m_pTaskboardDialogController = std::make_shared<gamewindow::TTaskboardDialogController>(
        m_pGameActionMapper, shared::TClockService::process(), engine, dialogParentItem);
    m_pSoulSealsDialogController = std::make_unique<gamewindow::TSoulSealsDialogController>(engine, dialogParentItem);
    m_pBossDifficultySelectionDialogController =
        std::make_unique<gamewindow::TBossDifficultySelectionDialogController>(engine, dialogParentItem);
    m_pStashDialogController = std::make_unique<gamewindow::TStashDialogController>(
        m_pGameActionMapper, appearancesManager(), traderStorage(), clientOptions(), engine, dialogParentItem);
    m_pSelectObjectAmountDialogController =
        std::make_unique<gamewindow::TSelectObjectAmountDialogController>(engine, dialogParentItem);
    // CIP: the confirmation before an object goes into the stash is the client's
    // own dialog (it answers SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG itself).
    m_pConfirmMoveObjectToStashDialogController =
        std::make_unique<gamewindow::TConfirmMoveObjectToStashDialogController>(clientOptions(), engine, dialogParentItem);
    m_pShowImageDialogController = std::make_unique<gamewindow::TShowImageDialogController>(engine, dialogParentItem);
    m_pShowSpellBookDialogController = std::make_unique<gamewindow::TShowSpellBookDialogController>(engine, dialogParentItem);
    m_pNaggingMessageDialogController = std::make_unique<gamewindow::TNaggingMessageDialogController>(engine, dialogParentItem);
    g.npcDialog = std::make_unique<gamewindow::TNpcDialogController>(clientOptions(), m_pGameActionMapper, chat,
                                                                     m_pEventFilterManager.get(), engine, dialogParentItem);
    // CIP: the client options mode switcher; it reaches the session's sidebar
    // options through the client.
    g.clientOptionsModeSwitcher = std::make_unique<config::TClientOptionsModeSwitcher>(clientOptions(), dialogParentItem);
    g.clientOptionsModeSwitcher->connectToGameSession([this] { return sidebarOptions(); });
    // Host: the game's end and the client's game actions that feed the
    // dialogs above until they follow the session and the router themselves.
    clearQuestLogOnGameEnd(m_pQuestLogController.get());
    g.gameActions.on(input::EGameAction::OPEN_QUESTLOG_DIALOG, [this](const input::TGameActionOpenQuestLog& action) {
        m_pQuestLogController->showDialog(action.questId());
        return true;
    });

    // CIP: the screenshot manager (over the client window and the map window).
    g.screenshotManager = std::make_unique<screenshot::TScreenshotManager>(
        clientOptions(), tibiaFileSystemHelper(), shared::TClockService::process(), window ? window->contentItem() : nullptr,
        mapWindowPane);
    // CIP: the container storage, the sidebar panels (the sidebar manager,
    // the client options, the mouse mapping, the social dialog, the dialog
    // parent item and the engine) and the sidebar widgets.
    g.containers =
        std::make_unique<gamewindow::TContainerStorageController>(sidebarManager, m_pGameActionMapper, clientOptions(), gameWindowItem);
    gamewindow::TContainerStorageController* containers = g.containers.get();
    g.sidebarPanels = std::make_unique<gamewindow::TSidebarPanelsManager>(
        sidebarManager, clientOptions(), m_pGameActionMapper, g.socialDialog.get(), dialogParentItem, engine);
    gamewindow::TSidebarPanelsManager* sidebarPanels = g.sidebarPanels.get();
    g.sidebarWidgets = std::make_unique<gamewindow::TSidebarWidgetsManager>(
        sidebarManager, containers, m_pGameActionMapper, m_pEventFilterManager.get(), clientOptions(), tibiaFileSystemHelper(),
        newsStorage(), spellInfoStorage(), dialogParentItem, g.analyserStorages.get());
    gamewindow::TSidebarWidgetsManager* sidebarWidgets = g.sidebarWidgets.get();
    sidebarPanels->connectToSidebarWidgetsManager(sidebarWidgets);
    // CIP: a switched options mode restores the sidebar widgets and reorders
    // the sidebar panels from the options.
    connect(g.clientOptionsModeSwitcher.get(), &config::TClientOptionsModeSwitcher::configureSidebarFromClientOptions, sidebarWidgets,
            &gamewindow::TSidebarWidgetsManager::restoreSidebarConfigurationFromOptions);
    connect(g.clientOptionsModeSwitcher.get(), &config::TClientOptionsModeSwitcher::configureSidebarFromClientOptions, sidebarPanels,
            &gamewindow::TSidebarPanelsManager::reorderPanelsFromOptions);
    // Host: the stored control and loot scheme reach the controllers that
    // read them (CipSoft: the client's scheme options slot).
    const QJsonObject storedOptions = app::ClientOptions::load().options();
    const int controlScheme = config::controlSchemeForListModelIndex(storedOptions.value(QLatin1String("controlSchemeIndex")).toInt(0));
    const int lootScheme = config::lootSchemeForListModelIndex(storedOptions.value(QLatin1String("lootSchemeIndex")).toInt(0));
    g.minimapController = sidebarPanels->minimap();
    containers->setControlScheme(controlScheme);
    containers->setLootScheme(lootScheme);
    sidebarWidgets->setControlScheme(controlScheme);
    m_pCyclopediaDialogController->setControlScheme(controlScheme);
    connect(clientOptions(), &config::TClientOptions::optionChanged, gameWindowItem,
            [this, &g](const config::EClientOption id) {
                const QJsonObject opts = app::ClientOptions::load().options();
                if (id == config::EClientOption::controlSchemeIndex) {
                    const int scheme = config::controlSchemeForListModelIndex(opts.value(QLatin1String("controlSchemeIndex")).toInt(0));
                    if (m_pGameActionMapper)
                        m_pGameActionMapper->setControlScheme(scheme);
                    g.containers->setControlScheme(scheme);
                    g.sidebarWidgets->setControlScheme(scheme);
                } else if (id == config::EClientOption::lootSchemeIndex) {
                    const int scheme = config::lootSchemeForListModelIndex(opts.value(QLatin1String("lootSchemeIndex")).toInt(0));
                    if (m_pGameActionMapper)
                        m_pGameActionMapper->setLootScheme(scheme);
                    g.containers->setLootScheme(scheme);
                }
            });
    // Host: a pack's social button (setSocialButtonOnButtonBar) replaces the bar's.
    if (gamewindow::TButtonBarController* buttonBar = sidebarPanels->buttonBar()) {
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
    // Host: the analyser widgets (on the sidebar widgets).
    g.analyserWidgets = std::make_unique<analysers::TAnalyserWidgets>(g.analyserStorages.get(), sidebarWidgets, gameWindowItem, engine,
                                                                      [this] { return sidebarOptions(); });
    // CIP: the stash follows the market and the loot container dialogs closing.
    connect(m_pMarketController.get(), &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed,
            m_pStashDialogController.get(), &gamewindow::TStashDialogController::onMarketDialogDialogClosed);
    connect(g.manageLootDialog.get(), &cip::tibia::gamewindow::TDialogControllerBase::dialogClosed,
            m_pStashDialogController.get(), &gamewindow::TStashDialogController::onManageLootContainerDialogClosed);

    // CIP: the controllers' publishGameAction reach the router, in CipSoft's order.
    game::TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    const auto publishToRouter = [router](auto* publisher) {
        if (publisher)
            connect(publisher, &std::remove_pointer_t<decltype(publisher)>::publishGameAction, router,
                    &game::TInternalGameActionRouter::handleGameActionInternal);
    };
    connect(m_pKeyboardShortcutHandler.get(), &input::TKeyboardShortcutHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
    publishToRouter(g.mapWindowController);
    publishToRouter(g.chat.get());
    publishToRouter(g.containers.get());
    publishToRouter(gameWindowController);
    publishToRouter(g.sidebarPanels.get());
    publishToRouter(g.sidebarWidgets.get());
    publishToRouter(g.optionsDialog.get());
    publishToRouter(g.editMinimapMarkerDialog.get());
    publishToRouter(m_pSelectOutfitDialogController.get());
    publishToRouter(m_pConfigureCreaturePodiumDialogController.get());
    publishToRouter(m_pReportViolationController.get());
    publishToRouter(m_pDeathDialogController.get());
    publishToRouter(g.newsDialog.get());
    publishToRouter(m_pStoreDialogController.get());
    publishToRouter(m_pMarketController.get());
    connect(m_pClipboardGameActionHandler.get(), &input::TClipboardGameActionHandler::publishGameAction, router,
            &game::TInternalGameActionRouter::handleGameActionInternal);
    publishToRouter(m_pLinkCopyWarningDialogController.get());
    publishToRouter(gamewindow::TContextMenuController::instance());
    publishToRouter(m_pPreyDialogController.get());
    publishToRouter(g.imbuingDialog.get());
    publishToRouter(g.inspectObjectDialog.get());
    publishToRouter(g.inspectPlayerDialog.get());
    publishToRouter(g.actionBars.get());
    g.actionBars->connectToGameActionRouter(gameActionRouter());
    publishToRouter(m_pBlessingsDialogController.get());
    publishToRouter(m_pRewardWallDialogController.get());
    publishToRouter(m_pCyclopediaDialogController.get());
    publishToRouter(m_pTaskboardDialogController.get());
    publishToRouter(m_pQuestLogController.get());
    publishToRouter(g.manageLootDialog.get());
    publishToRouter(g.exivaOptionsDialog.get());
    publishToRouter(g.screenshotManager.get());
    publishToRouter(m_pSelectObjectAmountDialogController.get());
    publishToRouter(m_pStashDialogController.get());
    publishToRouter(m_pConfirmMoveObjectToStashDialogController.get());
    publishToRouter(g.hirelingNameChange.get());
    publishToRouter(g.socialDialog.get());
    // CIP: the tutorial hint dialog confirms its hint (CONFIRM_TUTORIAL_HINT)
    // through the router; the session's hints processor waits for it.
    if (g.tutorialShell)
        publishToRouter(g.tutorialShell->hint());
    publishToRouter(g.characterTradeDialog.get());
    publishToRouter(m_pExaltationForgeDialogController.get());
    publishToRouter(m_pEditTextController.get());
    publishToRouter(g.skillWheelDialog.get());
    publishToRouter(m_pShowSpellBookDialogController.get());
    publishToRouter(g.sidebarManager.get());
    publishToRouter(m_pWeaponProficiencyDialogController.get());
    publishToRouter(m_pWeaponProficiencyShapeDialogController.get());
    publishToRouter(g.npcDialog.get());
    // Host: the publishers CipSoft's sub-controllers and singletons own (the
    // status bar, the target
    // selection, the analysers) and the engine's text copy
    // controller, which outlives the game windows.
    publishToRouter(g.targetSelection.get());
    publishToRouter(g.analyserStorages.get());
    publishToRouter(g.analyserWidgets.get());
    if (auto* textCopy = qobject_cast<gamewindow::TTextCopyController*>(
            engine->rootContext()->contextProperty(QStringLiteral("tibiaTextCopyController")).value<QObject*>()))
        connect(textCopy, &gamewindow::TTextCopyController::publishGameAction, router,
                &game::TInternalGameActionRouter::handleGameActionInternal, Qt::UniqueConnection);

    // CIP: the router's actions reach the handlers, in CipSoft's order; the
    // client's own handler comes first (host: then the game window's own
    // dispatch for the quest log).
    const auto handleFromRouter = [router](auto* handler) {
        if (handler)
            connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, handler,
                    qOverload<const input::IGameActionPtr&>(&std::remove_pointer_t<decltype(handler)>::handleGameAction));
    };
    // The handlers whose handleGameAction takes the action itself.
    const auto handleActionFromRouter = [router](auto* handler) {
        if (handler)
            connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, handler,
                    [handler](const input::IGameActionPtr& action) {
                        if (action)
                            handler->handleGameAction(*action);
                    });
    };
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, this, &TGameClient::handleGameAction);
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, g.productHost.get(),
            [&g](const input::IGameActionPtr& action) { g.gameActions.handle(action); });
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, m_pKeyboardShortcutHandler.get(),
            qOverload<const input::IGameActionPtr&>(&input::TKeyboardShortcutHandler::handleGameAction));
    handleFromRouter(g.mapWindowController);
    handleFromRouter(g.sidebarWidgets.get());
    handleFromRouter(g.sidebarPanels.get());
    handleFromRouter(g.chat.get());
    handleFromRouter(g.optionsDialog.get());
    handleActionFromRouter(g.editMinimapMarkerDialog.get());
    handleFromRouter(m_pReportViolationController.get());
    handleActionFromRouter(g.whiteBlacklistDialog.get());
    handleFromRouter(m_pBugReportDialogController.get());
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, m_pOpenWebsiteGameActionHandler.get(),
            &input::TOpenWebsiteGameActionHandler::handleGameAction);
    handleActionFromRouter(g.newsDialog.get());
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, m_pClipboardGameActionHandler.get(),
            &input::TClipboardGameActionHandler::handleGameAction);
    handleFromRouter(m_pLenshelpManager.get());
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal,
            m_pGameSessionDisconnectReactionController.get(), &gamewindow::TGameSessionDisconnectReactionController::handleGameAction);
    connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, m_authentication.get(),
            &authentication::TAuthenticationProcessController::handleGameAction);
    handleActionFromRouter(m_pLinkCopyWarningDialogController.get());
    handleFromRouter(g.tutorialOverlay.get());
    handleFromRouter(gamewindow::TContextMenuController::instance());
    handleFromRouter(g.clientOptionsModeSwitcher.get());
    handleFromRouter(m_pPreyDialogController.get());
    handleFromRouter(g.actionBars.get());
    handleFromRouter(m_pRewardWallDialogController.get());
    handleFromRouter(g.inspectObjectDialog.get());
    handleFromRouter(g.characterTradeDialog.get());
    handleFromRouter(m_pCyclopediaDialogController.get());
    handleFromRouter(m_pTaskboardDialogController.get());
    handleFromRouter(g.socialDialog.get());
    handleFromRouter(g.manageLootDialog.get());
    handleFromRouter(g.containers.get());
    handleFromRouter(g.exivaOptionsDialog.get());
    handleActionFromRouter(g.screenshotManager.get());
    handleFromRouter(m_pSelectObjectAmountDialogController.get());
    handleFromRouter(m_pStashDialogController.get());
    handleFromRouter(m_pConfirmMoveObjectToStashDialogController.get());
    handleFromRouter(g.calendarDialog.get());
    handleFromRouter(m_pStoreDialogController.get());
    handleFromRouter(m_pHighscoresDialogController.get());
    handleFromRouter(m_pExaltationForgeDialogController.get());
    handleFromRouter(g.skillWheelDialog.get());
    handleFromRouter(m_pShowImageDialogController.get());
    handleFromRouter(m_pNaggingMessageDialogController.get());
    handleFromRouter(m_pWeaponProficiencyDialogController.get());
    handleFromRouter(m_pWeaponProficiencyShapeDialogController.get());
    handleFromRouter(m_pWeaponProficiencyShapeOptionsDialogController.get());
    handleFromRouter(g.npcDialog.get());
    handleActionFromRouter(m_pMultiOfflineTrainingDialogController.get());
    // Host: the handlers CipSoft does not connect here (the market, the
    // target selection, the tutorial shell and the session chrome).
    handleFromRouter(m_pMarketController.get());
    handleFromRouter(g.targetSelection.get());
    handleFromRouter(g.tutorialShell.get());
    handleFromRouter(g.sessionBootstrap.get());
    // CIP: the action bars restore their last configuration once connected.
    g.actionBars->restoreLastAktionBarConfiguration();
    qmlcomponents::followSoundOptions(clientOptions());
    // Host: the session starts once every controller exists and hears the
    // router, so login errors can show (a replay's session connects them
    // when it becomes the current one).
    const bool sessionBeforeStart = static_cast<bool>(m_currentGameSession);
    // CIP: a pending error aborts the authentication and shows as a popup.
    if (!m_pendingErrorMessage.isEmpty()) {
        if (m_authentication)
            Q_EMIT m_authentication->abort();
        Q_EMIT showErrorAsPopupAndCriticalLog(m_pendingErrorMessage);
    }
    // CIP: a startup notice of the client options shows with the invalid
    // configuration notice. Host: the renderer change confirmation CipSoft
    // asks otherwise is the session bootstrap's (promptKeepGraphicsEngine).
    if (config::TClientOptions* const options = clientOptions(); options && options->hasStartupNotice()) {
        QString message;
        const QString notice = options->startupNotice();
        if (!notice.isEmpty()) {
            message.append(notice);
            message.append(QStringLiteral("\n\n"));
        }
        message.append(qtTrId("configuration_invalid"));
        m_pConfigurationInvalidDialogController->showMessage(qtTrId("configuration_invalid_caption"), message,
                                                             gamewindow::okDialogButtons());
    }
    // CIP: a crash dump of the last run offers a crash report.
    if (!preview)
        checkForCrashDump();
    // CIP: the event schedule is asked for once at start (host: the browser
    // waits for the login, see above).
    if (tibia::platform::kStartScreenServicesBeforeLogin)
        g.eventsUploader->requestUpdate();
    // CIP: the looping sounds pause until the game screen shows.
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionSoundTrigger>(true));
    // CIP: on the first start the create-account option shows when the
    // portal allows account creation, and rotating with Shift is on.
    if (const auto* application = dynamic_cast<const TGameApplication*>(QCoreApplication::instance());
        application && application->isFirstStart()) {
        qDebug() << "Client is started for first the time.";
        m_pCreateAccountDialogController->checkIfAccountCreationEnabled([gameWindowController](const bool enabled) {
            if (enabled)
                gameWindowController->setShowCreateAccountOption(true);
        });
        if (config::TClientOptions* const options = clientOptions())
            options->setBoolOption(config::EClientOption::rotateWithShift, true);
    }
    if (!preview)
        start();

    // Host: the packs' wiring. Must outlive bootstrapAll: SuiteRuntime keeps
    // PackHostWire* for deferred slots (HUD chat switch, store/options/logout).
    g.packHost = std::make_unique<qtclient::PackHostWire>();
    qtclient::PackHostWire& packHost = *g.packHost;
    // Host extension: a pack's extra quest rows (the backend handles its own actions).
    packHost.bindQuestTracker = [&g](mods::MissionBackend* backend) {
        g.sidebarWidgets->questTrackerWidget()->setMissionBackend(backend);
    };
    packHost.bindQuestLog = [this](mods::MissionBackend* backend) {
        m_pQuestLogController->setMissionBackend(backend);
    };
    packHost.bindInstanceToInfoDialogs = [this](mods::InstanceBackend* instance) {
        m_pCyclopediaDialogController->setInstanceStorage(instance);
    };
    packHost.bindAttentionToButtonBar = [&g](mods::AttentionBackend* attention) {
        if (auto* buttonBar = g.sidebarPanels->buttonBar())
            buttonBar->setAttentionStorage(attention);
    };
    packHost.addOptionsPage = [optionsDialog](const int category, void* page) {
        optionsDialog->addExtensionPage(category, static_cast<config::TOptionsPage*>(page));
    };
    // Host extension: chrome drawn over the map window forwards the mouse to it.
    packHost.mapOverlayHover = [&g](const bool hovering, const double x, const double y) {
        if (g.mapWindowController)
            g.mapWindowController->setOverlayHover(hovering, x, y);
    };
    packHost.mapOverlayClick = [&g](const int x, const int y, const int button, const int modifiers) {
        if (g.mapWindowController)
            g.mapWindowController->onMapWindowClicked(x, y, button, modifiers);
    };
    packHost.mapOverlayStartDrag = [&g](const int x, const int y) {
        if (g.mapWindowController)
            g.mapWindowController->onMapWindowStartDrag(x, y, nullptr);
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
    // Host: a pack that brings its own social button (and dialog) replaces
    // the stock one: the stock dialog stops answering the router's actions.
    packHost.setSocialButtonOnButtonBar = [&g, router](QObject* controller) {
        if (auto* buttonBar = g.sidebarPanels->buttonBar())
            buttonBar->setSocialButtonController(controller);
        if (controller && g.socialDialog)
            QObject::disconnect(router, nullptr, g.socialDialog.get(), nullptr);
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
        try {
            g.sidebarWidgets->setSidebarOptions(sidebarOptions());
            g.sidebarPanels->setSidebarOptions(sidebarOptions());
            if (gamewindow::TPremiumPanelController* premiumPanel = g.sidebarPanels->premiumPanel()) {
                premiumPanel->setPremiumTimeStorage(m_pPremiumTimeStorage.get());
                premiumPanel->onSessionStarted();
                if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_currentGameSession.get()))
                    connect(session->storeProtocolMessageHandler().get(), &store::TStoreProtocolMessageHandler::storeButtonDeeplinkChanged,
                            premiumPanel, &gamewindow::TPremiumPanelController::onStoreDeeplinkChanged, Qt::UniqueConnection);
            }
            // CIP: the skill wheel dialog's premium status follows the premium time.
            g.skillWheelDialog->setPremiumTimeStorage(m_pPremiumTimeStorage.get());
            g.sidebarWidgets->onLoginRampUpFinished();
            m_pCyclopediaDialogController->setAmbientLightStorage(ambientLight());
            // The Cyclopedia map's local minimap mode draws the local minimap.
            qmlcomponents::TMinimapRendererQuickItem::setMinimapTileStorage(minimapTileStorage().get());
            if (g.minimapController) {
                g.minimapController->setAmbientLightStorage(ambientLight());
                g.minimapController->setMarkerStorage(minimapMarkerStorage().get());
            }
            g.sidebarPanels->setTileStorage(minimapTileStorage().get());
            g.optionsDialog->setInGame(true);
            g.actionBars->onWorldEntered();
            qmlcomponents::applyOptionVolumes(); // stop login anthem; keep channel gains
        } catch (...) {
        }
    });
    connect(this, &TGameClient::worldLeft, gameWindowItem, [this, &g] {
        if (g.minimapController) {
            g.minimapController->setMarkerStorage(nullptr);
            g.minimapController->setTileStorage(nullptr);
        }
        qmlcomponents::TMinimapRendererQuickItem::setMinimapTileStorage(nullptr);
        g.sidebarPanels->setSidebarOptions(nullptr);
        g.sidebarWidgets->setSidebarOptions(nullptr);
        g.optionsDialog->setInGame(false);
        m_pDialogStackController->hideAll();
        g.gameWindowController->showStartScreen();
        qmlcomponents::applyOptionVolumes(); // resume login anthem when offline
    });
    connect(application, &QCoreApplication::aboutToQuit, this, [this, &g] {
        g.mapWindowController->unbind();
        if (g.minimapController) {
            g.minimapController->setMarkerStorage(nullptr);
            g.minimapController->setTileStorage(nullptr);
        }
        qmlcomponents::TMinimapRendererQuickItem::setMinimapTileStorage(nullptr);
        shutdown();
    });
    // Host: a session that already ran before the game window links the
    // protocol message handlers the controllers above made (chat, store,
    // market, trades, ...) and connects the controllers to it.
    if (auto* session = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get()))
        session->relinkProtocolMessageHandlers();
    if (sessionBeforeStart && m_currentGameSession)
        connectGameWindowControllersToGameSession(m_currentGameSession);
    return true;
}

input::TKeyboardStatusStorage* TGameClient::keyboardStatusStorage() const
{
    return m_pKeyboardStatusStorage.get();
}

gamewindow::TGameWindowController* TGameClient::gameWindowController() const
{
    return m_gameWindow ? m_gameWindow->gameWindowController.get() : nullptr;
}

gamewindow::TSidebarManager* TGameClient::sidebarManager() const
{
    return m_gameWindow ? m_gameWindow->sidebarManager.get() : nullptr;
}

screenshot::TScreenshotManager* TGameClient::screenshotManager() const
{
    return m_gameWindow ? m_gameWindow->screenshotManager.get() : nullptr;
}

gamewindow::TTargetSelectionController* TGameClient::targetSelectionController() const
{
    return m_gameWindow ? m_gameWindow->targetSelection.get() : nullptr;
}

void TGameClient::onKeyboardStatusStorageChanged()
{
    if (!m_pKeyboardInputToGameActionConverter)
        return;
    const input::TMovementIntentPtr intent = m_pKeyboardInputToGameActionConverter->changedMovementIntent();
    if (!intent)
        return;
    // Host (pack notify, as the session walk did): a manual step, e.g. to stop a pack's travel.
    if (intent->kind() == input::TMovementIntentKind::Direction)
        tibia::game::notify("onWalk", static_cast<tibia::client::Direction>(
                                          static_cast<const input::TMovementIntentDirection&>(*intent).direction()));
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionPlayerMovementIntent>(intent));
}

void TGameClient::checkKeyboardStatusStorageAfterDelay(const int DelayMillis)
{
    QTimer::singleShot(DelayMillis, this, &TGameClient::onKeyboardStatusStorageChanged);
}

gamewindow::TGameSessionDisconnectReactionController* TGameClient::gameSessionDisconnectReactionController() const
{
    return m_pGameSessionDisconnectReactionController.get();
}

void TGameClient::onFullscreenModeEntered()
{
    if (m_gameWindow && m_gameWindow->mapWindowController)
        m_gameWindow->mapWindowController->onFullscreenModeEntered();
}

void TGameClient::connectGameWindowControllersToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    TGameWindow* g = m_gameWindow.get();
    // CIP: the controllers follow the new session (or let go of the old one)
    // in this order; the premium-aware ones also get the client's premium
    // time storage and the Cyclopedia and the social dialog the minimap's.
    if (g && g->gameWindowController)
        g->gameWindowController->connectToGameSession(GameSession);
    if (g && g->mapWindowController)
        g->mapWindowController->connectToGameSession(GameSession);
    // Host: the battle lists' mouse target and the FPS indicator's build
    // times are the session's world map frame's.
    if (g && g->sidebarWidgets)
        g->sidebarWidgets->battleLists()->setWorldMapRenderProvider(session ? session->worldMapRenderProvider().get() : nullptr);
    if (g && g->fpsLatency)
        g->fpsLatency->setRenderProvider(session ? session->worldMapRenderProvider().get() : nullptr);
    if (g && g->containers)
        g->containers->connectToGameSession(GameSession);
    if (m_pSelectObjectAmountDialogController)
        m_pSelectObjectAmountDialogController->connectToGameSession(GameSession);
    if (g && g->chat)
        g->chat->connectToGameSession(GameSession);
    if (m_pMarketController)
        m_pMarketController->connectToGameSession(GameSession);
    if (m_pOutfitMemorialDialogController)
        m_pOutfitMemorialDialogController->connectToGameSession(GameSession);
    if (m_pSelectOutfitDialogController)
        m_pSelectOutfitDialogController->connectToGameSession(GameSession);
    if (m_pConfigureCreaturePodiumDialogController)
        m_pConfigureCreaturePodiumDialogController->connectToGameSession(GameSession);
    // Host shim: the analysers' aggregate follows the session's storages;
    // the drop tracker reports on the chat's channels.
    if (g && g->analyserStorages)
        g->analyserStorages->connectToGameSession(GameSession);
    if (session)
        session->itemTrackingStorage()->setChatStorage(session->chatChannelStorage().get());
    // CIP: the sidebar widgets manager connects every widget it owns and
    // opens and closes the player trade and depot search widgets from
    // the session's protocol handlers.
    if (g && g->sidebarWidgets) {
        g->sidebarWidgets->connectToGameSession(GameSession);
        // The client hands the widgets its premium time storage.
        g->sidebarWidgets->setPremiumTimeStorage(m_pPremiumTimeStorage.get());
    }
    if (g && g->sidebarPanels)
        g->sidebarPanels->connectToGameSession(GameSession);
    if (g && g->cooldownBar)
        g->cooldownBar->connectToGameSession(GameSession);
    // The quest log and the quest tracker show the session's quest log storage.
    if (m_pQuestLogController)
        m_pQuestLogController->connectToGameSession(GameSession);
    if (m_pServerModalDialogController)
        m_pServerModalDialogController->connectToGameSession(GameSession);
    if (m_pGameSessionDisconnectReactionController)
        m_pGameSessionDisconnectReactionController->connectToGameSession(GameSession);
    if (m_pWhiteBlacklist)
        m_pWhiteBlacklist->setGameSession(GameSession);
    if (m_pReportViolationController)
        m_pReportViolationController->connectToGameSession(GameSession);
    if (m_pEditTextController)
        m_pEditTextController->connectToGameSession(GameSession);
    if (m_pDeathDialogController)
        m_pDeathDialogController->connectToGameSession(GameSession);
    if (g && g->optionsDialog)
        g->optionsDialog->connectToGameSession(GameSession);
    if (g && g->newsDialog)
        g->newsDialog->connectToGameSession(GameSession);
    if (m_pStoreDialogController)
        m_pStoreDialogController->connectToGameSession(GameSession);
    if (m_pBugReportDialogController)
        m_pBugReportDialogController->connectToGameSession(GameSession);
    if (m_pPreyDialogController)
        m_pPreyDialogController->connectToGameSession(GameSession);
    if (g && g->imbuingDialog) {
        g->imbuingDialog->connectToGameSession(GameSession);
        g->imbuingDialog->connectToPremiumTimeStorage(m_pPremiumTimeStorage.get());
    }
    if (g && g->inspectObjectDialog)
        g->inspectObjectDialog->connectToGameSession(GameSession);
    if (g && g->inspectPlayerDialog)
        g->inspectPlayerDialog->connectToGameSession(GameSession);
    if (g && g->actionBars)
        g->actionBars->connectToGameSession(GameSession);
    if (m_pBlessingsDialogController) {
        m_pBlessingsDialogController->connectToGameSession(GameSession);
        m_pBlessingsDialogController->connectToPremiumTimeStorage(m_pPremiumTimeStorage.get());
    }
    // Host: the premium panel lives in the sidebar panels manager.
    if (g && g->sidebarPanels)
        g->sidebarPanels->setPremiumTimeStorage(m_pPremiumTimeStorage.get());
    if (m_pRewardWallDialogController) {
        m_pRewardWallDialogController->connectToGameSession(GameSession);
        m_pRewardWallDialogController->connectToPremiumTimeStorage(m_pPremiumTimeStorage.get());
    }
    if (g && g->manageLootDialog) {
        g->manageLootDialog->connectToGameSession(GameSession);
        g->manageLootDialog->connectToPremiumTimeStorage(m_pPremiumTimeStorage.get());
    }
    if (m_pCyclopediaDialogController) {
        m_pCyclopediaDialogController->connectToGameSession(GameSession);
        m_pCyclopediaDialogController->setPremiumTimeStorage(m_pPremiumTimeStorage.get());
        // Host: the Cyclopedia's houses and map pages draw the session's light.
        m_pCyclopediaDialogController->setAmbientLightStorage(session ? session->ambientLightStorage().get() : nullptr);
    }
    if (m_pTaskboardDialogController)
        m_pTaskboardDialogController->connectToGameSession(GameSession);
    if (m_pSoulSealsDialogController)
        m_pSoulSealsDialogController->connectToGameSession(GameSession);
    if (g && g->socialDialog) {
        g->socialDialog->connectToGameSession(GameSession);
        g->socialDialog->setPremiumTimeStorage(m_pPremiumTimeStorage.get());
    }
    if (m_pShowSpellBookDialogController)
        m_pShowSpellBookDialogController->connectToGameSession(GameSession);
    if (m_pStashDialogController)
        m_pStashDialogController->connectToGameSession(GameSession);
    if (g && g->exivaOptionsDialog)
        g->exivaOptionsDialog->connectToGameSession(GameSession);
    if (g && g->screenshotManager)
        g->screenshotManager->connectToGameSession(GameSession);
    m_predictor->connectToGameSession(GameSession);
    if (g && g->calendarDialog)
        g->calendarDialog->closeDialog();
    if (g && g->hirelingNameChange)
        g->hirelingNameChange->connectToGameSession(GameSession);
    // CIP: the login's controller follows the session (its player's level and
    // data, and the login request uploader's character list).
    if (m_authentication)
        m_authentication->connectToGameSession(std::dynamic_pointer_cast<game::TGameSessionBase>(GameSession));
    if (m_pHighscoresDialogController)
        m_pHighscoresDialogController->connectToGameSession(GameSession);
    if (g && g->characterTradeDialog)
        g->characterTradeDialog->connectToGameSession(GameSession);
    if (m_pExaltationForgeDialogController)
        m_pExaltationForgeDialogController->connectToGameSession(GameSession);
    if (soundConfiguration())
        soundConfiguration()->setGameSession(GameSession);
    if (g && g->skillWheelDialog)
        g->skillWheelDialog->connectToGameSession(GameSession);
    if (m_pWeaponProficiencyDialogController)
        m_pWeaponProficiencyDialogController->connectToGameSession(GameSession);
    if (m_pWeaponProficiencyShapeDialogController)
        m_pWeaponProficiencyShapeDialogController->connectToGameSession(GameSession);
    if (m_pWeaponProficiencyReshapeDialogController)
        m_pWeaponProficiencyReshapeDialogController->connectToGameSession(GameSession);
    if (g && g->npcDialog)
        g->npcDialog->connectToGameSession(GameSession);
    if (g && g->tutorialShell)
        g->tutorialShell->hint()->connectToGameSession(GameSession);
    if (m_pStoreAndResourceBalanceHelperController)
        m_pStoreAndResourceBalanceHelperController->connectToGameSession(GameSession);
    if (m_pSoundController)
        m_pSoundController->connectToGameSession(GameSession);
    if (g && g->tutorialShell)
        g->tutorialShell->vocation()->connectToGameSession(GameSession);
    if (m_pMultiOfflineTrainingDialogController)
        m_pMultiOfflineTrainingDialogController->connectToGameSession(GameSession);
    if (m_pBossDifficultySelectionDialogController)
        m_pBossDifficultySelectionDialogController->connectToGameSession(GameSession);
    if (m_pInfoBannerController)
        m_pInfoBannerController->connectToGameSession(GameSession);
    if (g && g->editMinimapMarkerDialog)
        g->editMinimapMarkerDialog->connectToGameSession(GameSession);
    // Host shim until these controllers connect to the session themselves:
    // they follow the session's character configuration being loaded.
    if (g && g->containers)
        g->containers->setCharacterConfigurationManager(session ? session->characterConfigurationManager().get() : nullptr);
    if (config::TCharacterConfigurationManager* manager = session ? session->characterConfigurationManager().get() : nullptr) {
        if (g && g->analyserStorages)
            connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, g->analyserStorages.get(),
                    &analysers::TAnalyserStorages::onCharacterConfigurationLoaded, Qt::UniqueConnection);
        if (gamewindow::TCyclopediaDialogController* cyclopedia = m_pCyclopediaDialogController.get())
            connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, cyclopedia->mapDialog(),
                    &gamewindow::TCyclopediaMapDialogController::showDialogTab, Qt::UniqueConnection);
        if (gamewindow::TQuestLogController* questLog = m_pQuestLogController.get())
            connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, questLog,
                    &gamewindow::TQuestLogController::onCharacterConfigurationLoaded, Qt::UniqueConnection);
    }
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
    // login advice, white/blacklist, move to stash, tutorial hint, vocation
    // selection, offline training and the proficiency shape options.
    if (m_pConnectingDialogController)
        m_pConnectingDialogController->dialog()->close();
    if (m_gameWindow) {
        TGameWindow& g = *m_gameWindow;
        if (g.editMinimapMarkerDialog)
            g.editMinimapMarkerDialog->dialog()->close();
        if (m_pSelectOutfitDialogController)
            m_pSelectOutfitDialogController->dialog()->close();
        if (m_pConfigureCreaturePodiumDialogController)
            m_pConfigureCreaturePodiumDialogController->dialog()->close();
        if (m_pLoginAdviceDialogController)
            m_pLoginAdviceDialogController->dialog()->close();
        if (g.whiteBlacklistDialog)
            g.whiteBlacklistDialog->close();
        if (m_pConfirmMoveObjectToStashDialogController)
            m_pConfirmMoveObjectToStashDialogController->closeDialog();
        if (g.tutorialShell) {
            g.tutorialShell->hint()->dialog()->close();
            g.tutorialShell->vocation()->dialog()->close();
        }
        if (m_pMultiOfflineTrainingDialogController)
            m_pMultiOfflineTrainingDialogController->closeDialog();
        if (m_pWeaponProficiencyShapeOptionsDialogController)
            m_pWeaponProficiencyShapeOptionsDialogController->closeDialog();
    }
    // CIP: the walk keys no longer reach the session (a live session connects the first again).
    if (input::TKeyboardStatusStorage* keyboardStatus = keyboardStatusStorage()) {
        disconnect(keyboardStatus, &input::TKeyboardStatusStorage::changed, this, &TGameClient::onKeyboardStatusStorageChanged);
        disconnect(keyboardStatus, &input::TKeyboardStatusStorage::checkForChangeAfterDelay, this,
                   &TGameClient::checkKeyboardStatusStorageAfterDelay);
    }
    // CIP: the session's spell modifications are dropped.
    if (spells::TSpellInfoStorage* spells = spellInfoStorage())
        spells->clearSpellModifications();
    // Host: the world is left (minimap and character options written, start screen).
    onWorldLeft();
    if (!m_gameSessionMetaInfoQueue.empty()) {
        autoconnectNextGameSessionInQueue();
        return;
    }
    setCurrentGameSession(nullptr);
    updateWindowTitleExtension();
    updateCrashReportAnnotations();
    // CIP: a character change or a login again connects the next session.
    if (m_pGameSessionDisconnectReactionController && m_pGameSessionDisconnectReactionController->reconnectRequested())
        connectClientToGameserverWithExistingCredentials();
}

}
}
