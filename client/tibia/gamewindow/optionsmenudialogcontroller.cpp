#include "optionsmenudialogcontroller.h"

#include "config/clientoptions.h"

#include "app/clientoptions.h"
#include "app/graphicsrestart.h"
#include "chat/chatchannel.h"
#include "creatures/creaturestorage.h"
#include "client/gameclient.h"
#include "config/clientoptiondefaults.h"
#include "config/characterconfigurationmanager.h"
#include "config/configbackupreader.h"
#include "config/hotkeyoptions.h"
#include "game/currentgamesession.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "gamewindow/aboutdialogcontroller.h"
#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "shared/tibiafilesystemhelper.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "minimap/minimapdiskio.h"
#include "minimap/minimaptilestorage.h"
#include "systemsurvey/slidingfpscounter.h"


#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>
#include <protocol.pb.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using O = cip::EClientOption;
using EGameAction = input::EGameAction;
using Button = TMessageDialogController::Button;

/// CIP's default TClientOptions.
std::unique_ptr<config::TClientOptions> defaultOptions()
{
    auto options = std::make_unique<config::TClientOptions>();
    options->setDefaults();
    return options;
}

/// The renderer, vsync and font renderer options (a change asks for a restart).
struct TRendererOptions
{
    int renderer = 0;
    bool vSync = false;
    bool freetype = false;

    explicit TRendererOptions(const config::TClientOptions& options)
        : renderer(options.intOption(O::rendererIndex))
        , vSync(options.boolOption(O::vSyncEnabled))
        , freetype(options.boolOption(O::useFreetypeRenderer))
    {}

    bool operator==(const TRendererOptions&) const = default;
};

/// : "OK" (Return) and "Cancel" (Escape).
QList<TMessageDialogButton*> okCancelButtons()
{
    return { new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))),
             new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) };
}

QList<TMessageDialogButton*> okButton()
{
    return { new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))) };
}

input::IGameActionPtr simpleAction(const EGameAction type)
{
    return input::makeGameAction(type);
}

/// CIP DAT_102793370 and the storage directories (,
/// import list DAT_102793700 / DAT_102793800 / DAT_1027936a0).
constexpr auto OptionsFile = "conf/clientoptions.json";
constexpr auto ConfDirectory = "conf";
constexpr auto CharacterDataDirectory = "characterdata";
constexpr auto MinimapDirectory = "minimap";

QString dataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString localFile(const QString& url)
{
    const QUrl parsed(url);
    return parsed.isLocalFile() ? parsed.toLocalFile() : url;
}

}

