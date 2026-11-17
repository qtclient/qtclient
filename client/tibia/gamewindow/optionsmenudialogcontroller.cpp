#include "optionsmenudialogcontroller.h"

#include "config/clientoptions.h"

#include "app/clientoptions.h"
#include "app/graphicsrestart.h"
#include "chat/chatchannel.h"
#include "client/gameclient.h"
#include "config/clientoptiondefaults.h"
#include "config/hotkeyoptions.h"
#include "gamewindow/aboutdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "minimap/minimapmanager.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "worldmap/nextframetimestamppredictor.h"

#include <tibia/game/gamesession.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>

#include <memory>
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

void publish(const EGameAction type)
{
    input::publish(input::makeAction(type));
}

/// CIP DAT_102793370 and the storage directories (,
/// import list DAT_102793700 / DAT_102793800 / DAT_1027936a0).
constexpr auto OptionsFile = "conf/clientoptions.json";
constexpr auto ConfDirectory = "conf";
constexpr auto CharacterDataDirectory = "characterdata";
constexpr auto MinimapDirectory = "minimap";
/// GameclientMessageSetClientOptions oneof case 1 .
constexpr uint8_t TurnTowardsMoveDirectionOption = 1;

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

TOptionsMenuDialogController::TOptionsMenuDialogController(QObject* parent)
    : TDialogController(QStringLiteral("OptionsMenuDialog.qml"), parent)
    , m_clientOptionsModeSwitcher(config::TClientOptions::instance(), this)
    , m_optionPageQmlWrapperBase(this)
{
    (void)m_configBackupException;
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
    m_hotkeys = new config::THotkeysOptionsPage(this);
    m_controls = new config::TControlsOptionsPage(this);
    auto* screenshots = new config::TScreenshotsOptionsPage(config::screenshotsBindings(), this);
    auto* sound = new config::TSoundOptionsPage(this);
    auto* battleSounds = new config::TBattleSoundsOptionsPage(config::battleSoundsBindings(), this);
    auto* uiSounds = new config::TUISoundsOptionsPage(config::uiSoundsBindings(), this);

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

    m_resetPageMessage = makeMessageDialog();
    m_resetAllMessage = makeMessageDialog();
    m_restartMessage = makeMessageDialog();
    m_infoMessage = makeMessageDialog();
    m_importMessage = makeMessageDialog();
    m_about = new TAboutDialogController(this);
    connect(m_importMessage, &TMessageDialogController::acceptClicked, this, &TOptionsMenuDialogController::importArchive);
    connect(m_resetPageMessage, &TMessageDialogController::acceptClicked, this, &TOptionsMenuDialogController::resetCurrentPageToDefault);
    connect(m_resetAllMessage, &TMessageDialogController::acceptClicked, this, &TOptionsMenuDialogController::resetAllOptionsToDefault);
    connect(m_restartMessage, &TMessageDialogController::buttonClicked, this, &TOptionsMenuDialogController::onResartMessageControllerClicked);

    m_showAdvancedOptions = app::ClientOptions::load().options().value(QLatin1String("optionsShowAdvanced")).toBool(false);

    // (connected when the game connection is set): the current value at login.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this] {
        sendTurnTowardsMoveDirection(config::TClientOptions::instance()->boolOption(O::alwaysTurnTowardsMoveDirection));
    });


    m_frameRateTimer.setInterval(1000);
    connect(&m_frameRateTimer, &QTimer::timeout, this, [this] {
        // CipSoft / : ring-derived double, QString::number(d, 'f', 2).
        (void)m_frames.exchange(0);
        double fps = 0.0;
        if (m_frameDurations) {
            const qint64 averageNs = m_frameDurations->averageNs();
            if (averageNs > 0)
                fps = 1'000'000'000.0 / static_cast<double>(averageNs);
        }
        m_currentFrameRate = QString::number(fps, 'f', 2);
        Q_EMIT frameRateChanged();
    });
}

TOptionsMenuDialogController::~TOptionsMenuDialogController()
{
    disconnect(m_frameConnection);
}

