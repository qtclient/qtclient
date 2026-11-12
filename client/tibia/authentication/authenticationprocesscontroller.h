#pragma once

#include "authentication/cloudflarechallengetypes.h"
#include "authentication/loginrequestuploader.h"

#include <QObject>
#include <QPointer>
#include <QString>

namespace tibia {
namespace authentication {

/// CIP `ELoginConfirmationMethod` (moc arg on onRequestNewLoginConfirmationCode).
enum ELoginConfirmationMethod : int
{
    LoginConfirmationMethodUnknown = 0,
    LoginConfirmationMethodEmail = 1,
    LoginConfirmationMethodApp = 2,
};

/// Account-login state machine surface (CIP: TAuthenticationProcessController,
/// contract superclass QStateMachine — qtclient Qt build has no StateMachine module,
/// so the moc API lives on QObject). Live account login stays on
/// client::TGameClient + TLoginRequestUploader; bindLoginUploader() overlays
/// CIP slot names onto that uploader without replacing its GameClient sinks.
class TAuthenticationProcessController final : public QObject
{
    Q_OBJECT
public:
    explicit TAuthenticationProcessController(QObject* parent = nullptr);

    /// Optional overlay: CIP abort / 2FA / out-of-date slots onto an existing
    /// TLoginRequestUploader (the GameClient-owned one when wired later).
    void bindLoginUploader(TLoginRequestUploader* uploader);
    TLoginRequestUploader* loginUploader() const { return m_login.data(); }

    bool authenticated() const { return m_authenticated; }
    bool fpsTrackingActive() const { return m_fpsTrackingActive; }
    bool optionsTrackingActive() const { return m_optionsTrackingActive; }
    QString confirmationToken() const { return m_confirmationToken; }
    bool hasPendingConfirmationCode() const { return m_hasPendingConfirmationCode; }
    bool loginWaitingDialogVisible() const { return m_loginWaitingDialogVisible; }
    bool characterSelectionVisible() const { return m_characterSelectionVisible; }
    bool waitingForAssets() const { return m_waitingForAssets; }
    ELoginConfirmationMethod pendingConfirmationMethod() const { return m_pendingConfirmationMethod; }

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
    /// CIP Cloudflare challenge state-machine slots (live solve is on TGameClient).
    void onSolveCloudflareChallengeStateEntered();
    void onShowCloudflareChallengeFailedStateEntered();
    void onCloudflareSolverSuccess(tibia::authentication::TCloudflareChallengeClearanceInfo CloudflareClearanceInfo);
    /// CIP moc handleGameAction(shared_ptr<IGameAction>) — qtclient skips the
    /// shared_ptr arg (same pattern as friends search page).
    void handleGameAction();

Q_SIGNALS:
    void advanceStateMachine();
    void advanceStateMachineToLoginUpload();
    void advanceStateMachineDirectlyToCharacterSelection();
    void advanceStateMachineToWaitForAssetCompletion();
    void abort();
    void requestCharacterGameserverLogin();
    void publishGameAction();
    void authenticatedChanged(bool IsAuthenticated);
    void fpsTrackingActiveChanged(bool FpsTrackingActive);
    void optionsTrackingActiveChanged(bool OptionsTrackingActive);
    void quitAndPatchClient();

private:
    void setAuthenticated(bool value);
    void setFpsTrackingActive(bool value);
    void setOptionsTrackingActive(bool value);

    QPointer<TLoginRequestUploader> m_login;
    bool m_authenticated = false;
    bool m_fpsTrackingActive = false;
    bool m_optionsTrackingActive = false;
    bool m_loginWaitingDialogVisible = false;
    bool m_characterSelectionVisible = false;
    bool m_waitingForAssets = false;
    QString m_confirmationToken;
    bool m_hasPendingConfirmationCode = false;
    ELoginConfirmationMethod m_pendingConfirmationMethod = LoginConfirmationMethodUnknown;
};

}
}