TOptionsMenuDialogController::TOptionsMenuDialogController(
    config::TClientOptions* ClientOptions, systemsurvey::TSlidingFPSCounter* SlidingFPSCounter,
    std::shared_ptr<shared::TTibiaFileSystemHelper> FileSystemHelper, input::TEventFilterManager* EventFilterManager,
    std::shared_ptr<appearances::TAppearanceInstanceFactory> AppearanceInstanceFactory,
    std::shared_ptr<minimap::TMinimapDiskIOAsync> MinimapTileDiskIO, std::shared_ptr<minimap::TMinimapTileStorage> MinimapTileStorage,
    spells::TSpellInfoStorage* SpellInfoStorage, QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("OptionsMenuDialog.qml"), Engine, DialogParentItem, parent)
    , m_clientOptions(ClientOptions)
    , m_pSlidingFPSCounter(SlidingFPSCounter)
    , m_pFileSystemHelper(std::move(FileSystemHelper))
    , m_pMinimapTileDiskIO(std::move(MinimapTileDiskIO))
    , m_pMinimapTileStorage(std::move(MinimapTileStorage))
    , m_pEventFilterManager(EventFilterManager)
    , m_pAppearanceInstanceFactory(std::move(AppearanceInstanceFactory))
    , m_pSpellInfoStorage(SpellInfoStorage)
    , m_optionPageQmlWrapperBase(this)
{
    auto* actionBars = new config::TActionBarsOptionsPage(config::actionBarsBindings(), this);
    auto* buttonBar = new config::TButtonBarOptionsPage(this);
    auto* console = new config::TConsoleOptionsPage(config::consoleBindings(), this);
    auto* interfacePage = new config::TInterfaceOptionsPage(this);
    auto* gameWindow = new config::TGameWindowOptionsPage(this);
    auto* hud = new config::THudOptionsPage(this);
    auto* misc = new config::TMiscOptionsPage(config::miscBindings(), this);
    m_gameplay = new config::TGameplayOptionsPage(config::gameplayBindings(), this);
    auto* graphics = new config::TGraphicsOptionsPage(this);
    auto* effects = new config::TEffectsOptionsPage(this);
    m_hotkeys = new config::THotkeysOptionsPage(Engine, DialogParentItem, m_pSpellInfoStorage, m_pEventFilterManager,
                                                m_pAppearanceInstanceFactory, this);
    connect(m_hotkeys, &config::THotkeysOptionsPage::publishGameAction, this, &TOptionsMenuDialogController::publishGameAction);
    m_controls = new config::TControlsOptionsPage(this);
    connect(m_controls, &config::TControlsOptionsPage::changed, this, &TOptionsMenuDialogController::onControlOptionsChanged);
    auto* screenshots = new config::TScreenshotsOptionsPage(config::screenshotsBindings(), this);
    auto* sound = new config::TSoundOptionsPage(this);
    auto* battleSounds = new config::TBattleSoundsOptionsPage(config::battleSoundsBindings(), this);
    auto* uiSounds = new config::TUISoundsOptionsPage(config::uiSoundsBindings(), this);

    m_actionBars = actionBars;
    m_actionBarsOptions = actionBars;
    m_buttonBarOptions = buttonBar;
    m_consoleOptions = console;
    m_interfaceOptions = interfacePage;
    m_gameWindowOptions = gameWindow;
    m_hudOptions = hud;
    m_miscOptions = misc;
    m_gameplayOptions = m_gameplay;
    m_graphicsOptions = graphics;
    m_effectsOptions = effects;
    m_hotkeyOptions = m_hotkeys;
    m_controlsOptions = m_controls;
    m_screenshotsOptions = screenshots;
    m_soundOptions = sound;
    m_battleSoundsOptions = battleSounds;
    m_uiSoundsOptions = uiSounds;

    m_pages = {
        { ActionBars, actionBars },
        { ButtonBar, buttonBar },
        { Console, console },
        { Interface, interfacePage },
        { GameWindow, gameWindow },
        { HUD, hud },
        { Misc, misc },
        { Gameplay, m_gameplay },
        { Graphics, graphics },
        { Effects, effects },
        { DefaultHotkeys, m_hotkeys },
        { Controls, m_controls },
        { Screenshots, screenshots },
        { Sound, sound },
        { BattleSounds, battleSounds },
        { UISounds, uiSounds },
    };

    m_ruleViolationsMessage = makeMessageDialog(Engine, DialogParentItem);
    m_restartMessage = makeMessageDialog(Engine, DialogParentItem);
    m_rotationKeysMessage = makeMessageDialog(Engine, DialogParentItem);
    m_resetPageMessage = makeMessageDialog(Engine, DialogParentItem);
    m_resetAllMessage = makeMessageDialog(Engine, DialogParentItem);
    m_infoMessage = makeMessageDialog(Engine, DialogParentItem);
    m_importMessage = makeMessageDialog(Engine, DialogParentItem);
    m_about = new TAboutDialogController(this);
    connect(m_about, &TAboutDialogController::publishGameAction, this, &TOptionsMenuDialogController::publishGameAction);
    connect(m_importMessage, &TMessageDialogController::acceptClicked, this, &TOptionsMenuDialogController::importArchive);
    connect(m_resetPageMessage, &TMessageDialogController::acceptClicked, this, &TOptionsMenuDialogController::resetCurrentPageToDefault);
    connect(m_resetAllMessage, &TMessageDialogController::acceptClicked, this, &TOptionsMenuDialogController::resetAllOptionsToDefault);
    connect(m_restartMessage, &TMessageDialogController::buttonClicked, this, &TOptionsMenuDialogController::onResartMessageControllerClicked);
    connect(this, &cip::tibia::gamewindow::TOptionsMenuDialogController::dialogClosed, this, &TOptionsMenuDialogController::closeSubDialogs);
    m_gameActions.on(EGameAction::TARGET_SELECTION_FINISHED, [this](const input::IGameAction& action) { handleGameAction(action); });
    m_gameActions.on(EGameAction::OPEN_OPTIONS_MENU_DIALOG, [this](const input::IGameAction& action) { handleGameAction(action); });

    m_showAdvancedOptions = app::ClientOptions::load().options().value(QLatin1String("optionsShowAdvanced")).toBool(false);

    // (connected when the game connection is set): the current value at login.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this] {
        sendTurnTowardsMoveDirection(m_clientOptions->boolOption(O::alwaysTurnTowardsMoveDirection));
    });


    m_frameRateTimer.setInterval(1000);
    connect(&m_frameRateTimer, &QTimer::timeout, this, &TOptionsMenuDialogController::onBeforeRenderFrame);
}

