#include "authentication/authenticationprocesscontroller.h"
#include "authentication/cloudflarechallengecontroller.h"
#include "authentication/emailcoderequestuploader.h"
#include "authentication/loginconfirmationcoderequestuploader.h"
#include "authentication/loginrequestuploader.h"
#include "config/clientoptions.h"
#include "controllers/characterselectioncontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameaction.h"

#include <QCoreApplication>

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
    , m_pTwoFactorCodeEnterTextDialogController(std::make_unique<gamewindow::TTwoFactorCodeEnterTextDialogController>())
    , m_pLoginConfirmationCodeEnterTextDialogController(
          std::make_unique<gamewindow::TLoginConfirmationCodeEnterTextDialogController>())
    , m_pDeviceVerificationCodeEnterTextDialogController(std::make_unique<gamewindow::TGenericEnterTextDialogController>(
          qtTrId("request_device_verification_code_caption"), qtTrId("request_device_verification_code_text"), 10))
    , m_pMessageDialogController(std::make_unique<gamewindow::TMessageDialogController>())
    , m_pSurveyInvitationDialogController(std::make_unique<gamewindow::TSurveyInvitationDialogController>())
    , m_pReturnerRewardDialogController(std::make_unique<gamewindow::TReturnerRewardDialogController>())
    , m_pEmergencyDialogController(std::make_unique<gamewindow::TEmergencyDialogController>())
    , m_pCharacterTradeDuePaymentDialogController(std::make_unique<gamewindow::TCharacterTradeDuePaymentDialogController>())
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

    // The dialogs' answers move the machine.
    {
        gamewindow::TMessageDialogController* message = m_pMessageDialogController.get();
        connect(message, &gamewindow::TMessageDialogController::acceptClicked, this,
                [this] { trigger(T::MessageDialogAccepted); });
        connect(message, &gamewindow::TMessageDialogController::discardClicked, this,
                [this] { trigger(T::MessageDialogDiscarded); });
        gamewindow::TTwoFactorCodeEnterTextDialogController* twoFactor = m_pTwoFactorCodeEnterTextDialogController.get();
        connect(twoFactor, &gamewindow::TTwoFactorCodeEnterTextDialogController::textEntered, this,
                [this] { trigger(T::TwoFactorCodeEntered); });
        connect(twoFactor, &gamewindow::TTwoFactorCodeEnterTextDialogController::abort, this,
                [this] { trigger(T::DialogAborted); });
        connect(twoFactor, &gamewindow::TTwoFactorCodeEnterTextDialogController::buttonClicked, this,
                &C::onRequestNewEMailCode);
        gamewindow::TGenericEnterTextDialogController* deviceVerification =
            m_pDeviceVerificationCodeEnterTextDialogController.get();
        connect(deviceVerification, &gamewindow::TGenericEnterTextDialogController::textEntered, this,
                [this] { trigger(T::DeviceVerificationCodeEntered); });
        connect(deviceVerification, &gamewindow::TGenericEnterTextDialogController::abort, this,
                [this] { trigger(T::DialogAborted); });
        gamewindow::TLoginConfirmationCodeEnterTextDialogController* confirmation =
            m_pLoginConfirmationCodeEnterTextDialogController.get();
        connect(confirmation, &gamewindow::TLoginConfirmationCodeEnterTextDialogController::textEntered, this,
                [this] { trigger(T::LoginConfirmationCodeEntered); });
        connect(confirmation, &gamewindow::TLoginConfirmationCodeEnterTextDialogController::abort, this,
                [this] { trigger(T::DialogAborted); });
        connect(confirmation, &gamewindow::TLoginConfirmationCodeEnterTextDialogController::confirmationCodeRequested, this,
                [this](const int Method) { onRequestNewLoginConfirmationCode(static_cast<ELoginConfirmationMethod>(Method)); });
        connect(m_pReturnerRewardDialogController.get(), &gamewindow::TReturnerRewardDialogController::dialogClosed, this,
                [this] { trigger(T::ReturnerRewardDialogClosed); });
        connect(m_pEmergencyDialogController.get(), &gamewindow::TEmergencyDialogController::dialogClosed, this,
                [this] { trigger(T::EmergencyDialogClosed); });
        connect(m_pCharacterTradeDuePaymentDialogController.get(),
                &gamewindow::TCharacterTradeDuePaymentDialogController::dialogClosed, this,
                [this] { trigger(T::CharacterTradeDuePaymentDialogClosed); });
        connect(m_pSurveyInvitationDialogController.get(), &gamewindow::TSurveyInvitationDialogController::dialogClosed, this,
                [this] { trigger(T::SurveyInvitationDialogClosed); });
    }

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
    m_loginData.email = EMail;
    m_loginData.password = Password;
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
    // The connecting message (Cancel aborts the login) while the request runs;
    // the account's device cookie from the client options goes with it.
    m_pMessageDialogController->setButtons(gamewindow::cancelDialogButtons());
    m_pMessageDialogController->showMessage(qtTrId("loginwait_caption"), qtTrId("loginwait_text"));
    if (config::TClientOptions* options = config::TClientOptions::instance())
        m_loginData.deviceCookie = options->deviceCookie(m_loginData.email);
    m_pLoginRequestUploader->uploadLoginRequest(m_loginData);
}

