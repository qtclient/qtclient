#pragma once

#include "authentication/authenticationjsondocumenthelper.h"
#include "authentication/cloudflarechallengetypes.h"
#include "input/gameactionptr.h"

#include "qtconnectionmanager.h"

#include <QJsonDocument>
#include <QObject>
#include <QString>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace game {
class TGameSessionBase;
}
namespace assets {
class TAssetManager;
}
namespace gamewindow {
class TCharacterSelectionController;
class TCharacterTradeDuePaymentDialogController;
class TEmergencyDialogController;
class TGenericEnterTextDialogController;
class TLoginConfirmationCodeEnterTextDialogController;
class TMessageDialogController;
class TReturnerRewardDialogController;
class TSurveyInvitationDialogController;
class TTwoFactorCodeEnterTextDialogController;
struct TCharacterLoginData;
}
namespace authentication {

class TCloudflareChallengeController;
class TEmailCodeRequestUploader;
class TLoginConfirmationCodeRequestUploader;
class TLoginRequestUploader;

/// CIP `ELoginConfirmationMethod` (moc arg on onRequestNewLoginConfirmationCode):
/// where a login confirmation code is sent. The login service names them
/// emailPrimary, emailSecondary and sms.
enum ELoginConfirmationMethod : int
{
    LoginConfirmationMethodEmailPrimary = 0,
    LoginConfirmationMethodEmailSecondary = 1,
    LoginConfirmationMethodSms = 2,
};

/// The account login as a state machine (CIP: TAuthenticationProcessController,
/// a QStateMachine). The Qt build has no StateMachine module, so the states and
/// transitions are a table here with QStateMachine's semantics: a transition
/// leaves the active state (its exited slot), then enters the target (its
/// entered slot); triggers raised while a transition runs are queued and
/// handled after it; a final top-level state stops the machine.
///
/// It owns the login's parts: the character selection, the login request
/// uploader, the two code request uploaders, the Cloudflare challenge, the
/// login's dialogs (the two-factor code, the login confirmation code, the
/// device verification code, the message box, the survey invitation, the
/// returner reward, the important balancing message and the character trade
/// due payments) and the login data the dialogs fill in.
class TAuthenticationProcessController final : public QObject
{
    Q_OBJECT
public:
    enum class EState
    {
        Start,
        ShowLoginWaitingDialog,
        /// Compound: the dialogs after a successful login, one after another.
        PostLoginDialogs,
        ShowReturnerReward,
        ShowEmergencyDialog,
        ShowCharacterTradeDuePayment,
        ShowSurveyInvitation,
        PostLoginDialogsFinished,
        LoginFailed,
        ShowTwoFactorTokenInput,
        ShowDeviceVerificationCodeInput,
        ShowTwoFactorEMailInput,
        ShowLoginConfirmationInput,
        SolveCloudflareChallenge,
        ShowCloudflareChallengeFailed,
        ShowCharacterSelection,
        StartGameServerLogin,
        WaitForAssetsCompletion,
        ShowClientOutOfDate,
        BeginPatchClient,
        LoginFinishedSuccessfully,
        LoginAborted,
    };

    /// The signals that move the machine: its own advance signals, the login
    /// uploaders', the dialogs' and the asset manager's.
    enum class ETrigger
    {
        AdvanceStateMachine,
        AdvanceToLoginUpload,
        AdvanceDirectlyToCharacterSelection,
        AdvanceToWaitForAssetCompletion,
        LoginSuccessful,
        LoginFailed,
        LoginFailedDueToInvalidServerResponse,
        LoginFailedDueToMissingTwoFactorToken,
        LoginFailedDueToInvalidDeviceCookie,
        LoginFailedClientOutOfDate,
        LoginFailedDueToMissingEMailLoginCode,
        LoginFailedDueToMissingLoginConfirmation,
        LoginFailedDueToCloudflareMitigation,
        InvalidLoginConfirmationToken,
        CloudflareSolveFailed,
        MessageDialogAccepted,
        MessageDialogDiscarded,
        ReturnerRewardDialogClosed,
        EmergencyDialogClosed,
        CharacterTradeDuePaymentDialogClosed,
        SurveyInvitationDialogClosed,
        TwoFactorCodeEntered,
        DeviceVerificationCodeEntered,
        LoginConfirmationCodeEntered,
        DialogAborted,
        CharacterLoginRequested,
        AssetsComplete,
    };

