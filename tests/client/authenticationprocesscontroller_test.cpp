#include <gtest/gtest.h>

#include "authentication/authenticationprocesscontroller.h"

#include <QObject>
#include <QSignalSpy>

using tibia::authentication::TAuthenticationProcessController;
using EState = TAuthenticationProcessController::EState;
using ETrigger = TAuthenticationProcessController::ETrigger;

/// A login connection that answers whatever the test tells it to.
class FakeLoginConnection : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void loginSuccessful(int result);
    void loginFailed(const QString& ErrorMessage);
    void loginFailedDueToMissingTwoFactorToken();
    void loginFailedDueToMissingEMailLoginCode();
    void loginFailedDueToMissingLoginConfirmation(const QString& ConfirmationToken, bool hasPendingCode);
    void loginFailedDueToInvalidServerResponse();
    void loginFailedDueToInvalidDeviceCookie();
    void loginFailedDueToCloudflareMitigation();
    void loginFailedClientOutOfDate();
};

namespace {

struct Fixture
{
    TAuthenticationProcessController controller;
    FakeLoginConnection connection;
    int aborts = 0;
    bool assetsReady = true;
    bool returnerReward = false;

    Fixture()
    {
        TAuthenticationProcessController::THost host;
        host.abortLogin = [this] { ++aborts; };
        host.assetsComplete = [this] { return assetsReady; };
        host.showReturnerRewardDialog = [this] { return returnerReward; };
        controller.setHost(std::move(host));
        controller.bindLoginUploader(&connection);
    }
};

}

TEST(AuthenticationProcessController, LoginThroughCharacterSelectionToGameServer)
{
    Fixture f;
    QSignalSpy gameLogin(&f.controller, &TAuthenticationProcessController::requestCharacterGameserverLogin);
    QSignalSpy finished(&f.controller, &TAuthenticationProcessController::finished);
    f.controller.start();
    EXPECT_EQ(f.controller.state(), EState::ShowLoginWaitingDialog);
    EXPECT_TRUE(f.controller.loginWaitingDialogVisible());

    Q_EMIT f.connection.loginSuccessful(0);
    // No post-login dialog is due: straight through to the character selection.
    EXPECT_EQ(f.controller.state(), EState::ShowCharacterSelection);
    EXPECT_FALSE(f.controller.postLoginDialogState().has_value());
    EXPECT_FALSE(f.controller.loginWaitingDialogVisible());
    EXPECT_TRUE(f.controller.characterSelectionVisible());
    EXPECT_TRUE(f.controller.authenticated());

    f.controller.trigger(ETrigger::CharacterLoginRequested);
    EXPECT_EQ(gameLogin.count(), 1);
    EXPECT_EQ(f.controller.state(), EState::LoginFinishedSuccessfully);
    EXPECT_FALSE(f.controller.isRunning());
    EXPECT_EQ(finished.count(), 1);
}

TEST(AuthenticationProcessController, GameServerLoginWaitsForAssets)
{
    Fixture f;
    f.assetsReady = false;
    QSignalSpy gameLogin(&f.controller, &TAuthenticationProcessController::requestCharacterGameserverLogin);
    f.controller.start(true);
    EXPECT_EQ(f.controller.state(), EState::ShowCharacterSelection);
    f.controller.trigger(ETrigger::CharacterLoginRequested);
    EXPECT_EQ(f.controller.state(), EState::WaitForAssetsCompletion);
    EXPECT_TRUE(f.controller.waitingForAssets());
    EXPECT_EQ(gameLogin.count(), 0);

    f.assetsReady = true;
    f.controller.trigger(ETrigger::AssetsComplete);
    EXPECT_EQ(gameLogin.count(), 1);
    EXPECT_FALSE(f.controller.waitingForAssets());
    EXPECT_EQ(f.controller.state(), EState::LoginFinishedSuccessfully);
}

TEST(AuthenticationProcessController, PostLoginDialogWaitsUntilClosed)
{
    Fixture f;
    f.returnerReward = true;
    f.controller.start();
    Q_EMIT f.connection.loginSuccessful(0);
    EXPECT_EQ(f.controller.state(), EState::PostLoginDialogs);
    EXPECT_EQ(f.controller.postLoginDialogState(), EState::ShowReturnerReward);
    // Triggers of other states change nothing.
    f.controller.trigger(ETrigger::CharacterLoginRequested);
    EXPECT_EQ(f.controller.postLoginDialogState(), EState::ShowReturnerReward);

    f.controller.trigger(ETrigger::ReturnerRewardDialogClosed);
    EXPECT_EQ(f.controller.state(), EState::ShowCharacterSelection);
}