void TOptionsMenuDialogController::onBeforeRenderFrame()
{
    // The client's frame rate over the last second. Host: CipSoft's client calls
    // this before every frame; qtclient polls it once a second while shown.
    setFrameRate(m_pSlidingFPSCounter ? m_pSlidingFPSCounter->framesPerSecond() : 0.0);
}

void TOptionsMenuDialogController::setFrameRate(const double frameRate)
{
    if (m_frameRate == frameRate)
        return;
    m_frameRate = frameRate;
    Q_EMIT frameRateChanged();
}

QString TOptionsMenuDialogController::currentFrameRate() const { return QString::number(m_frameRate, 'f', 2); }

TOptionsMenuDialogController::~TOptionsMenuDialogController() = default;

void TOptionsMenuDialogController::setGameClient(client::TGameClient* client)
{
    m_gameClient = client;
}

bool TOptionsMenuDialogController::handleGameAction(const input::IGameAction& action)
{
    if (action.gameActionType() == EGameAction::TARGET_SELECTION_FINISHED) {
        if (m_hiddenForTargetSelection && dialog()->item() && !dialog()->item()->isVisible()) {
            dialog()->show();
            m_hiddenForTargetSelection = false;
        }
    } else if (const auto* open = input::gameActionAs<input::TGameActionOpenOptionsMenuDialog>(&action)) {
        this->open(open->page());
        return true;
    }
    return false;
}

void TOptionsMenuDialogController::processAndPublishGameActions(const input::IGameActionPtr& action)
{
    if (action->gameActionType() == EGameAction::START_TARGET_SELECTION) {
        dialog()->hide();
        m_hiddenForTargetSelection = true;
    }
    Q_EMIT publishGameAction(action);
}

void TOptionsMenuDialogController::closeSubDialogs()
{
    for (TMessageDialogController* message : { m_ruleViolationsMessage, m_resetPageMessage, m_resetAllMessage, m_infoMessage, m_importMessage })
        if (message->dialog()->item())
            message->dialog()->close();
    if (m_about->dialog()->item())
        m_about->dialog()->close();
    m_hotkeys->closeDialogs();
    m_clientOptions->saveToJsonFile();
}

TMessageDialogController* TOptionsMenuDialogController::makeMessageDialog(QQmlEngine* Engine, QQuickItem* DialogParentItem)
{
    auto* message = new TMessageDialogController(Engine, DialogParentItem, this);
    message->setCloseOnButtonClicked(true);
    return message;
}

void TOptionsMenuDialogController::open(const int category)
{
    m_showAdvancedOptions = app::ClientOptions::load().options().value(QLatin1String("optionsShowAdvanced")).toBool(false);
    Q_EMIT showAdvancedOptionsChanged();
    EDialogCategories target = m_dialogCategory;
    switch (category) {
    case 0:
        target = m_showAdvancedOptions ? Controls : Basic;
        break;
    case 1:
        target = m_showAdvancedOptions ? CustomHotkeys : DefaultHotkeys;
        break;
    case 2:
        target = ButtonBar;
        break;
    default:
        break;
    }
    setDialogCategory(target);

    const config::TClientOptions& options = *m_clientOptions;
    for (const Page& page : m_pages)
        page.page->load(options);
    dialog()->show();
}

void TOptionsMenuDialogController::setInGame(const bool inGame)
{
    if (m_isInGame == inGame)
        return;
    // CIP: a new game session closes the dialog first.
    if (dialog()->item())
        dialog()->close();
    m_isInGame = inGame;
    Q_EMIT gameSessionChanged();
}

void TOptionsMenuDialogController::addProductPage(const int category, config::TOptionsPage* page)
{
    m_pages.push_back({ static_cast<EDialogCategories>(category), page });
}

void TOptionsMenuDialogController::afterShow()
{
    m_frameRateTimer.start();
}