    /// What the host provides.
    struct THost
    {
        /// The host's copy of the play session data is forgotten too.
        std::function<void()> resetSessionData;
    };

    explicit TAuthenticationProcessController(
        std::shared_ptr<gamewindow::TCharacterSelectionController> CharacterSelectionController = {},
        std::shared_ptr<TLoginRequestUploader> LoginRequestUploader = {},
        std::shared_ptr<TEmailCodeRequestUploader> EMailCodeRequestUploader = {},
        std::shared_ptr<TLoginConfirmationCodeRequestUploader> LoginConfirmationCodeRequestUploader = {},
        std::shared_ptr<TCloudflareChallengeController> CloudflareChallengeController = {},
        std::shared_ptr<assets::TAssetManager> AssetManager = {}, QObject* parent = nullptr);
    ~TAuthenticationProcessController() override;

    gamewindow::TCharacterSelectionController* characterSelectionController() const
    {
        return m_pCharacterSelectionController.get();
    }
    TLoginRequestUploader* loginRequestUploader() const { return m_pLoginRequestUploader.get(); }
    TCloudflareChallengeController* cloudflareChallengeController() const
    {
        return m_pCloudflareChallengeController.get();
    }
    /// The login data the next login request sends (the login mask's account,
    /// stay-logged-in and client fields); the dialogs add their codes to it.
    const TLoginRequestData& loginData() const { return m_loginData; }
    /// CIP: the character the selection handed on for the game server login
    /// (one entry, or none).
    std::vector<gamewindow::TCharacterLoginData> characterLoginData() const;
    /// CIP: the first of characterLoginData, or an empty one.
    gamewindow::TCharacterLoginData firstCharacterLoginData() const;

    /// CIP: a login with `EMail` and `Password` from the login mask: the
    /// login data is renewed (stay logged in, the device cookie and the
    /// trusted device token for the account; the codes are cleared), the
    /// email address is remembered when the client options say so, and the
    /// machine starts (or, when it runs, goes to the login upload).
    void loginWithCredentials(const QString& EMail, const QString& Password);
    /// CIP: the game session whose player's level and data the news and the
    /// level popups follow; the login request uploader's character list
    /// follows it too.
    void connectToGameSession(const std::shared_ptr<game::TGameSessionBase>& GameSession);

    void setHost(THost host) { m_host = std::move(host); }

    /// Connects a login uploader's results (anything with
    /// TLoginRequestUploader's signals) to the machine.
    template<typename Uploader>
    void bindLoginUploader(Uploader* uploader);

    /// CIP: the account's play data from a reply (the character creation's): the
    /// login request uploader and the character selection take its characters and
    /// worlds; a single character still in the tutorial is selected (logs in).
    void setPlayDataFromReply(const QJsonDocument& Reply);

    /// Starts the machine (again, when it runs); with `shortcutToCharacterSelection`
    /// it goes straight to the character selection (a running session's character switch).
    void start(bool shortcutToCharacterSelection = false);
    bool isRunning() const { return m_running; }
    EState state() const { return m_state; }
    /// The active child of PostLoginDialogs, when that state is active.
    std::optional<EState> postLoginDialogState() const { return m_child; }
    /// Raises a trigger; ignored unless the active state has a transition for it.
    void trigger(ETrigger trigger);
    /// CIP: back to the character selection while authenticated: a running
    /// machine advances straight to it, a stopped one starts there.
    void showCharacterSelection();
    /// CIP: forgets the account's session data, so the next login asks for
    /// the credentials again: the credentials, the play session, the login
    /// confirmation token, an expiring Cloudflare clearance, the character
    /// selection's characters, the authentication and the premium time.
    void resetSessionData();

    /// Stops the machine and closes the character selection, the two-factor
    /// code dialog, the message box and the login confirmation code dialog.
    void stopProcess();