TEST(AuthenticationProcessController, TwoFactorTokenRetriesTheLogin)
{
    Fixture f;
    f.controller.start();
    Q_EMIT f.connection.loginFailedDueToMissingTwoFactorToken();
    EXPECT_EQ(f.controller.state(), EState::ShowTwoFactorTokenInput);
    EXPECT_FALSE(f.controller.loginWaitingDialogVisible());
    f.controller.trigger(ETrigger::TwoFactorCodeEntered);
    EXPECT_EQ(f.controller.state(), EState::ShowLoginWaitingDialog);
    EXPECT_TRUE(f.controller.loginWaitingDialogVisible());
}

TEST(AuthenticationProcessController, LoginConfirmationKeepsTheToken)
{
    Fixture f;
    f.controller.start();
    Q_EMIT f.connection.loginFailedDueToMissingLoginConfirmation(QStringLiteral("token"), true);
    EXPECT_EQ(f.controller.state(), EState::ShowLoginConfirmationInput);
    EXPECT_EQ(f.controller.confirmationToken(), QStringLiteral("token"));
    EXPECT_TRUE(f.controller.hasPendingConfirmationCode());
}

TEST(AuthenticationProcessController, FailedLoginAbortsAfterTheMessage)
{
    Fixture f;
    QSignalSpy aborted(&f.controller, &TAuthenticationProcessController::abort);
    f.controller.start();
    Q_EMIT f.connection.loginFailed(QStringLiteral("wrong password"));
    EXPECT_EQ(f.controller.state(), EState::LoginFailed);
    EXPECT_FALSE(f.controller.authenticated());
    f.controller.trigger(ETrigger::MessageDialogAccepted);
    EXPECT_EQ(f.controller.state(), EState::LoginAborted);
    EXPECT_EQ(f.aborts, 1);
    EXPECT_EQ(aborted.count(), 1);
    EXPECT_FALSE(f.controller.isRunning());
    // A stopped machine ignores triggers.
    f.controller.trigger(ETrigger::AdvanceToLoginUpload);
    EXPECT_EQ(f.controller.state(), EState::LoginAborted);
}

TEST(AuthenticationProcessController, CloudflareClearanceRetriesTheLogin)
{
    Fixture f;
    f.controller.start();
    Q_EMIT f.connection.loginFailedDueToCloudflareMitigation();
    EXPECT_EQ(f.controller.state(), EState::SolveCloudflareChallenge);
    f.controller.onCloudflareSolverSuccess({});
    EXPECT_EQ(f.controller.state(), EState::ShowLoginWaitingDialog);

    Q_EMIT f.connection.loginFailedDueToCloudflareMitigation();
    f.controller.trigger(ETrigger::CloudflareSolveFailed);
    EXPECT_EQ(f.controller.state(), EState::ShowCloudflareChallengeFailed);
}

TEST(AuthenticationProcessController, ClientOutOfDatePatchesTheClient)
{
    Fixture f;
    QSignalSpy patch(&f.controller, &TAuthenticationProcessController::quitAndPatchClient);
    f.controller.start();
    Q_EMIT f.connection.loginFailedClientOutOfDate();
    EXPECT_EQ(f.controller.state(), EState::ShowClientOutOfDate);
    EXPECT_EQ(patch.count(), 0);
    f.controller.trigger(ETrigger::MessageDialogAccepted);
    EXPECT_EQ(patch.count(), 1);
    EXPECT_EQ(f.controller.state(), EState::LoginAborted);
}

TEST(AuthenticationProcessController, CharacterSelectionAbortStopsTheMachine)
{
    Fixture f;
    f.controller.start(true);
    f.controller.trigger(ETrigger::DialogAborted);
    EXPECT_EQ(f.controller.state(), EState::LoginAborted);
    EXPECT_FALSE(f.controller.characterSelectionVisible());
}

TEST(AuthenticationProcessController, NewLoginRestartsAPendingMachine)
{
    Fixture f;
    f.controller.start();
    Q_EMIT f.connection.loginFailed(QStringLiteral("wrong password"));
    EXPECT_EQ(f.controller.state(), EState::LoginFailed);
    f.controller.start();
    EXPECT_EQ(f.controller.state(), EState::ShowLoginWaitingDialog);
    Q_EMIT f.connection.loginSuccessful(0);
    EXPECT_EQ(f.controller.state(), EState::ShowCharacterSelection);
}

#include "authenticationprocesscontroller_test.moc"