void TOptionsMenuDialogController::afterHide()
{
    m_frameRateTimer.stop();
}

void TOptionsMenuDialogController::setDialogCategory(const EDialogCategories Category)
{
    if (m_dialogCategory == Category)
        return;
    m_dialogCategory = Category;
    if (Category >= DefaultHotkeys && Category <= CustomHotkeys)
        m_hotkeys->setHotkeyMode(static_cast<config::THotkeysOptionsPage::EHotkeyMode>(Category - DefaultHotkeys));
    Q_EMIT dialogCategoryChanged();
}

void TOptionsMenuDialogController::setShowAdvancedOptions(const bool Show)
{
    if (m_showAdvancedOptions == Show)
        return;
    m_showAdvancedOptions = Show;
    app::ClientOptions::store(QStringLiteral("optionsShowAdvanced"), Show);
    const EDialogCategories category = Show ? Controls : Basic;
    if (m_dialogCategory != category) {
        m_dialogCategory = category;
        Q_EMIT dialogCategoryChanged();
    }
    Q_EMIT showAdvancedOptionsChanged();
}

void TOptionsMenuDialogController::requestResetCurrentPageToDefault()
{
    // The three hotkey pages share one reset; the warning names them all.
    QString warning = qtTrId("optionsmenu_reset_current_page_warning");
    if (m_dialogCategory >= DefaultHotkeys && m_dialogCategory <= CustomHotkeys)
        warning = qtTrId("optionsmenu_reset_hotkeys_page_warning")
                      .arg(qtTrId("optionsmenu_individual_hotkeys"), qtTrId("optionsmenu_default_hotkeys"), qtTrId("optionsmenu_action_buttons_hotkeys"));
    m_resetPageMessage->setButtons(okCancelButtons());
    m_resetPageMessage->showMessage(qtTrId("optionsmenu_reset_current_page_caption"),
                                    warning + QStringLiteral("\n\n") + qtTrId("optionsmenu_reset_confirmation"));
}

void TOptionsMenuDialogController::requestResetAllOptionsToDefault()
{
    m_resetAllMessage->setButtons(okCancelButtons());
    m_resetAllMessage->showMessage(qtTrId("optionsmenu_reset_all_options"),
                                   qtTrId("optionsmenu_reset_all_options_warning") + QStringLiteral("\n\n") + qtTrId("optionsmenu_reset_confirmation"));
}

void TOptionsMenuDialogController::cancelButtonClicked()
{
    dialog()->close();
}

std::unique_ptr<config::TClientOptions> TOptionsMenuDialogController::pendingOptions() const
{
    auto options = std::make_unique<config::TClientOptions>();
    options->assignFrom(*m_clientOptions);
    for (const Page& page : m_pages)
        page.page->apply(*options);
    return options;
}

void TOptionsMenuDialogController::applyButtonClicked()
{
    const config::TClientOptions& current = *m_clientOptions;

    // The server learns the "allow all to inspect" switch.
    if (m_gameplay->allowAllToInspectPlayer() != current.boolOption(O::inspectPlayerAllowAllEnabled))
        Q_EMIT publishGameAction(simpleAction(m_gameplay->allowAllToInspectPlayer() ? EGameAction::ALLOW_ALL_TO_INSPECT_PLAYER : EGameAction::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER));

    // The server learns "always turn towards the move direction" when it changes.
    if (m_controls->alwaysTurnTowardsMoveDirection() != current.boolOption(O::alwaysTurnTowardsMoveDirection))
        sendTurnTowardsMoveDirection(m_controls->alwaysTurnTowardsMoveDirection());

    // No rotation key left.
    if (!m_controls->rotateWithAlt() && !m_controls->rotateWithCtrl() && !m_controls->rotateWithShift()) {
        m_rotationKeysMessage->setButtons(okButton());
        m_rotationKeysMessage->showMessage(qtTrId("warning"), qtTrId("optionsmenu_keys_for_rotation_no_key_warning"));
    }

    clearMarkedActionBars();

    // Every page into the options, in CipSoft's order.
    const std::unique_ptr<config::TClientOptions> pending = pendingOptions();

    // A renderer, vsync or font renderer change needs a restart.
    const TRendererOptions currentRenderer(current);
    const bool restartNeeded = TRendererOptions(*pending) != currentRenderer;
    if (restartNeeded) {
        // Keep the previous graphics trio so keep_renderer_after_restart can revert.
        app::graphicsrestart::markPendingConfirm(currentRenderer.renderer, currentRenderer.vSync, currentRenderer.freetype);
    }

    app::ClientOptions::storeChanged(*pending);

    if (restartNeeded)
        showRestartMessage(qtTrId("graphics_setting_requires_restart_caption"), qtTrId("graphics_setting_requires_restart_text"));
}