void TAuthenticationProcessController::onShowLoginWaitingDialogStateLeft()
{
    m_pMessageDialogController->dialog()->close();
}

void TAuthenticationProcessController::onLoginFailedStateEntered()
{
    // A request that got no answer (error -100) has no message of its own.
    const QString message = m_pLoginRequestUploader->errorCode() == -100 ? qtTrId("login_failed_error_message")
                                                                         : m_pLoginRequestUploader->errorMessage();
    m_pMessageDialogController->setButtons(gamewindow::okDialogButtons());
    m_pMessageDialogController->showMessage(qtTrId("loginerror_caption"), message);
}

void TAuthenticationProcessController::onLoginFailedStateLeft()
{
    m_pMessageDialogController->dialog()->close();
}

void TAuthenticationProcessController::onShowTwoFactorTokenInputStateEntered()
{
    using EMethod = gamewindow::TTwoFactorCodeEnterTextDialogController::ETwoFactorMethod;
    m_pTwoFactorCodeEnterTextDialogController->setAdditionalInfoText(QString());
    m_pTwoFactorCodeEnterTextDialogController->setTwoFactorMethod(EMethod::Authenticator);
    m_pTwoFactorCodeEnterTextDialogController->dialog()->show();
}

void TAuthenticationProcessController::onShowTwoFactorTokenInputStateLeft()
{
    // The entered authenticator token goes with the next request; without a
    // trusted device the device token is dropped.
    const QString token = m_pTwoFactorCodeEnterTextDialogController->enteredText();
    if (token.isEmpty())
        m_loginData.token.clear();
    else
        m_loginData.token = token;
    if (!m_pTwoFactorCodeEnterTextDialogController->getIsTrustedDevice())
        m_loginData.trustedDeviceToken = QString();
}

void TAuthenticationProcessController::onShowDeviceVerificationCodeInputStateEntered()
{
    m_pDeviceVerificationCodeEnterTextDialogController->showWithText(QString());
}

void TAuthenticationProcessController::onShowDeviceVerificationCodeInputStateLeft()
{
    const QString code = m_pDeviceVerificationCodeEnterTextDialogController->enteredText();
    if (!code.isEmpty()) {
        m_loginData.deviceVerificationCode = code;
        return;
    }
    m_loginData.deviceVerificationCode.clear();
}

void TAuthenticationProcessController::onShowTwoFactorEMailInputStateEntered()
{
    using EMethod = gamewindow::TTwoFactorCodeEnterTextDialogController::ETwoFactorMethod;
    m_pTwoFactorCodeEnterTextDialogController->setTwoFactorMethod(EMethod::Email);
    // The login answer says whether a new email code can be asked for.
    m_pTwoFactorCodeEnterTextDialogController->setEmailResendPossible(m_pLoginRequestUploader->showResendButton());
    m_pTwoFactorCodeEnterTextDialogController->dialog()->show();
}

void TAuthenticationProcessController::onShowTwoFactorEMailInputStateLeft()
{
    const QString code = m_pTwoFactorCodeEnterTextDialogController->enteredText();
    if (code.isEmpty())
        m_loginData.emailCode.clear();
    else
        m_loginData.emailCode = code;
    if (!m_pTwoFactorCodeEnterTextDialogController->getIsTrustedDevice())
        m_loginData.trustedDeviceToken = QString();
}