    bool authenticated() const { return m_authenticated; }
    bool fpsTrackingActive() const { return m_fpsTrackingActive; }
    bool optionsTrackingActive() const { return m_optionsTrackingActive; }

public Q_SLOTS:
    void onLoginStateMachineStartedShortcutToCharacterSelection();
    void onLoginStateMachineStarted();
    void onShowLoginWaitingDialogStateEntered();
    void onShowLoginWaitingDialogStateLeft();
    void onLoginFailedStateEntered();
    void onLoginFailedStateLeft();
    void onShowTwoFactorTokenInputStateEntered();
    void onShowTwoFactorTokenInputStateLeft();
    void onShowDeviceVerificationCodeInputStateEntered();
    void onShowDeviceVerificationCodeInputStateLeft();
    void onShowTwoFactorEMailInputStateEntered();
    void onShowTwoFactorEMailInputStateLeft();
    void onShowLoginConfirmationInputStateEntered();
    void onShowLoginConfirmationInputStateLeft();
    void onShowCharacterSelectionStateEntered();
    void onShowCharacterSelectionStateLeft();
    void onStartGameServerLoginStateEntered();
    void onWaitForAssetsCompletionEntered();
    void onWaitForAssetsCompletionLeft();
    void onLoginFinishedSuccessfullyEntered();
    void onLoginAbortedStateEntered();
    void onShowReturnerRewardStateEntered();
    void onShowEmergencyDialogEntered();
    void onShowCharacterTradeDuePaymentEntered();
    void onShowSurveyInvitationStateEntered();
    void onShowClientOutOfDateStateEntered();
    void onBeginPatchClientStateEntered();
    void onRequestNewEMailCode();
    void onRequestNewLoginConfirmationCode(tibia::authentication::ELoginConfirmationMethod Method);
    void onTooManyEMailCodeRequests();
    void onAlreadyRequestedEmailCodeRecently();
    void onEMailCodeRequestCompleted(bool Success);
    void onTooManyLoginConfirmationCodeRequests();
    void onAlreadyRequestedLoginConfirmationCodeRecently();
    void onInvalidLoginConfirmationToken();
    void onLoginConfirmationCodeRequestSuccessful(bool Value);
    void onLoginFailedDueToMissingLoginConfirmation(QString ConfirmationToken, bool HasPendingCode);
    void onPlayerLevelUp();
    void onPlayerDataChanged();
    void onSolveCloudflareChallengeStateEntered();
    void onShowCloudflareChallengeFailedStateEntered();
    void onCloudflareSolverSuccess(tibia::authentication::TCloudflareChallengeClearanceInfo CloudflareClearanceInfo);
    /// A character finished the tutorial: the character selection and the login
    /// answer forget its tutorial flag, and the pending login data is read again.
    void handleGameAction(const tibia::input::IGameActionPtr& pGameAction);

Q_SIGNALS:
    void advanceStateMachine();
    void advanceStateMachineToLoginUpload();
    void advanceStateMachineDirectlyToCharacterSelection();
    void advanceStateMachineToWaitForAssetCompletion();
    void abort();
    void requestCharacterGameserverLogin();
    void publishGameAction(const tibia::input::IGameActionPtr& pGameAction);
    void authenticatedChanged(bool IsAuthenticated);
    void fpsTrackingActiveChanged(bool FpsTrackingActive);
    void optionsTrackingActiveChanged(bool OptionsTrackingActive);
    void quitAndPatchClient();
    /// The machine entered `state` (after its entered slot ran).
    void stateEntered(tibia::authentication::TAuthenticationProcessController::EState state);
    /// The machine reached a final top-level state and stopped.
    void finished();

private:
    struct TTransition
    {
        EState from;
        ETrigger trigger;
        EState to;
    };

    /// CIP's states, their entered/exited slots and transitions.
    void buildStateMachine();
    void process(ETrigger trigger);
    void enter(EState state);
    void exit(EState state);
    void stop();
    static bool isPostLoginDialog(EState state);

    void setAuthenticated(bool value);
    void setFpsTrackingActive(bool value);
    void setOptionsTrackingActive(bool value);
    /// CIP: the account's trusted device token: a SHA-1 of the lower-cased
    /// email address, the machine id, the user name and the CPU.
    static QString trustedDeviceToken(const QString& EMail);