void TOptionsMenuDialogController::showRestartMessage(const QString& caption, const QString& text)
{
    // One "Restart" button (USER_DEFINED, no keys).
    m_restartMessage->setButtons({ new TMessageDialogButton(Button::USER_DEFINED, qtTrId("restart"), 0u) });
    m_restartMessage->showMessage(caption, text);
}

void TOptionsMenuDialogController::clearMarkedActionBars()
{
    const std::pair<bool, EActionBarOption> bars[] = {
        { m_actionBars->clearBottomActionBar1(), EActionBarOption::ShowBottomBar1 },
        { m_actionBars->clearBottomActionBar2(), EActionBarOption::ShowBottomBar2 },
        { m_actionBars->clearBottomActionBar3(), EActionBarOption::ShowBottomBar3 },
        { m_actionBars->clearLeftActionBar1(), EActionBarOption::ShowLeftBar1 },
        { m_actionBars->clearLeftActionBar2(), EActionBarOption::ShowLeftBar2 },
        { m_actionBars->clearLeftActionBar3(), EActionBarOption::ShowLeftBar3 },
        { m_actionBars->clearRightActionBar1(), EActionBarOption::ShowRightBar1 },
        { m_actionBars->clearRightActionBar2(), EActionBarOption::ShowRightBar2 },
        { m_actionBars->clearRightActionBar3(), EActionBarOption::ShowRightBar3 },
    };
    for (const auto& [clear, option] : bars)
        if (clear)
            m_hotkeys->clearActionBarMappings(TActionBarGroup::barOfShowOption(option));
    m_actionBars->resetClearFlags();
}

void TOptionsMenuDialogController::sendTurnTowardsMoveDirection(const bool enabled)
{
    const game::TGameSessionBase* session = game::currentGameSession();
    if (!session)
        return;
    ::tibia::protobuf::protocol::GameclientMessageSetClientOptions message;
    message.set_turn_towards_move_direction(enabled);
    Q_EMIT session->gameProtocolMessageHandler()->sendSetClientOptions(message);
}

void TOptionsMenuDialogController::okButtonClicked()
{
    applyButtonClicked();
    dialog()->close();
}

void TOptionsMenuDialogController::activateLensehelp()
{
    applyButtonClicked();
    dialog()->close();
    Q_EMIT publishGameAction(simpleAction(EGameAction::LENSHELP_ACTIVATE));
}

void TOptionsMenuDialogController::showRuleViolationsText()
{
    m_ruleViolationsMessage->setButtons(okButton());
    m_ruleViolationsMessage->showMessage(qtTrId("howtoruleviolationreport_caption"), qtTrId("howtoruleviolationreport_text"));
}

void TOptionsMenuDialogController::openManuelUrl()
{
    Q_EMIT publishGameAction(simpleAction(EGameAction::OPEN_URL_MANUAL));
}

void TOptionsMenuDialogController::openFaqUrl()
{
    Q_EMIT publishGameAction(simpleAction(EGameAction::OPEN_URL_FAQ));
}

void TOptionsMenuDialogController::openScreenshotDirectory()
{
    if (m_pFileSystemHelper)
        shared::TTibiaFileSystemHelper::openDirectoryInFileBrowser(m_pFileSystemHelper->getScreenshotsDirectory());
}

void TOptionsMenuDialogController::openHelpChannel()
{
    // CIP : apply, close, publish OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT
    // with THelpChannelIdentifier (server channel 7). Hotkey OpenHelpChannel
    // (Ctrl+T default) uses the same make-current join path.
    applyButtonClicked();
    dialog()->close();
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChatChannel>(
        EGameAction::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT,
        std::make_shared<const chat::TChatChannelIdentifier>(chat::TChatChannelIdentifier::help())));
}

void TOptionsMenuDialogController::setPortalBase(const QUrl& loginBase)
{
    if (m_about)
        m_about->setPortalBase(loginBase);
}

