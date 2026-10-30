#pragma once

#include "client/gamecore.h"
#include "config/optionsarchive.h"
#include "config/configciptypes.h"
#include "config/optionspages.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <vector>

class QQuickWindow;

namespace tibia::client {
class TGameClient;
}

namespace tibia::gamewindow {

class TAboutDialogController;
class TMessageDialogController;

/// OptionsMenuDialog.qml (CIP: TOptionsMenuDialogController, ctor 0x100d33a30,
/// vtable 0x102635710, metacall 0x1008343c0). Each page holds pending values
/// loaded when the dialog opens; Apply / OK write them, Cancel drops them.
/// Only "show advanced options" is written at once.
class TOptionsMenuDialogController final : public TDialogController<cip::tibia::gamewindow::TOptionsMenuDialogController>
{
    Q_OBJECT
public:
    explicit TOptionsMenuDialogController(QObject* parent = nullptr);
    ~TOptionsMenuDialogController() override;

    /// OPEN_OPTIONS_MENU_DIALOG (CIP 0x100d39480, action 0x54): the page of
    /// `category` (FUN_100d36280: 0 general, 1 hotkeys, 2 button bar), every
    /// page loaded from the options (FUN_100d36390), then shown.
    void open(int category);
    /// The game session (CIP +0xc08) began or ended.
    void setInGame(bool inGame);
    /// The window whose frames currentFrameRate counts (CIP +0xbe0).
    void setWindow(QQuickWindow* window);
    /// product: a product page (category past CIP's) loaded, applied and reset like CIP's own.
    void addProductPage(int category, config::TOptionsPage* page);
    /// The client whose minimap the minimap export saves first (CIP +0xc0).
    void setGameClient(client::TGameClient* client) { m_gameClient = client; }
    /// Portal origin for About / Website (LauncherEnv::loginBase).
    void setPortalBase(const QUrl& loginBase);

    /// CIP 0x100d36ff0.
    void setDialogCategory(EDialogCategories Category) override;
    /// CIP 0x100d37040: written to option 0x83 at once.
    void setShowAdvancedOptions(bool Show) override;
    /// CIP 0x100d370f0 / 0x100d37610: asks first.
    void requestResetCurrentPageToDefault() override;
    void requestResetAllOptionsToDefault() override;
    /// CIP 0x100d378d0.
    void cancelButtonClicked() override;
    /// CIP 0x100d378e0.
    void applyButtonClicked() override;
    /// CIP 0x100d37ae0.
    void okButtonClicked() override;
    /// CIP 0x100d37b00: apply, then LENSHELP_ACTIVATE.
    void activateLensehelp() override;
    /// CIP 0x100d37d60.
    void showRuleViolationsText() override;
    /// CIP 0x100d37ed0 / 0x100d37fb0.
    void openManuelUrl() override;
    void openFaqUrl() override;
    /// CIP 0x100d392d0.
    void openScreenshotDirectory() override;
    /// CIP 0x100d37bf0: apply, close, then OPEN_CHAT_CHANNEL of the Help
    /// channel (THelpChannelIdentifier: server channel 7).
    void openHelpChannel() override;
    /// CIP 0x100d37eb0.
    void showAboutDialog() override;
    /// CIP 0x100d38090: OPEN_NEWS_DIALOG.
    void showCompendium() override;
    /// CIP 0x100d38190: conf/clientoptions.json (without loginEmailAddress)
    /// and characterdata/ into a zip archive.
    void requestExportAllOptions(const QString& OutputFilename) override;
    /// CIP 0x100d38c20: asks first, then imports (0x100d34d40).
    void requestImportAllOptions(const QString& InputFilename) override;
    /// CIP 0x100d38d80: minimap/ into a zip archive.
    void requestExportMinimap(const QString& OutputFilename) override;
    /// CIP 0x100d34a80: the current page from CIP's defaults (FUN_1009bb440).
    void resetCurrentPageToDefault() override;
    /// CIP 0x100d34bd0: every page but the sound and hotkey pages.
    void resetAllOptionsToDefault() override;
    /// CIP 0x100d35820: "Restart" publishes OPEN_LAUNCHER.
    void onResartMessageControllerClicked(cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier) override;

    void afterShow() override;
    void afterHide() override;

private:
    struct Page
    {
        EDialogCategories category;
        config::TOptionsPage* page;
    };

    /// The pages in CIP's apply order (0x100d378e0).
    std::vector<Page> m_pages;
    config::TGameplayOptionsPage* m_gameplay;
    config::TControlsOptionsPage* m_controls;
    config::THotkeysOptionsPage* m_hotkeys;

    config::TOptionsPage* pageOf(EDialogCategories category) const;
    /// CIP 0x100d395a0: the Basic page's options from the defaults.
    void resetBasicPage();
    /// The options as the pages would write them.
    QJsonObject pendingSettings() const;
    TMessageDialogController* makeMessageDialog();
    /// CIP 0x100d34d40: conf, characterdata and minimap files from the archive.
    void importArchive();
    void exportArchive(const QString& url, const QList<config::TArchiveEntry>& entries);
    /// CIP FUN_100adf300: GameclientMessageSetClientOptions with alwaysTurnTowardsMoveDirection.
    void sendTurnTowardsMoveDirection(bool enabled);

    /// CIP config surfaces owned for the options dialog lifetime.
    config::TActionBarOptions m_actionBarOptions;
    config::TCharacterConfigurationManager m_characterConfigurationManager;
    config::TClientOptions m_clientOptions;
    config::TClientOptionsModeSwitcher m_clientOptionsModeSwitcher;
    config::TOptionPageQMLWrapperBase m_optionPageQmlWrapperBase;
    config::TConfigBackupReader::TConfigBackupException m_configBackupException;

    TMessageDialogController* m_resetPageMessage;
    TMessageDialogController* m_resetAllMessage;
    TMessageDialogController* m_restartMessage;
    TMessageDialogController* m_infoMessage;
    TMessageDialogController* m_importMessage;
    TAboutDialogController* m_about;
    /// CIP +0xbf0.
    QString m_importFile;
    client::TGameClient* m_gameClient = nullptr;
    client::gamecore::Hook m_gameStartHook;

    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_frameConnection;
    std::atomic<int> m_frames { 0 };
    QTimer m_frameRateTimer;
};

}
