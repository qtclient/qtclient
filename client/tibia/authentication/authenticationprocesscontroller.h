#pragma once

#include "authentication/cloudflarechallengetypes.h"
#include "input/gameactionptr.h"

#include <QObject>
#include <QString>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace gamewindow {
class TCharacterSelectionController;
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
/// uploader, the two code request uploaders and the Cloudflare challenge.
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

    /// What the host provides: the login request's abort and Cloudflare
    /// clearance, and the post-login dialogs (each shows its dialog and answers
    /// whether it did; unset means the dialog is not shown).
    struct THost
    {
        std::function<void()> abortLogin;
        std::function<void(const TCloudflareChallengeClearanceInfo&)> applyClearance;
        std::function<bool()> showReturnerRewardDialog;
        std::function<bool()> showEmergencyDialog;
        std::function<bool()> showCharacterTradeDuePaymentDialog;
        std::function<bool()> showSurveyInvitationDialog;
        /// Whether the assets are loaded, so the game server login can start.
        std::function<bool()> assetsComplete;
        /// Forgets the account's session (the host keeps the play session data).
        std::function<void()> resetSessionData;
    };

    explicit TAuthenticationProcessController(
        std::shared_ptr<gamewindow::TCharacterSelectionController> CharacterSelectionController = {},
        std::shared_ptr<TLoginRequestUploader> LoginRequestUploader = {},
        std::shared_ptr<TEmailCodeRequestUploader> EMailCodeRequestUploader = {},
        std::shared_ptr<TLoginConfirmationCodeRequestUploader> LoginConfirmationCodeRequestUploader = {},
        std::shared_ptr<TCloudflareChallengeController> CloudflareChallengeController = {}, QObject* parent = nullptr);
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
    /// The account the login requests and the code requests are made for.
    void setLoginCredentials(const QString& EMail, const QString& Password);

    void setHost(THost host) { m_host = std::move(host); }

    /// Connects a login uploader's results (anything with
    /// TLoginRequestUploader's signals) to the machine.
    template<typename Uploader>
    void bindLoginUploader(Uploader* uploader);

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
    /// the credentials again.
    void resetSessionData();

    bool authenticated() const { return m_authenticated; }
    bool fpsTrackingActive() const { return m_fpsTrackingActive; }
    bool optionsTrackingActive() const { return m_optionsTrackingActive; }
    QString confirmationToken() const { return m_confirmationToken; }
    bool hasPendingConfirmationCode() const { return m_hasPendingConfirmationCode; }
    bool loginWaitingDialogVisible() const { return m_loginWaitingDialogVisible; }
    bool characterSelectionVisible() const { return m_characterSelectionVisible; }
    bool waitingForAssets() const { return m_waitingForAssets; }

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

    std::shared_ptr<gamewindow::TCharacterSelectionController> m_pCharacterSelectionController;
    std::shared_ptr<TLoginRequestUploader> m_pLoginRequestUploader;
    std::shared_ptr<TEmailCodeRequestUploader> m_pEMailCodeRequestUploader;
    std::shared_ptr<TLoginConfirmationCodeRequestUploader> m_pLoginConfirmationCodeRequestUploader;
    /// The login data's account.
    QString m_eMail;
    QString m_password;
    /// What the character selection handed on for the game server login.
    std::vector<gamewindow::TCharacterLoginData> m_characterLoginData;
    std::shared_ptr<TCloudflareChallengeController> m_pCloudflareChallengeController;

    THost m_host;
    std::vector<TTransition> m_transitions;
    std::vector<std::pair<EState, void (TAuthenticationProcessController::*)()>> m_entered;
    std::vector<std::pair<EState, void (TAuthenticationProcessController::*)()>> m_exited;
    EState m_state = EState::Start;
    std::optional<EState> m_child;
    bool m_running = false;
    bool m_processing = false;
    std::deque<ETrigger> m_queue;
    std::optional<TCloudflareChallengeClearanceInfo> m_cloudflareClearance;

    bool m_authenticated = false;
    bool m_fpsTrackingActive = false;
    bool m_optionsTrackingActive = false;
    bool m_loginWaitingDialogVisible = false;
    bool m_characterSelectionVisible = false;
    bool m_waitingForAssets = false;
    QString m_confirmationToken;
    bool m_hasPendingConfirmationCode = false;
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