void TOptionsMenuDialogController::showAboutDialog()
{
    m_about->showDialog();
}

void TOptionsMenuDialogController::showCompendium()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenNewsDialog>());
}

void TOptionsMenuDialogController::onControlOptionsChanged()
{
    m_hotkeys->setRotateModifiers(m_controls->rotateWithAlt(), m_controls->rotateWithShift(), m_controls->rotateWithCtrl());
}

void TOptionsMenuDialogController::exportArchive(const QString& url, const QList<config::TArchiveEntry>& entries)
{
    // Throws std::runtime_error when the archive cannot be written.
    config::optionsarchive::write(localFile(url), entries);
    m_infoMessage->setButtons(okButton());
    m_infoMessage->showMessage(qtTrId("optionsmenu_file_export_success_caption"), qtTrId("optionsmenu_file_export_success_text"));
}

void TOptionsMenuDialogController::requestExportAllOptions(const QString& OutputFilename)
{
    QJsonObject root = app::ClientOptions::load().root();
    QJsonObject options = root.value(QLatin1String("options")).toObject();
    options.insert(QLatin1String("loginEmailAddress"), QString());
    root.insert(QLatin1String("options"), options);
    QList<config::TArchiveEntry> entries{ { QLatin1String(OptionsFile), QJsonDocument(root).toJson() } };
    entries << config::optionsarchive::collect(dataDirectory(), { QLatin1String(CharacterDataDirectory) });
    try {
        exportArchive(OutputFilename, entries);
    } catch (const std::exception& e) {
        showInfoMessage(qtTrId("optionsmenu_file_export_error_caption"), QString::fromUtf8(e.what()));
    }
}

void TOptionsMenuDialogController::showInfoMessage(const QString& caption, const QString& text)
{
    m_infoMessage->showMessage(caption, text, okButton());
}

void TOptionsMenuDialogController::requestImportAllOptions(const QString& InputFilename)
{
    m_importFile = InputFilename;
    m_importMessage->setButtons(okCancelButtons());
    m_importMessage->showMessage(qtTrId("optionsmenu_confirm_options_import_caption"), qtTrId("optionsmenu_confirm_options_import_text"));
}

void TOptionsMenuDialogController::requestExportMinimap(const QString& OutputFilename)
{
    if (m_pMinimapTileStorage)
        m_pMinimapTileStorage->saveAllTilesToDisk();
    // Host: qtclient's tile writes are asynchronous; the archive needs them done.
    if (m_pMinimapTileDiskIO)
        m_pMinimapTileDiskIO->waitForDone();
    try {
        exportArchive(OutputFilename, config::optionsarchive::collect(dataDirectory(), { QLatin1String(MinimapDirectory) }));
    } catch (const std::exception& e) {
        showInfoMessage(qtTrId("optionsmenu_file_export_error_caption"), QString::fromUtf8(e.what()));
    }
}

void TOptionsMenuDialogController::importArchive()
{
    const QDir root(dataDirectory());
    std::optional<config::TConfigBackupReader> reader;
    try {
        reader.emplace(m_pFileSystemHelper ? m_pFileSystemHelper->getFileSystemHelper() : files::data().getFileSystemHelper(),
                       localFile(m_importFile),
                       std::vector<QString>{ QLatin1String(ConfDirectory), QLatin1String(CharacterDataDirectory),
                                             QLatin1String(MinimapDirectory) });
    } catch (const std::runtime_error&) {
        // CipSoft catches std::runtime_error here (the handler body is not
        // decompiled); the import is dropped.
        return;
    }
    reader->extractTo(root);

    // Imported minimap files: the markers are read again and the minimap
    // shows the new tiles.
    if (reader->containsMinimap()) {
        if (m_pMinimapTileDiskIO)
            m_pMinimapTileDiskIO->loadMinimapMarkerFileContentFromDiskIfPossible();
        if (m_pMinimapTileStorage)
            Q_EMIT m_pMinimapTileStorage->reloadVisibleMinimap();
    }

    if (!reader->containsOptions())
        return;
    const TRendererOptions before(*m_clientOptions);
    // qtclient's TClientOptions keeps its own file: the extracted
    // conf/clientoptions.json is applied through ClientOptions.
    QFile optionsFile(root.filePath(QLatin1String(OptionsFile)));
    if (optionsFile.open(QIODevice::ReadOnly)) {
        const QJsonObject imported = QJsonDocument::fromJson(optionsFile.readAll()).object();
        if (!imported.isEmpty())
            app::ClientOptions::storeAll(imported);
    }

    const config::TClientOptions& after = *m_clientOptions;
    for (const Page& page : m_pages)
        page.page->load(after);
    // In game the character's configuration is read again from the
    // imported character data.
    if (game::TGameSessionBase* session = game::currentGameSession()) {
        if (session->characterConfigurationManager() && session->creatureStorage())
            session->characterConfigurationManager()->loadConfigurationForCharacter(session->creatureStorage()->playerID());
    }

    if (TRendererOptions(after) == before) {
        m_infoMessage->setButtons(okButton());
        m_infoMessage->showMessage(qtTrId("optionsmenu_file_import_success_caption"), qtTrId("optionsmenu_file_import_success_text"));
    } else {
        // A renderer, vsync or font renderer change takes a restart.
        showRestartMessage(qtTrId("optionsmenu_file_import_info_caption"), qtTrId("optionsmenu_file_import_info_text"));
    }
}