void TAuthenticationProcessController::onShowLoginConfirmationInputStateEntered()
{
    // Where the login answer says a confirmation code can be sent.
    const TTwoFactorEmailAddresses& addresses = m_pLoginRequestUploader->twoFactorEmailAddresses();
    m_pLoginConfirmationCodeEnterTextDialogController->setAvailableMethods(
        { addresses.primaryEmail, addresses.secondaryEmail, addresses.sms });
    m_pLoginConfirmationCodeEnterTextDialogController->showDialog();
}

void TAuthenticationProcessController::onShowLoginConfirmationInputStateLeft()
{
    m_loginData.loginConfirmationCode = m_pLoginConfirmationCodeEnterTextDialogController->enteredText();
}

void TAuthenticationProcessController::onShowCharacterSelectionStateEntered()
{
    setAuthenticated(true);
    if (m_pCharacterSelectionController)
        m_pCharacterSelectionController->open();
}

void TAuthenticationProcessController::onShowCharacterSelectionStateLeft()
{
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
    // Cancel goes back to the character selection.
    m_pMessageDialogController->setButtons(gamewindow::cancelDialogButtons());
    m_pMessageDialogController->showMessage(qtTrId("login_wait_please_wait"), qtTrId("gamefiles_wait_text"));
}

void TAuthenticationProcessController::onWaitForAssetsCompletionLeft()
{
    m_pMessageDialogController->dialog()->close();
}

void TAuthenticationProcessController::onLoginFinishedSuccessfullyEntered()
{
    if (m_pCharacterSelectionController)
        m_pCharacterSelectionController->close();
    m_pTwoFactorCodeEnterTextDialogController->dialog()->close();
    m_pMessageDialogController->dialog()->close();
    m_pLoginConfirmationCodeEnterTextDialogController->dialog()->close();
}

void TAuthenticationProcessController::stopProcess()
{
    stop();
    if (m_pCharacterSelectionController)
        m_pCharacterSelectionController->close();
    m_pTwoFactorCodeEnterTextDialogController->dialog()->close();
    m_pMessageDialogController->dialog()->close();
    m_pLoginConfirmationCodeEnterTextDialogController->dialog()->close();
}

void TAuthenticationProcessController::onLoginAbortedStateEntered()
{
    // The dialogs close and the login request stops.
    if (m_pCharacterSelectionController)
        m_pCharacterSelectionController->close();
    m_pTwoFactorCodeEnterTextDialogController->dialog()->close();
    m_pMessageDialogController->dialog()->close();
    m_pLoginConfirmationCodeEnterTextDialogController->dialog()->close();
    if (m_host.abortLogin)
        m_host.abortLogin();
    setAuthenticated(false);
    stop();
    Q_EMIT abort();
}

void TAuthenticationProcessController::onShowReturnerRewardStateEntered()
{
    if (!m_pLoginRequestUploader->showReturnerNotification()) {
        Q_EMIT advanceStateMachine();
        return;
    }
    m_pReturnerRewardDialogController->dialog()->create();
    m_pReturnerRewardDialogController->dialog()->show();
}

void TAuthenticationProcessController::onShowEmergencyDialogEntered()
{
    if (!m_pLoginRequestUploader->showImportantBalancingMessage()) {
        Q_EMIT advanceStateMachine();
        return;
    }
    m_pEmergencyDialogController->dialog()->create();
    m_pEmergencyDialogController->dialog()->show();
}

void TAuthenticationProcessController::onShowCharacterTradeDuePaymentEntered()
{
    // The dialog stays away while the player postponed the reminder.
    const std::vector<TCharacterTradePayment>& payments = m_pLoginRequestUploader->characterTradePayments();
    if (!payments.empty() && m_pCharacterTradeDuePaymentDialogController->showDuePayments(payments))
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
    // One button, Restart (Return), which patches the client.
    using Button = gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier;
    m_pMessageDialogController->setButtons({ new gamewindow::TMessageDialogButton(
        Button::YES, qtTrId("restart"), gamewindow::TMessageDialogButton::RETURN_KEYS) });
    m_pMessageDialogController->showMessage(qtTrId("info"), qtTrId("client_too_old"));
}

void TAuthenticationProcessController::onBeginPatchClientStateEntered()
{
    Q_EMIT advanceStateMachine();
    Q_EMIT quitAndPatchClient();
}