void TOptionsMenuDialogController::setGameClient(client::TGameClient* client)
{
    m_gameClient = client;
    m_clientOptionsModeSwitcher.connectToGameSession([client] { return client ? client->sidebarOptions() : nullptr; });
}

TMessageDialogController* TOptionsMenuDialogController::makeMessageDialog()
{
    auto* message = new TMessageDialogController(this);
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

    const config::TClientOptions& options = *config::TClientOptions::instance();
    for (const Page& page : m_pages)
        page.page->load(options);
    dialog()->show();
}

void TOptionsMenuDialogController::setInGame(const bool inGame)
{
    if (m_isInGame == inGame)
        return;
    m_isInGame = inGame;
    Q_EMIT gameSessionChanged();
}

void TOptionsMenuDialogController::setWindow(QQuickWindow* window)
{
    disconnect(m_frameConnection);
    m_window = window;
    if (window)
        m_frameConnection = connect(window, &QQuickWindow::frameSwapped, this, [this] { ++m_frames; }, Qt::DirectConnection);
}

void TOptionsMenuDialogController::setFrameDurations(worldmap::TFrameDurationRing* ring)
{
    m_frameDurations = ring;
}

void TOptionsMenuDialogController::addProductPage(const int category, config::TOptionsPage* page)
{
    m_pages.push_back({ static_cast<EDialogCategories>(category), page });
}

void TOptionsMenuDialogController::afterShow()
{
    m_frames = 0;
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
    options->assignFrom(*config::TClientOptions::instance());
    for (const Page& page : m_pages)
        page.page->apply(*options);
    return options;
}

void TOptionsMenuDialogController::applyButtonClicked()
{
    const config::TClientOptions& current = *config::TClientOptions::instance();

    // 
    if (m_gameplay->allowAllToInspectPlayer() != current.boolOption(O::inspectPlayerAllowAllEnabled))
        publish(m_gameplay->allowAllToInspectPlayer() ? EGameAction::ALLOW_ALL_TO_INSPECT_PLAYER : EGameAction::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER);

    // 
    if (!m_controls->rotateWithAlt() && !m_controls->rotateWithCtrl() && !m_controls->rotateWithShift()) {
        m_infoMessage->setButtons(okButton());
        m_infoMessage->showMessage(qtTrId("warning"), qtTrId("optionsmenu_keys_for_rotation_no_key_warning"));
    }

    const std::unique_ptr<config::TClientOptions> pending = pendingOptions();

    const TRendererOptions currentRenderer(current);
    const bool restartNeeded = TRendererOptions(*pending) != currentRenderer;

    // : the server learns the option when it changes.
    const bool turnTowards = pending->boolOption(O::alwaysTurnTowardsMoveDirection);
    if (turnTowards != current.boolOption(O::alwaysTurnTowardsMoveDirection))
        sendTurnTowardsMoveDirection(turnTowards);

    if (restartNeeded) {
        // Keep the previous graphics trio so keep_renderer_after_restart can revert.
        app::graphicsrestart::markPendingConfirm(currentRenderer.renderer, currentRenderer.vSync, currentRenderer.freetype);
    }

    app::ClientOptions::storeChanged(*pending);

    if (restartNeeded) {
        // Restart (Return) quits into the new engine; Cancel (Esc) keeps the new
        // settings for the next manual restart — CIP builds Restart;
        // Cancel matches the two-button UX (dismiss without quitting now).
        m_restartMessage->setButtons({
            new TMessageDialogButton(qtTrId("restart"), Button::USER_DEFINED, QKeySequence(QStringLiteral("Return"))),
            new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))),
        });
        m_restartMessage->showMessage(qtTrId("graphics_setting_requires_restart_caption"),
                                      qtTrId("graphics_setting_requires_restart_text"));
    }
}

void TOptionsMenuDialogController::sendTurnTowardsMoveDirection(const bool enabled)
{
    tibia::game::setClientOption(TurnTowardsMoveDirectionOption, enabled);
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
    publish(EGameAction::LENSHELP_ACTIVATE);
}

