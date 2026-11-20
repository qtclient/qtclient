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
namespace game {
class TBoostedCreatureRequestUploader;
class TLiveInformationRequestUploader;
class TShowOffRequestUploader;
}
}
namespace tibia {
namespace news {
class TEventsUploader;
class TNewsDiskIO;
class TNewsRequestUploader;
class TNewsStorage;
class TNewsViewsUploader;
}
namespace shared {
class TTibiaFileSystemHelper;
}
}

namespace tibia {
namespace gamewindow {

class TCalendarDialogController;
class TNewsButtonController;
class TNewsDialogController;

/// gamewindow.qml's controller (CIP: TGameWindowController, metacall
/// ). Without launcher SSO / --account the start-screen login
/// mask is shown (`isAuthenticated` false); otherwise the launcher (or CLI)
/// authenticates and the character list opens. Start-screen HTTP services
/// (boosted creature, event schedule, live info / cacheinfo, news, showoff) are
/// requested via CIP uploaders against /clientservices/clientservices.php.
class TGameWindowController final : public cip::tibia::gamewindow::TGameWindowController
{
    Q_OBJECT
public:
    /// `container` is clientwindow.qml's placeholder (gamewindowContainer).
    explicit TGameWindowController(QQuickItem* container, QObject* parent = nullptr);
    ~TGameWindowController() override;

    void setStatusBarController(QObject* controller);

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

    /// Button-bar Compendium / News button (CIP TNewsButtonController).
    TNewsButtonController* newsButton() const { return m_newsButton.get(); }

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
    QString boostedBossName() const override;
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

    /// True while Exit was chosen — window close may proceed without re-prompt.
    bool allowWindowClose() const { return m_allowWindowClose; }
    void setAllowWindowClose(bool allow) { m_allowWindowClose = allow; }
    /// Graphics-engine restart and other intentional process exits. The window
    /// close that follows must not ask Exit / Logout / Cancel.
    /// The gameExitRequested receiver: Exit / Logout / Cancel while a character
    /// is in the world, otherwise the client exits (quitGameExitRequested).
    void handleGameExitRequest();

    static void allowProcessExit();
    static bool processExitAllowed();

Q_SIGNALS:
    /// The options dialog should open on `category` (OPEN_OPTIONS_MENU_DIALOG's).
    void optionsMenuRequested(int category);
    /// Quit-game dialog: Exit (close client).
    void quitGameExitRequested();
    /// Quit-game dialog: Logout (leave character / return to login).
    void quitGameLogoutRequested();

private:
    void setScreen(const QString& state, bool backgroundImageVisible, const QColor& windowColor);
    void setShowCreateAccountOption(bool show);
    void requestStartScreenServices();
    void requestDeferredStartScreenServices();
    void loadHints();
    void setHintAreaTitle(const QString& title);
    void setHintAreaRichText(const QString& richText);
    void onBoostedCreatureReceived();
    void onLiveInformationReceived();
    void onShowOffReceived();
    void clearEventModels();
    /// Today's events and the next starting ones, with their tooltips.
    void onEventScheduleUpdated(const std::vector<news::TEventInformation>& events);
    void syncAnthemButton();

    QPointer<QQuickItem> m_container;
    QPointer<QQuickItem> m_gameWindowItem;
    QUrl m_hintsUrl;
    QVector<tibia::shared::Hint> m_hints;
    /// Seconds since the epoch of the last hints download.
    int m_hintsDownloadedAt = 0;
    QString m_cdnBase;
    std::vector<news::TEventInformation> m_events;
    QTimer m_eventsSchedulePreviewTimer;
    bool m_beforeServerSave = true;
    bool m_allowWindowClose = false;
    class TMessageDialogController* m_quitDialog = nullptr;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;
    std::shared_ptr<news::TNewsStorage> m_newsStorage;
    std::shared_ptr<news::TNewsDiskIO> m_newsDiskIO;
    std::unique_ptr<news::TEventsStorage> m_eventsStorage;
    std::unique_ptr<game::TBoostedCreatureRequestUploader> m_boostedUploader;
    std::unique_ptr<game::TLiveInformationRequestUploader> m_liveInfoUploader;
    std::unique_ptr<game::TShowOffRequestUploader> m_showOffUploader;
    std::unique_ptr<news::TEventsUploader> m_eventsUploader;
    std::unique_ptr<news::TNewsRequestUploader> m_newsUploader;
    std::unique_ptr<news::TNewsViewsUploader> m_newsViewsUploader;
    std::unique_ptr<TNewsDialogController> m_newsDialog;
    std::unique_ptr<TNewsButtonController> m_newsButton;
    std::unique_ptr<TCalendarDialogController> m_calendarDialog;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