    std::shared_ptr<gamewindow::TCharacterSelectionController> m_pCharacterSelectionController;
    std::shared_ptr<TLoginRequestUploader> m_pLoginRequestUploader;
    std::shared_ptr<TEmailCodeRequestUploader> m_pEMailCodeRequestUploader;
    std::shared_ptr<TLoginConfirmationCodeRequestUploader> m_pLoginConfirmationCodeRequestUploader;
    std::unique_ptr<gamewindow::TTwoFactorCodeEnterTextDialogController> m_pTwoFactorCodeEnterTextDialogController;
    std::unique_ptr<gamewindow::TLoginConfirmationCodeEnterTextDialogController>
        m_pLoginConfirmationCodeEnterTextDialogController;
    /// The device verification code ("request_device_verification_code_*", ten characters).
    std::unique_ptr<gamewindow::TGenericEnterTextDialogController> m_pDeviceVerificationCodeEnterTextDialogController;
    std::unique_ptr<gamewindow::TMessageDialogController> m_pMessageDialogController;
    std::unique_ptr<gamewindow::TSurveyInvitationDialogController> m_pSurveyInvitationDialogController;
    std::unique_ptr<gamewindow::TReturnerRewardDialogController> m_pReturnerRewardDialogController;
    std::unique_ptr<gamewindow::TEmergencyDialogController> m_pEmergencyDialogController;
    std::unique_ptr<gamewindow::TCharacterTradeDuePaymentDialogController> m_pCharacterTradeDuePaymentDialogController;
    /// What the next login request sends.
    TLoginRequestData m_loginData;
    /// What the character selection handed on for the game server login.
    std::vector<gamewindow::TCharacterLoginData> m_characterLoginData;
    std::shared_ptr<game::TGameSessionBase> m_pGameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<TCloudflareChallengeController> m_pCloudflareChallengeController;
    /// The client's asset loading; the game server login waits for it. Host:
    /// none while qtclient loads its assets before the login.
    std::shared_ptr<assets::TAssetManager> m_pAssetManager;

    THost m_host;
    std::vector<TTransition> m_transitions;
    std::vector<std::pair<EState, void (TAuthenticationProcessController::*)()>> m_entered;
    std::vector<std::pair<EState, void (TAuthenticationProcessController::*)()>> m_exited;
    EState m_state = EState::Start;
    std::optional<EState> m_child;
    bool m_running = false;
    bool m_processing = false;
    std::deque<ETrigger> m_queue;

    bool m_authenticated = false;
    bool m_fpsTrackingActive = false;
    bool m_optionsTrackingActive = false;
};

template<typename Uploader>
void TAuthenticationProcessController::bindLoginUploader(Uploader* uploader)
{
    using T = ETrigger;
    const auto on = [this, uploader](auto signal, const T trigger) {
        connect(uploader, signal, this, [this, trigger] { this->trigger(trigger); });
    };
    on(&Uploader::loginSuccessful, T::LoginSuccessful);
    on(&Uploader::loginFailed, T::LoginFailed);
    on(&Uploader::loginFailedDueToInvalidServerResponse, T::LoginFailedDueToInvalidServerResponse);
    on(&Uploader::loginFailedDueToMissingTwoFactorToken, T::LoginFailedDueToMissingTwoFactorToken);
    on(&Uploader::loginFailedDueToInvalidDeviceCookie, T::LoginFailedDueToInvalidDeviceCookie);
    on(&Uploader::loginFailedClientOutOfDate, T::LoginFailedClientOutOfDate);
    on(&Uploader::loginFailedDueToMissingEMailLoginCode, T::LoginFailedDueToMissingEMailLoginCode);
    on(&Uploader::loginFailedDueToCloudflareMitigation, T::LoginFailedDueToCloudflareMitigation);
    connect(uploader, &Uploader::loginFailedDueToMissingLoginConfirmation, this,
            &TAuthenticationProcessController::onLoginFailedDueToMissingLoginConfirmation);
    on(&Uploader::loginFailedDueToMissingLoginConfirmation, T::LoginFailedDueToMissingLoginConfirmation);
}

}
}