void TAuthenticationProcessController::onRequestNewEMailCode()
{
    m_pEMailCodeRequestUploader->requestEmailCode(m_loginData.email, m_loginData.password);
}

void TAuthenticationProcessController::onRequestNewLoginConfirmationCode(const ELoginConfirmationMethod Method)
{
    // The refused login's confirmation token goes with the request.
    m_pLoginConfirmationCodeRequestUploader->requestLoginConfirmationCode(
        m_loginData.email, m_loginData.password, m_loginData.loginConfirmationToken, Method);
}

void TAuthenticationProcessController::onTooManyEMailCodeRequests()
{
    m_pTwoFactorCodeEnterTextDialogController->setEmailResendPossible(false);
    m_pTwoFactorCodeEnterTextDialogController->setAdditionalInfoText(
        qtTrId("request_two_factor_email_code_new_request_reply_too_many_requests"));
}

void TAuthenticationProcessController::onAlreadyRequestedEmailCodeRecently()
{
    m_pTwoFactorCodeEnterTextDialogController->setAdditionalInfoText(
        qtTrId("request_two_factor_email_code_new_request_reply_already_requested_recently"));
}

void TAuthenticationProcessController::onEMailCodeRequestCompleted(const bool Success)
{
    m_pTwoFactorCodeEnterTextDialogController->setAdditionalInfoText(
        Success ? qtTrId("request_two_factor_email_code_new_request_reply_success")
                : qtTrId("request_two_factor_email_code_new_request_reply_fail"));
}

void TAuthenticationProcessController::onTooManyLoginConfirmationCodeRequests()
{
    m_pLoginConfirmationCodeEnterTextDialogController->setAdditionalInfoText(
        qtTrId("request_login_confirmation_code_new_request_reply_too_many_requests"));
}

void TAuthenticationProcessController::onAlreadyRequestedLoginConfirmationCodeRecently()
{
    m_pLoginConfirmationCodeEnterTextDialogController->setAdditionalInfoText(
        qtTrId("request_login_confirmation_code_new_request_reply_already_requested_recently"));
}

void TAuthenticationProcessController::onInvalidLoginConfirmationToken()
{
    m_pLoginConfirmationCodeEnterTextDialogController->onCancelClicked();
}

void TAuthenticationProcessController::onLoginConfirmationCodeRequestSuccessful(const bool Value)
{
    if (!Value) {
        m_pLoginConfirmationCodeEnterTextDialogController->setAdditionalInfoText(
            qtTrId("request_login_confirmation_code_new_request_reply_fail"));
        return;
    }
    m_pLoginConfirmationCodeEnterTextDialogController->setAdditionalInfoText(
        qtTrId("request_login_confirmation_code_new_request_reply_success"));
    m_pLoginConfirmationCodeEnterTextDialogController->setHasPendingConfirmationCode(true);
}

void TAuthenticationProcessController::onLoginFailedDueToMissingLoginConfirmation(const QString ConfirmationToken,
                                                                                 const bool HasPendingCode)
{
    m_pLoginConfirmationCodeEnterTextDialogController->setHasPendingConfirmationCode(HasPendingCode);
    if (ConfirmationToken.isEmpty())
        return;
    // A refusal after a code was sent: the same token means the code was
    // wrong, a new one that a new code is needed.
    if (!m_loginData.loginConfirmationToken.isEmpty()) {
        m_pLoginConfirmationCodeEnterTextDialogController->setAdditionalInfoText(
            m_loginData.loginConfirmationToken == ConfirmationToken
                ? qtTrId("request_login_confirmation_code_wrong")
                : qtTrId("request_login_confirmation_code_new_request_required"));
    }
    m_loginData.loginConfirmationToken = ConfirmationToken;
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
    // Why the challenge failed; OK aborts the login.
    QString message = qtTrId("cloudflare_challenge_error_unknown");
    if (const std::optional<TCloudflareChallengeError>& error = m_pCloudflareChallengeController->error()) {
        if (*error == TCloudflareChallengeError::Cancelled)
            message = qtTrId("cloudflare_challenge_error_cancelled");
        else if (*error == TCloudflareChallengeError::Timeout)
            message = qtTrId("cloudflare_challenge_error_timeout");
    }
    m_pMessageDialogController->setButtons(gamewindow::okDialogButtons());
    m_pMessageDialogController->showMessage(qtTrId("loginerror_caption"), message);
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
