#pragma once

#include "input/gameactiondispatch.h"
#include <cip_gen.h>

#include <QPointer>
#include <QTimer>
#include <QString>
#include <QUrl>

#include <memory>

#include "randomhints.h"
#include "news/eventsuploader.h"

#include <vector>

class QEvent;
class QQmlEngine;
class QObject;
class QQuickItem;

namespace tibia {
namespace input {
class TEventFilterManager;
}
namespace appearances {
class TAppearanceInstanceFactory;
}
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
class TGameActionRouter;
class TBoostedCreatureRequestUploader;
class TLiveInformationRequestUploader;
class TShowOffRequestUploader;
}
}
namespace tibia {
namespace shared {
class IClockService;
class TTibiaFileSystemHelper;
}
}

namespace tibia {
namespace gamewindow {

class TStatusBarController;

/// gamewindow.qml's controller (CIP: TGameWindowController, metacall
/// ). Without launcher SSO / --account the start-screen login
/// mask is shown (`isAuthenticated` false); otherwise the launcher (or CLI)
/// authenticates and the character list opens. Start-screen HTTP services
/// (boosted creature, live info / cacheinfo, showoff) are requested via CIP
/// uploaders against /clientservices/clientservices.php; the client's events
/// uploader answers onEventScheduleUpdated.
class TGameWindowController final : public cip::tibia::gamewindow::TGameWindowController
{
    Q_OBJECT
public:
    /// CIP: made with the game action router, the appearance instance factory,
    /// the engine, the client options, the client window (qtclient:
    /// clientwindow.qml's placeholder, gamewindowContainer), the hints URL, the
    /// client services URL (the boosted creature and live information
    /// uploaders ask it), the file system helper and the client's clock.
    TGameWindowController(std::shared_ptr<game::TGameActionRouter> GameActionRouter,
                          std::shared_ptr<appearances::TAppearanceInstanceFactory> AppearanceInstanceFactory,
                          QQmlEngine* Engine, config::TClientOptions* ClientOptions, QQuickItem* ClientWindow,
                          std::shared_ptr<input::TEventFilterManager> EventFilterManager, const QUrl& HintsUrl, const QUrl& ClientServicesUrl,
                          std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                          std::shared_ptr<shared::IClockService> ClockService, QObject* parent = nullptr);
    ~TGameWindowController() override;

    /// CIP: build-flavour constants. qtclient ships as a release build: the
    /// regular dragon logo (test-server builds show the test-server logo) and
    /// account creation on (off in test-server builds).
    QString dragonLogoSource() const override;
    bool createAccountEnabled() const override;

    /// CIP: hands the session to the status bar controller (the game window
    /// controller's own, made with the client options).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// gamewindow.qml's item, once it is shown: applies the option-driven game
    /// window state now and again on every option change.
    void setGameWindowItem(QQuickItem* gameWindowItem);
    /// CIP: gamewindow.qml made in the container with this controller, the background art on, the
    /// chat-input focus signals wired to the item's functions. The caller keeps the item alive
    /// (null and the QML error logged when it fails).
    std::unique_ptr<QObject> createGameWindow(QQmlEngine* engine);
    /// CIP: gamewindow.qml's item named `name` (dialogPlaceholder, mapWindowPane, the sidebar
    /// placeholders and managers, the action bars, cooldownBar, ...); null before the item is set.
    QQuickItem* gameWindowChild(const QString& name) const;
    /// The container's backgroundImageSmooth (option antialiasingMode is
    /// antialiasing), the cooldown bar's visibility (option cooldownBarEnabled)
    /// and the pane layout (options gameWindowUpperPaneHeight,
    /// gameWindowLowerPaneHeight, gameWindowResizingMode).
    void onClientOptionsChangedAfterShowGameWindows() override;
    /// CIP: before each frame the game window's render driver is polished and
    /// its window asked for the next frame.
    void requestRenderDriverUpdate();

    /// Button-bar Compendium / News button (CIP TNewsButtonController).

    /// CIP clientservices URL (`LauncherEnv::clientServicesUrl`) and optional
    /// client revision header.
    void configureClientServices(const QUrl& url, const QString& clientRevision = {});

    /// CIP : state "INGAME", no background art, black window.
    void showGameScreen();
    /// CIP : state "", the background art, window colour #041020.
    void showStartScreen();

    /// Hide the login mask once account login succeeded (or when SSO / --account starts).
    void setAuthenticated(bool authenticated);

    /// Persist `email` when remember-email is on (password login and SSO).
    void rememberLoginEmailIfEnabled(const QString& email);

    /// Options gameWindowUpperPaneHeight, gameWindowLowerPaneHeight and
    /// gameWindowResizingMode: a changed value is stored and layoutChanged emitted.
    void setUpperPaneHeight(uint height) override;
    /// CIP: grows clientWindow minimum width by the vertical status/action bars.
    void leftAndRightStatusActionBarWidthChanged(int newWidth) override;
    void setLowerPaneHeight(uint height) override;
    void setResizingMode(cip::tibia::qmlcomponents::enums::EGameWindowResizingMode mode) override;

