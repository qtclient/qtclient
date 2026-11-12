#include "authentication/authenticationprocesscontroller.h"

namespace tibia::authentication {

TAuthenticationProcessController::TAuthenticationProcessController(QObject* parent)
    : QObject(parent)
{
}

void TAuthenticationProcessController::bindLoginUploader(TLoginRequestUploader* uploader)
{
    if (m_login)
        disconnect(m_login, nullptr, this, nullptr);
    m_login = uploader;
    if (!uploader)
        return;

    // CIP name overrides onto the existing uploader signals — does not replace
    // TGameClient's own connections; only active when a host calls bind.
    connect(uploader, &TLoginRequestUploader::loginFailedClientOutOfDate, this,
            &TAuthenticationProcessController::onShowClientOutOfDateStateEntered);
    connect(uploader, &TLoginRequestUploader::loginFailedDueToMissingTwoFactorToken, this,
            &TAuthenticationProcessController::onShowTwoFactorTokenInputStateEntered);
    connect(uploader, &TLoginRequestUploader::loginFailedDueToCloudflareMitigation, this,
            &TAuthenticationProcessController::onSolveCloudflareChallengeStateEntered);
    connect(uploader, &TLoginRequestUploader::loginSuccessful, this, [this](const LoginResult&) {
        onShowCharacterSelectionStateEntered();
    });
}

void TAuthenticationProcessController::setAuthenticated(const bool value)
{
    if (m_authenticated == value)
        return;
    m_authenticated = value;
    Q_EMIT authenticatedChanged(m_authenticated);
}

void TAuthenticationProcessController::setFpsTrackingActive(const bool value)
{
    if (m_fpsTrackingActive == value)
        return;
    m_fpsTrackingActive = value;
    Q_EMIT fpsTrackingActiveChanged(m_fpsTrackingActive);
}

void TAuthenticationProcessController::setOptionsTrackingActive(const bool value)
{
    if (m_optionsTrackingActive == value)
        return;
    m_optionsTrackingActive = value;
    Q_EMIT optionsTrackingActiveChanged(m_optionsTrackingActive);
}

void TAuthenticationProcessController::onLoginStateMachineStartedShortcutToCharacterSelection()
{
    Q_EMIT advanceStateMachineDirectlyToCharacterSelection();
    onShowCharacterSelectionStateEntered();
}

void TAuthenticationProcessController::onLoginStateMachineStarted()
{
    Q_EMIT advanceStateMachineToLoginUpload();
    Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onShowLoginWaitingDialogStateEntered()
{
    m_loginWaitingDialogVisible = true;
}

void TAuthenticationProcessController::onShowLoginWaitingDialogStateLeft()
{
    m_loginWaitingDialogVisible = false;
}

void TAuthenticationProcessController::onLoginFailedStateEntered()
{
    setAuthenticated(false);
}

void TAuthenticationProcessController::onLoginFailedStateLeft()
{
}

void TAuthenticationProcessController::onShowTwoFactorTokenInputStateEntered()
{
}

void TAuthenticationProcessController::onShowTwoFactorTokenInputStateLeft()
{
}

void TAuthenticationProcessController::onShowDeviceVerificationCodeInputStateEntered()
{
}

void TAuthenticationProcessController::onShowDeviceVerificationCodeInputStateLeft()
{
}

void TAuthenticationProcessController::onShowTwoFactorEMailInputStateEntered()
{
}

void TAuthenticationProcessController::onShowTwoFactorEMailInputStateLeft()
{
}

void TAuthenticationProcessController::onShowLoginConfirmationInputStateEntered()
{
}

void TAuthenticationProcessController::onShowLoginConfirmationInputStateLeft()
{
}

void TAuthenticationProcessController::onShowCharacterSelectionStateEntered()
{
    m_characterSelectionVisible = true;
    setAuthenticated(true);
}

void TAuthenticationProcessController::onShowCharacterSelectionStateLeft()
{
    m_characterSelectionVisible = false;
}

void TAuthenticationProcessController::onStartGameServerLoginStateEntered()
{
    Q_EMIT requestCharacterGameserverLogin();
}

void TAuthenticationProcessController::onWaitForAssetsCompletionEntered()
{
    m_waitingForAssets = true;
    Q_EMIT advanceStateMachineToWaitForAssetCompletion();
}

void TAuthenticationProcessController::onWaitForAssetsCompletionLeft()
{
    m_waitingForAssets = false;
}

void TAuthenticationProcessController::onLoginFinishedSuccessfullyEntered()
{
    setAuthenticated(true);
    setFpsTrackingActive(true);
    setOptionsTrackingActive(true);
}

void TAuthenticationProcessController::onLoginAbortedStateEntered()
{
    // CIP Abort on the connecting dialog → TLoginRequestUploader::abort.
    if (m_login)
        m_login->abort();
    setAuthenticated(false);
    Q_EMIT abort();
}

void TAuthenticationProcessController::onShowReturnerRewardStateEntered()
{
}

void TAuthenticationProcessController::onShowEmergencyDialogEntered()
{
}

void TAuthenticationProcessController::onShowCharacterTradeDuePaymentEntered()
{
}

void TAuthenticationProcessController::onShowSurveyInvitationStateEntered()
{
}

void TAuthenticationProcessController::onShowClientOutOfDateStateEntered()
{
    // CIP out-of-date path ends in patch/quit; qtclient GameClient still fails toast.
    Q_EMIT quitAndPatchClient();
}

void TAuthenticationProcessController::onBeginPatchClientStateEntered()
{
    Q_EMIT quitAndPatchClient();
}

void TAuthenticationProcessController::onRequestNewEMailCode()
{
    // CIP → TEmailCodeRequestUploader; qtclient has no live email-code path yet.
}

void TAuthenticationProcessController::onRequestNewLoginConfirmationCode(const ELoginConfirmationMethod Method)
{
    m_pendingConfirmationMethod = Method;
}

void TAuthenticationProcessController::onTooManyEMailCodeRequests()
{
}

void TAuthenticationProcessController::onAlreadyRequestedEmailCodeRecently()
{
}

void TAuthenticationProcessController::onEMailCodeRequestCompleted(const bool Success)
{
    if (Success)
        Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onTooManyLoginConfirmationCodeRequests()
{
}

void TAuthenticationProcessController::onAlreadyRequestedLoginConfirmationCodeRecently()
{
}

void TAuthenticationProcessController::onInvalidLoginConfirmationToken()
{
    m_confirmationToken.clear();
    m_hasPendingConfirmationCode = false;
}

void TAuthenticationProcessController::onLoginConfirmationCodeRequestSuccessful(const bool Value)
{
    if (Value)
        Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onLoginFailedDueToMissingLoginConfirmation(const QString ConfirmationToken,
                                                                                 const bool HasPendingCode)
{
    m_confirmationToken = ConfirmationToken;
    m_hasPendingConfirmationCode = HasPendingCode;
    onShowLoginConfirmationInputStateEntered();
}

void TAuthenticationProcessController::onPlayerLevelUp()
{
}

void TAuthenticationProcessController::onPlayerDataChanged()
{
}

void TAuthenticationProcessController::onSolveCloudflareChallengeStateEntered()
{
    // CIP advances into the CloudflareChallengeDialog state; live qtclient solves
    // via TGameClient + TCloudflareChallengeController.
    Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onShowCloudflareChallengeFailedStateEntered()
{
    setAuthenticated(false);
}

void TAuthenticationProcessController::onCloudflareSolverSuccess(const TCloudflareChallengeClearanceInfo CloudflareClearanceInfo)
{
    if (m_login)
        m_login->applyClearance(CloudflareClearanceInfo);
    Q_EMIT advanceStateMachineToLoginUpload();
}

void TAuthenticationProcessController::handleGameAction()
{
    Q_EMIT publishGameAction();
}

} // namespace tibia::authentication
