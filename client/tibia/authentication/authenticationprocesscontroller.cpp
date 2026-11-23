#include "authentication/authenticationprocesscontroller.h"
#include "authentication/emailcoderequestuploader.h"
#include "authentication/loginconfirmationcoderequestuploader.h"
#include "authentication/loginrequestuploader.h"
#include "controllers/characterselectioncontroller.h"
#include "input/gameaction.h"

#include <algorithm>

namespace tibia::authentication {

using EState = TAuthenticationProcessController::EState;
using ETrigger = TAuthenticationProcessController::ETrigger;

TAuthenticationProcessController::TAuthenticationProcessController(
    std::shared_ptr<gamewindow::TCharacterSelectionController> CharacterSelectionController,
    std::shared_ptr<TLoginRequestUploader> LoginRequestUploader,
    std::shared_ptr<TEmailCodeRequestUploader> EMailCodeRequestUploader,
    std::shared_ptr<TLoginConfirmationCodeRequestUploader> LoginConfirmationCodeRequestUploader,
    std::shared_ptr<TCloudflareChallengeController> CloudflareChallengeController, QObject* parent)
    : QObject(parent)
    , m_pCharacterSelectionController(std::move(CharacterSelectionController))
    , m_pLoginRequestUploader(std::move(LoginRequestUploader))
    , m_pEMailCodeRequestUploader(std::move(EMailCodeRequestUploader))
    , m_pLoginConfirmationCodeRequestUploader(std::move(LoginConfirmationCodeRequestUploader))
    , m_pCloudflareChallengeController(std::move(CloudflareChallengeController))
{
    buildStateMachine();
}

TAuthenticationProcessController::~TAuthenticationProcessController() = default;

void TAuthenticationProcessController::buildStateMachine()
{
    using S = EState;
    using T = ETrigger;
    m_transitions = {
        { S::Start, T::AdvanceDirectlyToCharacterSelection, S::ShowCharacterSelection },
        { S::Start, T::AdvanceToLoginUpload, S::ShowLoginWaitingDialog },

        { S::ShowLoginWaitingDialog, T::LoginSuccessful, S::PostLoginDialogs },
        { S::ShowLoginWaitingDialog, T::LoginFailed, S::LoginFailed },
        { S::ShowLoginWaitingDialog, T::LoginFailedDueToInvalidServerResponse, S::LoginFailed },
        { S::ShowLoginWaitingDialog, T::LoginFailedDueToMissingTwoFactorToken, S::ShowTwoFactorTokenInput },
        { S::ShowLoginWaitingDialog, T::LoginFailedDueToInvalidDeviceCookie, S::ShowDeviceVerificationCodeInput },
        { S::ShowLoginWaitingDialog, T::LoginFailedClientOutOfDate, S::ShowClientOutOfDate },
        { S::ShowLoginWaitingDialog, T::LoginFailedDueToMissingEMailLoginCode, S::ShowTwoFactorEMailInput },
        { S::ShowLoginWaitingDialog, T::LoginFailedDueToMissingLoginConfirmation, S::ShowLoginConfirmationInput },
        { S::ShowLoginWaitingDialog, T::InvalidLoginConfirmationToken, S::LoginFailed },
        { S::ShowLoginWaitingDialog, T::LoginFailedDueToCloudflareMitigation, S::SolveCloudflareChallenge },
        { S::ShowLoginWaitingDialog, T::MessageDialogDiscarded, S::LoginAborted },

        { S::SolveCloudflareChallenge, T::AdvanceToLoginUpload, S::ShowLoginWaitingDialog },
        { S::SolveCloudflareChallenge, T::CloudflareSolveFailed, S::ShowCloudflareChallengeFailed },
        { S::ShowCloudflareChallengeFailed, T::MessageDialogAccepted, S::LoginAborted },

        // The post-login dialogs: each one closes or advances to the next.
        { S::ShowReturnerReward, T::ReturnerRewardDialogClosed, S::ShowEmergencyDialog },
        { S::ShowReturnerReward, T::AdvanceStateMachine, S::ShowEmergencyDialog },
        { S::ShowEmergencyDialog, T::EmergencyDialogClosed, S::ShowCharacterTradeDuePayment },
        { S::ShowEmergencyDialog, T::AdvanceStateMachine, S::ShowCharacterTradeDuePayment },
        { S::ShowCharacterTradeDuePayment, T::CharacterTradeDuePaymentDialogClosed, S::ShowSurveyInvitation },
        { S::ShowCharacterTradeDuePayment, T::AdvanceStateMachine, S::ShowSurveyInvitation },
        { S::ShowSurveyInvitation, T::SurveyInvitationDialogClosed, S::PostLoginDialogsFinished },
        { S::ShowSurveyInvitation, T::AdvanceStateMachine, S::PostLoginDialogsFinished },

        { S::LoginFailed, T::MessageDialogAccepted, S::LoginAborted },
        { S::ShowTwoFactorTokenInput, T::TwoFactorCodeEntered, S::ShowLoginWaitingDialog },
        { S::ShowTwoFactorTokenInput, T::DialogAborted, S::LoginAborted },
        { S::ShowDeviceVerificationCodeInput, T::DeviceVerificationCodeEntered, S::ShowLoginWaitingDialog },
        { S::ShowDeviceVerificationCodeInput, T::DialogAborted, S::LoginAborted },
        { S::ShowTwoFactorEMailInput, T::TwoFactorCodeEntered, S::ShowLoginWaitingDialog },
        { S::ShowTwoFactorEMailInput, T::DialogAborted, S::LoginAborted },
        { S::ShowLoginConfirmationInput, T::LoginConfirmationCodeEntered, S::ShowLoginWaitingDialog },
        { S::ShowLoginConfirmationInput, T::DialogAborted, S::LoginAborted },

        { S::ShowCharacterSelection, T::CharacterLoginRequested, S::StartGameServerLogin },
        { S::ShowCharacterSelection, T::DialogAborted, S::LoginAborted },
        { S::ShowCharacterSelection, T::AdvanceToLoginUpload, S::StartGameServerLogin },
        { S::StartGameServerLogin, T::AdvanceToWaitForAssetCompletion, S::WaitForAssetsCompletion },
        { S::StartGameServerLogin, T::AdvanceStateMachine, S::LoginFinishedSuccessfully },
        { S::WaitForAssetsCompletion, T::AssetsComplete, S::StartGameServerLogin },
        { S::WaitForAssetsCompletion, T::MessageDialogDiscarded, S::ShowCharacterSelection },

        { S::ShowClientOutOfDate, T::MessageDialogAccepted, S::BeginPatchClient },
        { S::BeginPatchClient, T::AdvanceStateMachine, S::LoginAborted },
    };

    using C = TAuthenticationProcessController;
    m_entered = {
        { S::ShowLoginWaitingDialog, &C::onShowLoginWaitingDialogStateEntered },
        { S::LoginFailed, &C::onLoginFailedStateEntered },
        { S::ShowTwoFactorTokenInput, &C::onShowTwoFactorTokenInputStateEntered },
        { S::ShowDeviceVerificationCodeInput, &C::onShowDeviceVerificationCodeInputStateEntered },
        { S::ShowTwoFactorEMailInput, &C::onShowTwoFactorEMailInputStateEntered },
        { S::ShowLoginConfirmationInput, &C::onShowLoginConfirmationInputStateEntered },
        { S::ShowReturnerReward, &C::onShowReturnerRewardStateEntered },
        { S::ShowEmergencyDialog, &C::onShowEmergencyDialogEntered },
        { S::ShowCharacterTradeDuePayment, &C::onShowCharacterTradeDuePaymentEntered },
        { S::ShowSurveyInvitation, &C::onShowSurveyInvitationStateEntered },
        { S::ShowCharacterSelection, &C::onShowCharacterSelectionStateEntered },
        { S::StartGameServerLogin, &C::onStartGameServerLoginStateEntered },
        { S::WaitForAssetsCompletion, &C::onWaitForAssetsCompletionEntered },
        { S::ShowClientOutOfDate, &C::onShowClientOutOfDateStateEntered },
        { S::BeginPatchClient, &C::onBeginPatchClientStateEntered },
        { S::LoginFinishedSuccessfully, &C::onLoginFinishedSuccessfullyEntered },
        { S::LoginAborted, &C::onLoginAbortedStateEntered },
        { S::SolveCloudflareChallenge, &C::onSolveCloudflareChallengeStateEntered },
        { S::ShowCloudflareChallengeFailed, &C::onShowCloudflareChallengeFailedStateEntered },
    };
    m_exited = {
        { S::ShowLoginWaitingDialog, &C::onShowLoginWaitingDialogStateLeft },
        { S::LoginFailed, &C::onLoginFailedStateLeft },
        { S::ShowTwoFactorTokenInput, &C::onShowTwoFactorTokenInputStateLeft },
        { S::ShowDeviceVerificationCodeInput, &C::onShowDeviceVerificationCodeInputStateLeft },
        { S::ShowTwoFactorEMailInput, &C::onShowTwoFactorEMailInputStateLeft },
        { S::ShowLoginConfirmationInput, &C::onShowLoginConfirmationInputStateLeft },
        { S::ShowCharacterSelection, &C::onShowCharacterSelectionStateLeft },
        { S::WaitForAssetsCompletion, &C::onWaitForAssetsCompletionLeft },
    };

    // The machine's own advance signals are triggers too.
    connect(this, &C::advanceStateMachine, this, [this] { trigger(T::AdvanceStateMachine); });
    connect(this, &C::advanceStateMachineToLoginUpload, this, [this] { trigger(T::AdvanceToLoginUpload); });
    connect(this, &C::advanceStateMachineDirectlyToCharacterSelection, this,
            [this] { trigger(T::AdvanceDirectlyToCharacterSelection); });
    connect(this, &C::advanceStateMachineToWaitForAssetCompletion, this,
            [this] { trigger(T::AdvanceToWaitForAssetCompletion); });

    // The code request uploaders' answers.
    if (m_pEMailCodeRequestUploader) {
        TEmailCodeRequestUploader* uploader = m_pEMailCodeRequestUploader.get();
        connect(uploader, &TEmailCodeRequestUploader::tooManyEmailCodeRequests, this, &C::onTooManyEMailCodeRequests);
        connect(uploader, &TEmailCodeRequestUploader::alreadyRequestedEMailCodeRecently, this,
                &C::onAlreadyRequestedEmailCodeRecently);
        connect(uploader, &TEmailCodeRequestUploader::emailCodeRequestCompleted, this, &C::onEMailCodeRequestCompleted);
    }
    if (m_pLoginConfirmationCodeRequestUploader) {
        TLoginConfirmationCodeRequestUploader* uploader = m_pLoginConfirmationCodeRequestUploader.get();
        connect(uploader, &TLoginConfirmationCodeRequestUploader::tooManyLoginConfirmationCodeRequests, this,
                &C::onTooManyLoginConfirmationCodeRequests);
        connect(uploader, &TLoginConfirmationCodeRequestUploader::alreadyRequestedLoginConfirmationCodeRecently, this,
                &C::onAlreadyRequestedLoginConfirmationCodeRecently);
        connect(uploader, &TLoginConfirmationCodeRequestUploader::invalidLoginConfirmationToken, this,
                &C::onInvalidLoginConfirmationToken);
        connect(uploader, &TLoginConfirmationCodeRequestUploader::loginConfirmationCodeRequestSuccessful, this,
                &C::onLoginConfirmationCodeRequestSuccessful);
    }
}

void TAuthenticationProcessController::setLoginCredentials(const QString& EMail, const QString& Password)
{
    m_eMail = EMail;
    m_password = Password;
}

bool TAuthenticationProcessController::isPostLoginDialog(const EState state)
{
    switch (state) {
    case EState::ShowReturnerReward:
    case EState::ShowEmergencyDialog:
    case EState::ShowCharacterTradeDuePayment:
    case EState::ShowSurveyInvitation:
    case EState::PostLoginDialogsFinished:
        return true;
    default:
        return false;
    }
}

void TAuthenticationProcessController::start(const bool shortcutToCharacterSelection)
{
    // A new login replaces a machine still running (a failed login waits on
    // its message forever when nobody accepts it).
    m_running = true;
    m_queue.clear();
    m_child.reset();
    m_state = EState::Start;
    Q_EMIT stateEntered(m_state);
    // The machine's started signal: one of the two start slots.
    if (shortcutToCharacterSelection)
        onLoginStateMachineStartedShortcutToCharacterSelection();
    else
        onLoginStateMachineStarted();
}

void TAuthenticationProcessController::trigger(const ETrigger trigger)
{
    if (!m_running)
        return;
    m_queue.push_back(trigger);
    if (m_processing)
        return;
    m_processing = true;
    while (m_running && !m_queue.empty()) {
        const ETrigger next = m_queue.front();
        m_queue.pop_front();
        process(next);
    }
    m_processing = false;
}

void TAuthenticationProcessController::process(const ETrigger trigger)
{
    // A child of the compound state handles a trigger before the compound.
    const EState active = m_child.value_or(m_state);
    const auto it = std::find_if(m_transitions.begin(), m_transitions.end(), [&](const TTransition& transition) {
        return transition.trigger == trigger && transition.from == active;
    });
    if (it == m_transitions.end())
        return;

    if (isPostLoginDialog(it->to)) {
        exit(active);
        m_child = it->to;
        enter(it->to);
        if (it->to == EState::PostLoginDialogsFinished) {
            // The compound's final child: its finished signal leads to the
            // character selection.
            exit(EState::PostLoginDialogsFinished);
            m_child.reset();
            exit(EState::PostLoginDialogs);
            m_state = EState::ShowCharacterSelection;
            enter(m_state);
        }
        return;
    }

    if (m_child) {
        exit(*m_child);
        m_child.reset();
    }
    exit(m_state);
    m_state = it->to;
    if (m_state == EState::PostLoginDialogs) {
        enter(EState::PostLoginDialogs);
        m_child = EState::ShowReturnerReward;
        enter(EState::ShowReturnerReward);
        return;
    }
    enter(m_state);
    if (m_state == EState::LoginFinishedSuccessfully || m_state == EState::LoginAborted)
        stop();
}

void TAuthenticationProcessController::enter(const EState state)
{
    for (const auto& [s, slot] : m_entered) {
        if (s == state)
            (this->*slot)();
    }
    Q_EMIT stateEntered(state);
}

void TAuthenticationProcessController::exit(const EState state)
{
    for (const auto& [s, slot] : m_exited) {
        if (s == state)
            (this->*slot)();
    }
}

void TAuthenticationProcessController::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_queue.clear();
    Q_EMIT finished();
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

void TAuthenticationProcessController::showCharacterSelection()
{
    if (!m_authenticated)
        return;
    if (m_running) {
        Q_EMIT advanceStateMachineDirectlyToCharacterSelection();
        return;
    }
    start(true);
}

void TAuthenticationProcessController::resetSessionData()
{
    if (m_host.resetSessionData)
        m_host.resetSessionData();
}

void TAuthenticationProcessController::onLoginStateMachineStartedShortcutToCharacterSelection()
{
    Q_EMIT advanceStateMachineDirectlyToCharacterSelection();
}

void TAuthenticationProcessController::onLoginStateMachineStarted()
{
    Q_EMIT advanceStateMachineToLoginUpload();
}

void TAuthenticationProcessController::onShowLoginWaitingDialogStateEntered()
{
    // Host: the game client shows the connecting dialog and sends the request.
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
    if (m_pCharacterSelectionController)
        m_pCharacterSelectionController->open();
}

void TAuthenticationProcessController::onShowCharacterSelectionStateLeft()
{
    m_characterSelectionVisible = false;
    if (m_pCharacterSelectionController)
        m_pCharacterSelectionController->close();
}

void TAuthenticationProcessController::onStartGameServerLoginStateEntered()
{
    // The character selection's choice; its first character becomes the one
    // the selection preselects next time. (The standalone tests run the
    // machine without a character selection.)
    if (m_pCharacterSelectionController) {
        m_characterLoginData = m_pCharacterSelectionController->loginData();
        if (!m_characterLoginData.empty())
            m_pCharacterSelectionController->setLastSelectedCharacterName(m_characterLoginData.front().characterName);
    }
    // The character's game server login waits for the assets.
    if (!m_host.assetsComplete || m_host.assetsComplete()) {
        Q_EMIT requestCharacterGameserverLogin();
        Q_EMIT advanceStateMachine();
    } else {
        Q_EMIT advanceStateMachineToWaitForAssetCompletion();
    }
}

void TAuthenticationProcessController::onWaitForAssetsCompletionEntered()
{
    m_waitingForAssets = true;
}

void TAuthenticationProcessController::onWaitForAssetsCompletionLeft()
{
    m_waitingForAssets = false;
}

void TAuthenticationProcessController::onLoginFinishedSuccessfullyEntered()
{
    m_characterSelectionVisible = false;
    m_loginWaitingDialogVisible = false;
}

void TAuthenticationProcessController::onLoginAbortedStateEntered()
{
    // The dialogs close and the login request stops.
    m_characterSelectionVisible = false;
    m_loginWaitingDialogVisible = false;
    if (m_host.abortLogin)
        m_host.abortLogin();
    setAuthenticated(false);
    stop();
    Q_EMIT abort();
}

void TAuthenticationProcessController::onShowReturnerRewardStateEntered()
{
    if (m_host.showReturnerRewardDialog && m_host.showReturnerRewardDialog())
        return;
    Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onShowEmergencyDialogEntered()
{
    if (m_host.showEmergencyDialog && m_host.showEmergencyDialog())
        return;
    Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onShowCharacterTradeDuePaymentEntered()
{
    if (m_host.showCharacterTradeDuePaymentDialog && m_host.showCharacterTradeDuePaymentDialog())
        return;
    Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onShowSurveyInvitationStateEntered()
{
    if (m_host.showSurveyInvitationDialog && m_host.showSurveyInvitationDialog())
        return;
    Q_EMIT advanceStateMachine();
}

void TAuthenticationProcessController::onShowClientOutOfDateStateEntered()
{
    // Host: the game client shows the out-of-date message; accepting it
    // patches the client.
}

void TAuthenticationProcessController::onBeginPatchClientStateEntered()
{
    Q_EMIT advanceStateMachine();
    Q_EMIT quitAndPatchClient();
}

void TAuthenticationProcessController::onRequestNewEMailCode()
{
    m_pEMailCodeRequestUploader->requestEmailCode(m_eMail, m_password);
}

void TAuthenticationProcessController::onRequestNewLoginConfirmationCode(const ELoginConfirmationMethod Method)
{
    // The refused login's confirmation token goes with the request.
    m_pLoginConfirmationCodeRequestUploader->requestLoginConfirmationCode(m_eMail, m_password, m_confirmationToken,
                                                                          Method);
}

void TAuthenticationProcessController::onTooManyEMailCodeRequests()
{
}

void TAuthenticationProcessController::onAlreadyRequestedEmailCodeRecently()
{
}

void TAuthenticationProcessController::onEMailCodeRequestCompleted(const bool)
{
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

void TAuthenticationProcessController::onLoginConfirmationCodeRequestSuccessful(const bool)
{
}

void TAuthenticationProcessController::onLoginFailedDueToMissingLoginConfirmation(const QString ConfirmationToken,
                                                                                 const bool HasPendingCode)
{
    m_confirmationToken = ConfirmationToken;
    m_hasPendingConfirmationCode = HasPendingCode;
}

void TAuthenticationProcessController::onPlayerLevelUp()
{
}

void TAuthenticationProcessController::onPlayerDataChanged()
{
}

void TAuthenticationProcessController::onSolveCloudflareChallengeStateEntered()
{
    // Host: the game client runs the Cloudflare solver.
}

void TAuthenticationProcessController::onShowCloudflareChallengeFailedStateEntered()
{
    setAuthenticated(false);
}

void TAuthenticationProcessController::onCloudflareSolverSuccess(const TCloudflareChallengeClearanceInfo CloudflareClearanceInfo)
{
    // The clearance goes with the next login request.
    m_cloudflareClearance = CloudflareClearanceInfo;
    if (m_host.applyClearance)
        m_host.applyClearance(CloudflareClearanceInfo);
    Q_EMIT advanceStateMachineToLoginUpload();
}

void TAuthenticationProcessController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (pGameAction->gameActionType() != input::EGameAction::FINISHED_TUTORIAL_FOR_CHARACTER)
        return;
    const auto* action = input::gameActionAs<input::TGameActionFinishedTutorialForCharacter>(pGameAction);
    const QString characterName = action->characterName();
    m_pCharacterSelectionController->clearTutorialFlag(characterName);
    m_pLoginRequestUploader->setTutorialFinished(characterName);
    m_characterLoginData = m_pCharacterSelectionController->loginData();
}

} // namespace tibia::authentication