config::TOptionsPage* TOptionsMenuDialogController::pageOf(const EDialogCategories category) const
{
    for (const Page& page : m_pages)
        if (page.category == category)
            return page.page;
    return nullptr;
}

void TOptionsMenuDialogController::resetCurrentPageToDefault()
{
    resetPageToDefault(m_dialogCategory);
}

void TOptionsMenuDialogController::resetPageToDefault(const EDialogCategories category)
{
    switch (category) {
    case Basic:
        resetBasicPage();
        return;
    case DefaultHotkeys:
    case ActionButtonsHotkeys:
    case CustomHotkeys:
        if (m_hotkeys)
            m_hotkeys->resetToDefault();
        return;
    case NoCategoryInitialState:
    case Help:
        return;
    default:
        break;
    }
    if (config::TOptionsPage* page = pageOf(category))
        page->load(*defaultOptions());
}

void TOptionsMenuDialogController::resetBasicPage()
{
    static constexpr O BasicOptions[] = {
        O::controlSchemeIndex, O::lootSchemeIndex,
        O::inspectPlayerAllowAllEnabled, O::autoChaseEnabled, O::quickLootAllCorpsesInAreaEnabled,
        O::playerHudShowBars, O::playerHudShowArcs, O::playerHudShowHarmonyLeft,
        O::actionBarShowBottom1, O::actionBarShowBottom2, O::actionBarShowBottom3, O::actionBarsShowBottom,
        O::actionBarShowLeft1, O::actionBarShowLeft2, O::actionBarShowLeft3, O::actionBarsShowLeft,
        O::actionBarShowRight1, O::actionBarShowRight2, O::actionBarShowRight3, O::actionBarsShowRight,
        O::antialiasingMode, O::soundMasterVolume,
    };
    const std::unique_ptr<config::TClientOptions> settings = pendingOptions();
    const std::unique_ptr<config::TClientOptions> defaults = defaultOptions();
    for (const O option : BasicOptions)
        settings->assignOption(option, *defaults);
    config::TClientWindowOptions window = settings->clientWindowOptions();
    window.borderless = false;
    settings->setClientWindowOptions(window);
    for (const EDialogCategories category : { Controls, Gameplay, HUD, ActionBars, Graphics, Sound })
        pageOf(category)->load(*settings);
}

void TOptionsMenuDialogController::resetAllOptionsToDefault()
{
    // CIP leaves the hotkey and sound pages as they are.
    const std::unique_ptr<config::TClientOptions> defaults = defaultOptions();
    for (const EDialogCategories category :
         { Controls, Interface, HUD, Console, GameWindow, ActionBars, ButtonBar, Graphics, Effects, Misc, Gameplay, Screenshots }) {
        if (config::TOptionsPage* page = pageOf(category))
            page->load(*defaults);
    }
}

void TOptionsMenuDialogController::onResartMessageControllerClicked(const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier)
{
    // CANCEL/NO: dismiss only — settings are already stored for the next launch.
    if (ButtonIdentifier != Button::USER_DEFINED)
        return;
    // OPEN_LAUNCHER starts this client again and quits without the exit prompt.
    Q_EMIT publishGameAction(simpleAction(EGameAction::OPEN_LAUNCHER));
}

}