    void setRememberEmail(bool v) override;
    /// CIP: the static race name of the boosted creature / boss, each word capitalised (the
    /// service's name when the race is not in the static data).
    QString boostedCreatureName() const override;
    /// The boosted boss as the client services last sent it.
    int boostedBossRace() const override;
    QString boostedBossName() const override;
    QObject* statusBarController() const override;
    /// CIP: OPEN_URL of a link in the hint area.
    void linkClicked(const QString& Url) override;
    /// CIP: the pointing hand over a hint-area link.
    void linkHoverChanged(bool IsOverLink) override;
    /// CIP: option showEMailAsPlainText.
    bool showEmailAsPlainText() const override;
    void setShowEmailAsPlainText(bool v) override;
    void loginPressed(const QString& EMailAddress, const QString& Password) override;
    void forgotEMailAddressOrPasswordPressed() override;
    void handleCreateNewAccount() override;
    void createNewAccountPressed() override;
    void goToLogin() override;
    void goToCreateNewAccount() override;
    void tibiaLogoPressed() override;
    void cipSoftLogoPressed() override;
    void twitchLogoPressed() override;
    void youTubeLogoPressed() override;
    /// Closes every open multi-action button (CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS).
    void closeOpenMultiActionButtons() override;
    /// Forwards the game window's close as gameWindowClosed.
    void onGameWindowClosed() override;
    /// Forward the chat input focus requests to the game window.
    void onRequestBindFocusToChatInput() override;
    void onRequestReleaseFocusFromChatInput() override;

    void quitPressed() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    /// Login-screen mute: toggles the anthem. Music volume 0 leaves the button checked and disabled.
    void muteAnthemPressed() override;
    /// CIP: open the account site.
    void manageAccountPressed() override;
    /// CIP: raise the launcher so the player can manage the installed client.
    void manageClientsPressed() override;
    /// CIP : OPEN_OPTIONS_MENU_DIALOG with no category.
    void optionsPressed() override;
    /// CIP gamewindow.qml calendar button → OPEN_CALENDAR_DIALOG.
    void requestOpenCalendar() override;

    /// CIP: a cached hint while the hints are under an hour old, else a new download.
    void updateHint() override;
    /// CIP: the hint to show now, from the downloaded hints.
    void selectRandomHintFromCache() override;
    void onHintDownloadSuccess(const QUrl& Url, const QByteArray& Data) override;
    /// CIP: a failed hints download shows the default hint.
    void onHintDownloadFailed(const QUrl& url);
    /// CIP: after the server save the preview moves on to the new day.
    void onEventsSchedulePreviewUpdateTimerTimeout() override;
    /// CIP slot (the client connects its events uploader's updated): today's
    /// events and the next starting ones, with their tooltips.
    void onEventScheduleUpdated(const std::vector<news::TEventInformation>& events);

    /// Graphics-engine restart and other intentional process exits. The window
    /// close that follows must not ask Exit / Logout / Cancel.
    static void allowProcessExit();
    static bool processExitAllowed();
    /// The start screen's create-account option (the client's first-start step shows it).
    void setShowCreateAccountOption(bool show);

Q_SIGNALS:
    /// The options dialog should open on `category` (OPEN_OPTIONS_MENU_DIALOG's).
    void optionsMenuRequested(int category);

private:
    void setScreen(const QString& state, bool backgroundImageVisible, const QColor& windowColor);
    void requestStartScreenServices();
    void requestDeferredStartScreenServices();
    void loadHints();
    void setHintAreaTitle(const QString& title);
    void setHintAreaRichText(const QString& richText);
    void onBoostedCreatureReceived();
    void onLiveInformationReceived();
    void onShowOffReceived();
    void clearEventModels();
    void syncAnthemButton();

    std::shared_ptr<game::TGameActionRouter> m_pGameActionRouter;
    std::shared_ptr<appearances::TAppearanceInstanceFactory> m_pAppearanceInstanceFactory;
    QQmlEngine* m_pEngine = nullptr;
    config::TClientOptions* m_pClientOptions = nullptr;
    QPointer<QQuickItem> m_container;
    /// The game window discards unhandled mouse buttons while it exists; the
    /// game screen turns the keyboard shortcuts and the movement keys on.
    std::shared_ptr<input::TEventFilterManager> m_pEventFilterManager;
    std::unique_ptr<TStatusBarController> m_pStatusBarController;
    QPointer<QQuickItem> m_gameWindowItem;
    /// The game window's RenderDriver (objectName renderDriver).
    QPointer<QQuickItem> m_renderDriver;
    QUrl m_hintsUrl;
    QVector<tibia::shared::Hint> m_hints;
    /// Seconds since the epoch of the last hints download.
    int m_hintsDownloadedAt = 0;
    QString m_cdnBase;
    std::vector<news::TEventInformation> m_events;
    QTimer m_eventsSchedulePreviewTimer;
    bool m_beforeServerSave = true;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;
    std::shared_ptr<shared::IClockService> m_pClockService;
    std::unique_ptr<game::TBoostedCreatureRequestUploader> m_boostedUploader;
    std::unique_ptr<game::TLiveInformationRequestUploader> m_liveInfoUploader;
    std::unique_ptr<game::TShowOffRequestUploader> m_showOffUploader;
};

}
}
