#include <gtest/gtest.h>

#include "authentication/authenticationprocesscontroller.h"
#include "authentication/emailcoderequestuploader.h"
#include "authentication/loginconfirmationcoderequestuploader.h"
#include "jsondocumenthttppostsender.h"

#include <QCoreApplication>
#include <QJsonObject>

#include <memory>
#include <vector>

using tibia::authentication::TAuthenticationProcessController;
using tibia::authentication::TEmailCodeRequestUploader;
using tibia::authentication::TLoginConfirmationCodeRequestUploader;
using tibia::shared::TJsonDocumentHttpPostSender;

namespace {

QCoreApplication& application()
{
    static int argc = 1;
    static char name[] = "authenticationprocesscontrollercoderequests_test";
    static char* argv[] = { name, nullptr };
    // Never destroyed: a static application torn down after Qt's own statics
    // crashes at exit.
    static QCoreApplication* app = new QCoreApplication(argc, argv);
    return *app;
}

/// Records what would be posted.
class TFakeSender final : public TJsonDocumentHttpPostSender
{
public:
    void send(const QUrl&, const QJsonDocument& document) override { documents.push_back(document); }

    std::vector<QJsonDocument> documents;
};

const QUrl Url(QStringLiteral("https://login.example/service"));

struct Fixture
{
    std::shared_ptr<TFakeSender> eMailSender = std::make_shared<TFakeSender>();
    std::shared_ptr<TFakeSender> confirmationSender = std::make_shared<TFakeSender>();
    TAuthenticationProcessController controller{
        {}, {}, std::make_shared<TEmailCodeRequestUploader>(Url, eMailSender),
        std::make_shared<TLoginConfirmationCodeRequestUploader>(Url, confirmationSender), {}
    };

    Fixture()
    {
        application();
        controller.setLoginCredentials(QStringLiteral("a@b.c"), QStringLiteral("secret"));
    }
};

}

TEST(AuthenticationProcessControllerCodeRequests, NewEMailCodeIsRequestedForTheLoginAccount)
{
    Fixture f;
    f.controller.onRequestNewEMailCode();
    ASSERT_EQ(f.eMailSender->documents.size(), 1u);
    const QJsonObject request = f.eMailSender->documents[0].object();
    EXPECT_EQ(request[QStringLiteral("type")].toString(), QStringLiteral("emailcoderequest"));
    EXPECT_EQ(request[QStringLiteral("email")].toString(), QStringLiteral("a@b.c"));
    EXPECT_EQ(request[QStringLiteral("password")].toString(), QStringLiteral("secret"));
    EXPECT_TRUE(f.confirmationSender->documents.empty());
}

TEST(AuthenticationProcessControllerCodeRequests, NewLoginConfirmationCodeCarriesTheRefusedLoginsToken)
{
    Fixture f;
    f.controller.onLoginFailedDueToMissingLoginConfirmation(QStringLiteral("token"), true);
    f.controller.onRequestNewLoginConfirmationCode(tibia::authentication::LoginConfirmationMethodSms);
    ASSERT_EQ(f.confirmationSender->documents.size(), 1u);
    const QJsonObject request = f.confirmationSender->documents[0].object();
    EXPECT_EQ(request[QStringLiteral("type")].toString(), QStringLiteral("loginconfirmationcoderequest"));
    EXPECT_EQ(request[QStringLiteral("email")].toString(), QStringLiteral("a@b.c"));
    EXPECT_EQ(request[QStringLiteral("password")].toString(), QStringLiteral("secret"));
    EXPECT_EQ(request[QStringLiteral("loginconfirmationtoken")].toString(), QStringLiteral("token"));
    EXPECT_EQ(request[QStringLiteral("loginconfirmationmethod")].toString(), QStringLiteral("sms"));
    EXPECT_TRUE(f.eMailSender->documents.empty());
}
