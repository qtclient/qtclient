#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "config/optionsarchive.h"
#include "config/clientoptionsmodeswitcher.h"
#include "config/configciptypes.h"
#include "config/optionspages.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace game {
class IGameSession;
}
namespace client {
class TGameClient;
}
namespace shared {
class TTibiaFileSystemHelper;
}
namespace systemsurvey {
class TSlidingFPSCounter;
}
namespace input {
class TEventFilterManager;
}
namespace appearances {
class TAppearanceInstanceFactory;
}
namespace minimap {
class TMinimapDiskIOAsync;
class TMinimapTileStorage;
}
namespace spells {
class TSpellInfoStorage;
}
}

namespace tibia {
namespace gamewindow {

class TAboutDialogController;
class TMessageDialogController;

/// OptionsMenuDialog.qml (CIP: TOptionsMenuDialogController, ctor ,
/// vtable , metacall). Each page holds pending values
/// loaded when the dialog opens; Apply / OK write them, Cancel drops them.
/// Only "show advanced options" is written at once.
class TOptionsMenuDialogController final : public TDialogController<cip::tibia::gamewindow::TOptionsMenuDialogController>
{
    Q_OBJECT
public:
    /// The client's options, its frame rate counter, file system helper, event
    /// filter manager, appearance instance factory, minimap disk IO and tile
    /// storage and spell info storage; the dialog and its message dialogs are
    /// made with the QML engine under the dialog parent item.
    TOptionsMenuDialogController(config::TClientOptions* ClientOptions, systemsurvey::TSlidingFPSCounter* SlidingFPSCounter,
                                 std::shared_ptr<shared::TTibiaFileSystemHelper> FileSystemHelper,
                                 input::TEventFilterManager* EventFilterManager,
                                 std::shared_ptr<appearances::TAppearanceInstanceFactory> AppearanceInstanceFactory,
                                 std::shared_ptr<minimap::TMinimapDiskIOAsync> MinimapTileDiskIO,
                                 std::shared_ptr<minimap::TMinimapTileStorage> MinimapTileStorage,
                                 spells::TSpellInfoStorage* SpellInfoStorage, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                                 QObject* parent = nullptr);
    ~TOptionsMenuDialogController() override;

    /// OPEN_OPTIONS_MENU_DIALOG (CIP , action 0x54): the page of
    /// `category` (: 0 general, 1 hotkeys, 2 button bar), every
    /// page loaded from the options , then shown.
    void open(int category);
    /// CIP handleGameAction: OPEN_OPTIONS_MENU_DIALOG opens the page of its parameter;
    /// TARGET_SELECTION_FINISHED shows the dialog again when a hotkey's target selection hid it.
    bool handleGameAction(const input::IGameAction& action);
    /// The game session (CIP +0xc08) began or ended.
    void setInGame(bool inGame);
    /// The hotkeys page's spell assign dialog follows the session.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession) { m_hotkeys->connectToGameSession(GameSession); }
    /// CIP: how the pages publish game actions. START_TARGET_SELECTION hides
    /// the dialog until TARGET_SELECTION_FINISHED.
    void processAndPublishGameActions(const input::IGameActionPtr& action);
    /// product: a product page (category past CIP's) loaded, applied and reset like CIP's own.
    void addProductPage(int category, config::TOptionsPage* page);
    /// The client whose minimap the minimap export saves first (CIP +0xc0).
    void setGameClient(client::TGameClient* client);
    /// Portal origin for About / Website (LauncherEnv::loginBase).
    void setPortalBase(const QUrl& loginBase);

    /// CIP .
    void setDialogCategory(EDialogCategories Category) override;
    /// CIP : written to option 0x83 at once.
    void setShowAdvancedOptions(bool Show) override;
    /// CIP / : asks first.
    void requestResetCurrentPageToDefault() override;
    void requestResetAllOptionsToDefault() override;
    /// CIP .
    void cancelButtonClicked() override;
    /// CIP .
    void applyButtonClicked() override;
    /// CIP .
    void okButtonClicked() override;
    /// CIP : apply, then LENSHELP_ACTIVATE.
    void activateLensehelp() override;
    /// CIP .
    void showRuleViolationsText() override;
    /// CIP.
    void openManuelUrl() override;
    void openFaqUrl() override;
    /// CIP .
    void openScreenshotDirectory() override;
    /// CIP : apply, close, then OPEN_CHAT_CHANNEL of the Help
    /// channel (THelpChannelIdentifier: server channel 7).
    void openHelpChannel() override;
    /// CIP .
    void showAboutDialog() override;
    /// CIP : OPEN_NEWS_DIALOG.
    void showCompendium() override;
    /// CIP : conf/clientoptions.json (without loginEmailAddress)
    /// and characterdata/ into a zip archive.
    void requestExportAllOptions(const QString& OutputFilename) override;
    /// CIP : asks first, then imports .
    void requestImportAllOptions(const QString& InputFilename) override;
    /// CIP : minimap/ into a zip archive.
    void requestExportMinimap(const QString& OutputFilename) override;
    /// CIP : the current page from CIP's defaults .
    void resetCurrentPageToDefault() override;
    /// CIP: every page but the hotkey and sound pages.
    void resetAllOptionsToDefault() override;
    /// CIP : "Restart" publishes OPEN_LAUNCHER.
    void onResartMessageControllerClicked(cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier) override;

