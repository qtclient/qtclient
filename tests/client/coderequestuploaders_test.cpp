#include <gtest/gtest.h>

#include "authentication/emailcoderequestuploader.h"
#include "authentication/loginconfirmationcoderequestuploader.h"
#include "jsondocumenthttppostsender.h"

#include <QCoreApplication>
#include <QJsonObject>

#include <memory>
#include <vector>

using tibia::authentication::TEmailCodeRequestUploader;
using tibia::authentication::TLoginConfirmationCodeRequestUploader;
using tibia::shared::TJsonDocumentHttpPostSender;

namespace {

QCoreApplication& application()
{
    static int argc = 1;
    static char name[] = "coderequestuploaders_test";
    static char* argv[] = { name, nullptr };
    // Never destroyed: a static application torn down after Qt's own statics
    // crashes at exit.
    static QCoreApplication* app = new QCoreApplication(argc, argv);
    return *app;
}

/// Records what would be posted; answers are fed back through finished().
class TFakeSender final : public TJsonDocumentHttpPostSender
{
public:
    void send(const QUrl& url, const QJsonDocument& document) override
    {
        urls.push_back(url);
        documents.push_back(document);
    }

    void answer(const QByteArray& reply) { Q_EMIT finished(QNetworkReply::NoError, QString(), reply); }

    std::vector<QUrl> urls;
    std::vector<QJsonDocument> documents;
};

const QUrl Url(QStringLiteral("https://login.example/service"));

}

TEST(EmailCodeRequestUploader, PostsTheRequestToItsUrl)
{
    application();
    auto sender = std::make_shared<TFakeSender>();
    TEmailCodeRequestUploader uploader(Url, sender);
    uploader.requestEmailCode(QStringLiteral("a@b.c"), QStringLiteral("secret"));
    ASSERT_EQ(sender->documents.size(), 1u);
    EXPECT_EQ(sender->urls[0], Url);
    const QJsonObject request = sender->documents[0].object();
    EXPECT_EQ(request.size(), 3);
    EXPECT_EQ(request[QStringLiteral("type")].toString(), QStringLiteral("emailcoderequest"));
    EXPECT_EQ(request[QStringLiteral("email")].toString(), QStringLiteral("a@b.c"));
    EXPECT_EQ(request[QStringLiteral("password")].toString(), QStringLiteral("secret"));
}

TEST(EmailCodeRequestUploader, ReportsTheServersAnswer)
{
    application();
    auto sender = std::make_shared<TFakeSender>();
    TEmailCodeRequestUploader uploader(Url, sender);
    std::vector<bool> completed;
    int tooMany = 0;
    int recently = 0;
    QObject::connect(&uploader, &TEmailCodeRequestUploader::emailCodeRequestCompleted,
                     [&](const bool success) { completed.push_back(success); });
    QObject::connect(&uploader, &TEmailCodeRequestUploader::tooManyEmailCodeRequests, [&] { ++tooMany; });
    QObject::connect(&uploader, &TEmailCodeRequestUploader::alreadyRequestedEMailCodeRecently, [&] { ++recently; });

    sender->answer(R"({"emailcoderequestsuccessful":true})");
    sender->answer(R"({})");
    sender->answer(R"({"errorCode":9,"errorMessage":"x"})");
    sender->answer(R"({"errorCode":10,"errorMessage":"x"})");
    sender->answer(R"({"errorCode":3,"errorMessage":"x"})");
    EXPECT_EQ(completed, (std::vector<bool>{ true, false, false }));
    EXPECT_EQ(tooMany, 1);
    EXPECT_EQ(recently, 1);
}

TEST(LoginConfirmationCodeRequestUploader, PostsTheMethodByName)
{
    application();
    auto sender = std::make_shared<TFakeSender>();
    TLoginConfirmationCodeRequestUploader uploader(Url, sender);
    uploader.requestLoginConfirmationCode(QStringLiteral("a@b.c"), QStringLiteral("secret"), QStringLiteral("token"),
                                          tibia::authentication::LoginConfirmationMethodEmailSecondary);
    uploader.requestLoginConfirmationCode(QStringLiteral("a@b.c"), QStringLiteral("secret"), QStringLiteral("token"),
                                          tibia::authentication::LoginConfirmationMethodSms);
    uploader.requestLoginConfirmationCode(QStringLiteral("a@b.c"), QStringLiteral("secret"), QStringLiteral("token"),
                                          static_cast<tibia::authentication::ELoginConfirmationMethod>(7));
    ASSERT_EQ(sender->documents.size(), 3u);
    const QJsonObject request = sender->documents[0].object();
    EXPECT_EQ(request.size(), 5);
    EXPECT_EQ(request[QStringLiteral("type")].toString(), QStringLiteral("loginconfirmationcoderequest"));
    EXPECT_EQ(request[QStringLiteral("email")].toString(), QStringLiteral("a@b.c"));
    EXPECT_EQ(request[QStringLiteral("password")].toString(), QStringLiteral("secret"));
    EXPECT_EQ(request[QStringLiteral("loginconfirmationmethod")].toString(), QStringLiteral("emailSecondary"));
    EXPECT_EQ(request[QStringLiteral("loginconfirmationtoken")].toString(), QStringLiteral("token"));
    EXPECT_EQ(sender->documents[1].object()[QStringLiteral("loginconfirmationmethod")].toString(), QStringLiteral("sms"));
    EXPECT_EQ(sender->documents[2].object()[QStringLiteral("loginconfirmationmethod")].toString(), QString());
}

TEST(LoginConfirmationCodeRequestUploader, ReportsTheServersAnswer)
{
    application();
    auto sender = std::make_shared<TFakeSender>();
    TLoginConfirmationCodeRequestUploader uploader(Url, sender);
    std::vector<bool> successful;
    int tooMany = 0;
    int recently = 0;
    int invalidToken = 0;
    QObject::connect(&uploader, &TLoginConfirmationCodeRequestUploader::loginConfirmationCodeRequestSuccessful,
                     [&](const bool value) { successful.push_back(value); });
    QObject::connect(&uploader, &TLoginConfirmationCodeRequestUploader::tooManyLoginConfirmationCodeRequests,
                     [&] { ++tooMany; });
    QObject::connect(&uploader, &TLoginConfirmationCodeRequestUploader::alreadyRequestedLoginConfirmationCodeRecently,
                     [&] { ++recently; });
    QObject::connect(&uploader, &TLoginConfirmationCodeRequestUploader::invalidLoginConfirmationToken,
                     [&] { ++invalidToken; });

    sender->answer(R"({"loginconfirmationcoderequestsuccessful":true})");
    sender->answer(R"({"loginconfirmationcoderequestsuccessful":false})");
    sender->answer(R"({"errorCode":13,"errorMessage":"x"})");
    sender->answer(R"({"errorCode":14,"errorMessage":"x"})");
    sender->answer(R"({"errorCode":15,"errorMessage":"x"})");
    // Any other refusal is not reported.
    sender->answer(R"({"errorCode":3,"errorMessage":"x"})");
    EXPECT_EQ(successful, (std::vector<bool>{ true, false }));
    EXPECT_EQ(tooMany, 1);
    EXPECT_EQ(recently, 1);
    EXPECT_EQ(invalidToken, 1);
}