void TOptionsMenuDialogController::showRuleViolationsText()
{
    m_infoMessage->setButtons(okButton());
    m_infoMessage->showMessage(qtTrId("howtoruleviolationreport_caption"), qtTrId("howtoruleviolationreport_text"));
}

void TOptionsMenuDialogController::openManuelUrl()
{
    publish(EGameAction::OPEN_URL_MANUAL);
}

void TOptionsMenuDialogController::openFaqUrl()
{
    publish(EGameAction::OPEN_URL_FAQ);
}

void TOptionsMenuDialogController::openScreenshotDirectory()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/screenshots");
    QDir().mkpath(directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void TOptionsMenuDialogController::openHelpChannel()
{
    // CIP : apply, close, publish OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT
    // with THelpChannelIdentifier (server channel 7). Hotkey OpenHelpChannel
    // (Ctrl+T default) uses the same make-current join path.
    applyButtonClicked();
    dialog()->close();
    const chat::TChatChannelIdentifier help = chat::TChatChannelIdentifier::help();
    auto action = input::mutableClone(*input::makeAction(EGameAction::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT));
    action->chatChannelType = help.type;
    action->chatChannelId = help.id;
    input::publish(action);
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
    publish(EGameAction::OPEN_NEWS_DIALOG);
}

void TOptionsMenuDialogController::exportArchive(const QString& url, const QList<config::TArchiveEntry>& entries)
{
    if (!config::optionsarchive::write(localFile(url), entries))
        return;
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
    exportArchive(OutputFilename, entries);
}

void TOptionsMenuDialogController::requestImportAllOptions(const QString& InputFilename)
{
    m_importFile = InputFilename;
    m_importMessage->setButtons(okCancelButtons());
    m_importMessage->showMessage(qtTrId("optionsmenu_confirm_options_import_caption"), qtTrId("optionsmenu_confirm_options_import_text"));
}

void TOptionsMenuDialogController::requestExportMinimap(const QString& OutputFilename)
{
    if (minimap::TMinimapManager* minimap = m_gameClient ? m_gameClient->minimap() : nullptr)
        minimap->flushToDisk();
    exportArchive(OutputFilename, config::optionsarchive::collect(dataDirectory(), { QLatin1String(MinimapDirectory) }));
}

void TOptionsMenuDialogController::importArchive()
{
    const QDir root(dataDirectory());
    const TRendererOptions before(*config::TClientOptions::instance());
    bool optionsImported = false;
    for (const config::TArchiveEntry& entry : config::optionsarchive::read(localFile(m_importFile))) {
        const QString name = QDir::cleanPath(entry.name);
        const QString top = name.section(QLatin1Char('/'), 0, 0);
        if (name.startsWith(QLatin1Char('/')) || name.contains(QLatin1String(".."))
            || (top != QLatin1String(ConfDirectory) && top != QLatin1String(CharacterDataDirectory) && top != QLatin1String(MinimapDirectory)))
            continue;
        if (name == QLatin1String(OptionsFile)) {
            const QJsonObject imported = QJsonDocument::fromJson(entry.data).object();
            if (!imported.isEmpty()) {
                app::ClientOptions::storeAll(imported);
                optionsImported = true;
            }
            continue;
        }
        const QString path = root.filePath(name);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(entry.data);
    }
    if (!optionsImported)
        return;

    const config::TClientOptions& after = *config::TClientOptions::instance();
    for (const Page& page : m_pages)
        page.page->load(after);
    const bool same = TRendererOptions(after) == before;
    m_infoMessage->setButtons(okButton());
    if (same)
        m_infoMessage->showMessage(qtTrId("optionsmenu_file_import_success_caption"), qtTrId("optionsmenu_file_import_success_text"));
    else
        m_infoMessage->showMessage(qtTrId("optionsmenu_file_import_info_caption"), qtTrId("optionsmenu_file_import_info_text"));
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
    publish(EGameAction::OPEN_LAUNCHER);
}

}