    QString currentFrameRate() const override;

    void afterShow() override;
    void afterHide() override;

private:
    struct Page
    {
        EDialogCategories category;
        config::TOptionsPage* page;
    };

    /// The pages in CIP's apply order .
    std::vector<Page> m_pages;
    config::TActionBarsOptionsPage* m_actionBars;
    config::TGameplayOptionsPage* m_gameplay;
    config::TControlsOptionsPage* m_controls;
    config::THotkeysOptionsPage* m_hotkeys;

    config::TOptionsPage* pageOf(EDialogCategories category) const;
    /// CIP: one page from the defaults (Basic and the hotkey pages their own way).
    void resetPageToDefault(EDialogCategories category);
    /// CIP : the Basic page's options from the defaults.
    void resetBasicPage();
    /// A copy of the client's options with every page's pending values.
    std::unique_ptr<config::TClientOptions> pendingOptions() const;
    TMessageDialogController* makeMessageDialog(QQmlEngine* Engine, QQuickItem* DialogParentItem);
    /// CIP : conf, characterdata and minimap files from the archive.
    void importArchive();
    /// The controls page's rotate modifiers go to the hotkeys page.
    void onControlOptionsChanged();
    void exportArchive(const QString& url, const QList<config::TArchiveEntry>& entries);
    /// The info message with the OK button.
    void showInfoMessage(const QString& caption, const QString& text);
    /// CIP : GameclientMessageSetClientOptions with alwaysTurnTowardsMoveDirection.
    void sendTurnTowardsMoveDirection(bool enabled);
    /// The hotkey page loses the button mappings of every action bar whose
    /// clear flag the action bar page set; the flags are reset after.
    void clearMarkedActionBars();
    /// CIP v14: closing the dialog closes its message dialogs (not the restart
    /// and rotation key messages Apply shows), the about dialog and the hotkey
    /// page's dialogs, then saves the options file.
    void closeSubDialogs();
    /// CIP: the shown frame rate from the frame timing, set when it changed.
    void onBeforeRenderFrame();
    void setFrameRate(double frameRate);

    /// CIP +0x80.
    config::TClientOptions* m_clientOptions;
    /// The client's frame rate over the last second (currentFrameRate).
    systemsurvey::TSlidingFPSCounter* m_pSlidingFPSCounter;
    /// The screenshot directory and the export / import paths.
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_pFileSystemHelper;
    std::shared_ptr<minimap::TMinimapDiskIOAsync> m_pMinimapTileDiskIO;
    /// Saved to disk before the minimap export.
    std::shared_ptr<minimap::TMinimapTileStorage> m_pMinimapTileStorage;
    /// Handed to the hotkeys page (key recording, the action icons, the spell
    /// list).
    input::TEventFilterManager* m_pEventFilterManager;
    std::shared_ptr<appearances::TAppearanceInstanceFactory> m_pAppearanceInstanceFactory;
    spells::TSpellInfoStorage* m_pSpellInfoStorage;
    /// CIP config surfaces owned for the options dialog lifetime.
    config::TOptionPageQMLWrapperBase m_optionPageQmlWrapperBase;

    /// CIP's message dialogs in layout order.
    TMessageDialogController* m_ruleViolationsMessage;
    TMessageDialogController* m_restartMessage;
    TMessageDialogController* m_rotationKeysMessage;
    TMessageDialogController* m_resetPageMessage;
    TMessageDialogController* m_resetAllMessage;
    /// The export and import results.
    TMessageDialogController* m_infoMessage;
    TMessageDialogController* m_importMessage;
    TAboutDialogController* m_about;
    /// CIP +0xbf0.
    QString m_importFile;
    /// CIP +0xbe8: hidden for a hotkey's target selection.
    bool m_hiddenForTargetSelection = false;
    client::TGameClient* m_gameClient = nullptr;

    QTimer m_frameRateTimer;
    /// CIP +0xbe0.
    double m_frameRate = 0.0;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
