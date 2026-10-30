#pragma once

#include <cip_gen.h>

#include <QPointer>
#include <QString>
#include <QUrl>

#include <memory>

class QQuickItem;

namespace tibia::game {
class TBoostedCreatureRequestUploader;
class TLiveInformationRequestUploader;
class TShowOffRequestUploader;
}
namespace tibia::news {
class TEventsUploader;
class TNewsRequestUploader;
class TNewsStorage;
}

namespace tibia::gamewindow {

class TCalendarDialogController;
class TNewsButtonController;
class TNewsDialogController;

/// gamewindow.qml's controller (CIP: TGameWindowController, metacall
/// 0x100820100). Without launcher SSO / --account the start-screen login
/// mask is shown (`isAuthenticated` false); otherwise the launcher (or CLI)
/// authenticates and the character list opens. Start-screen HTTP services
/// (boosted creature, event schedule, live info / cacheinfo, news, showoff) are
/// requested via CIP uploaders against the portal clientservices multiplex.
class TGameWindowController final : public cip::tibia::gamewindow::TGameWindowController
{
    Q_OBJECT
public:
    /// `container` is clientwindow.qml's placeholder (gamewindowContainer).
    explicit TGameWindowController(QQuickItem* container, QObject* parent = nullptr);
    ~TGameWindowController() override;

    void setStatusBarController(QObject* controller);

    /// Button-bar Compendium / News button (CIP TNewsButtonController).
    TNewsButtonController* newsButton() const { return m_newsButton.get(); }

    /// Portal login/clientservices URL (`LauncherEnv::loginUrl` /
    /// `PortalUrls::clientServicesUrl`) and optional client revision header.
    void configureClientServices(const QUrl& url, const QString& clientRevision = {});

    /// CIP 0x100b65820: state "INGAME", no background art, black window.
    void showGameScreen();
    /// CIP 0x100b62b80: state "", the background art, window colour #041020.
    void showStartScreen();

    /// Hide the login mask once account login succeeded (or when SSO / --account starts).
    void setAuthenticated(bool authenticated);

    /// Persist `email` when remember-email is on (password login and SSO).
    void rememberLoginEmailIfEnabled(const QString& email);

    /// Options 0x3a, 0x3b and 0x3c (CIP 0x100b67390, 0x100b67420, 0x100b674b0).
    void setUpperPaneHeight(uint height) override;
    /// CIP: grows clientWindow minimum width by the vertical status/action bars.
    void leftAndRightStatusActionBarWidthChanged(int newWidth) override;
    void setLowerPaneHeight(uint height) override;
    void setResizingMode(cip::tibia::qmlcomponents::enums::EGameWindowResizingMode mode) override;

    void setRememberEmail(bool v) override;
    void loginPressed(const QString& EMailAddress, const QString& Password) override;
    void forgotEMailAddressOrPasswordPressed() override;
    void handleCreateNewAccount() override;
    void createNewAccountPressed() override;
    void goToLogin() override;
    void goToCreateNewAccount() override;
    void tibiaLogoPressed() override;
    void cipSoftLogoPressed() override;

    void quitPressed() override;
    /// Login-screen mute: toggles the anthem. Music volume 0 leaves the button checked and disabled.
    void muteAnthemPressed() override;
    /// CIP: open the account site.
    void manageAccountPressed() override;
    /// CIP: raise the launcher so the player can manage the installed client.
    void manageClientsPressed() override;
    /// CIP 0x100b66210: OPEN_OPTIONS_MENU_DIALOG with no category.
    void optionsPressed() override;
    /// CIP gamewindow.qml calendar button → OPEN_CALENDAR_DIALOG.
    void requestOpenCalendar() override;

    /// True while Exit was chosen — window close may proceed without re-prompt.
    bool allowWindowClose() const { return m_allowWindowClose; }
    void setAllowWindowClose(bool allow) { m_allowWindowClose = allow; }
    /// Graphics-engine restart and other intentional process exits. The window
    /// close that follows must not ask Exit / Logout / Cancel.
    static void allowProcessExit();
    static bool processExitAllowed();

Q_SIGNALS:
    /// The options dialog should open on `category` (OPEN_OPTIONS_MENU_DIALOG's).
    void optionsMenuRequested(int category);
    /// Start-screen Login button (email + password).
    void accountLoginRequested(const QString& email, const QString& password);
    /// Create Account on the CIP login mask (CreateAccountDialog.qml).
    void createAccountDialogRequested();
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
    void onBoostedCreatureReceived();
    void onLiveInformationReceived();
    void onShowOffReceived();
    void onEventScheduleUpdated();
    void clearEventModels();
    void showQuitGameDialog();
    void syncAnthemButton();

    QPointer<QQuickItem> m_container;
    bool m_allowWindowClose = false;
    class TMessageDialogController* m_quitDialog = nullptr;
    std::unique_ptr<news::TNewsStorage> m_newsStorage;
    std::unique_ptr<game::TBoostedCreatureRequestUploader> m_boostedUploader;
    std::unique_ptr<game::TLiveInformationRequestUploader> m_liveInfoUploader;
    std::unique_ptr<game::TShowOffRequestUploader> m_showOffUploader;
    std::unique_ptr<news::TEventsUploader> m_eventsUploader;
    std::unique_ptr<news::TNewsRequestUploader> m_newsUploader;
    std::unique_ptr<TNewsDialogController> m_newsDialog;
    std::unique_ptr<TNewsButtonController> m_newsButton;
    std::unique_ptr<TCalendarDialogController> m_calendarDialog;
};

}
