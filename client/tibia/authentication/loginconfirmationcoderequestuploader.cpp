#include "authentication/loginconfirmationcoderequestuploader.h"

#include <QJsonObject>
#include <QJsonValue>

namespace tibia::authentication {

namespace {

const QString TypeKey = QStringLiteral("type");
const QString LoginConfirmationCodeRequestSuccessfulKey = QStringLiteral("loginconfirmationcoderequestsuccessful");
const QString LoginConfirmationCodeRequestType = QStringLiteral("loginconfirmationcoderequest");
const QString EmailKey = QStringLiteral("email");
const QString PasswordKey = QStringLiteral("password");
const QString LoginConfirmationMethodKey = QStringLiteral("loginconfirmationmethod");
const QString LoginConfirmationTokenKey = QStringLiteral("loginconfirmationtoken");

QString methodName(const ELoginConfirmationMethod Method)
{
    switch (Method) {
    case LoginConfirmationMethodEmailPrimary:
        return QStringLiteral("emailPrimary");
    case LoginConfirmationMethodEmailSecondary:
        return QStringLiteral("emailSecondary");
    case LoginConfirmationMethodSms:
        return QStringLiteral("sms");
    }
    return QString();
}

}

TLoginConfirmationCodeRequestUploader::TLoginConfirmationCodeRequestUploader(
    const QUrl& Url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> Sender)
    : shared::TJsonDocumentUploaderBase(std::move(Sender))
    , m_url(Url)
{
    connect(this, &shared::TJsonDocumentUploaderBase::finished, this, &TLoginConfirmationCodeRequestUploader::onFinished);
}

TLoginConfirmationCodeRequestUploader::~TLoginConfirmationCodeRequestUploader() = default;

void TLoginConfirmationCodeRequestUploader::requestLoginConfirmationCode(const QString& EMail, const QString& Password,
                                                                         const QString& Token,
                                                                         const ELoginConfirmationMethod Method)
{
    upload(m_url, createLoginConfirmationCodeRequest(EMail, Password, Token, Method));
}

QJsonDocument TLoginConfirmationCodeRequestUploader::createLoginConfirmationCodeRequest(
    const QString& EMail, const QString& Password, const QString& Token, const ELoginConfirmationMethod Method)
{
    QJsonObject request;
    request.insert(TypeKey, QJsonValue(LoginConfirmationCodeRequestType));
    request.insert(EmailKey, QJsonValue(EMail));
    request.insert(PasswordKey, QJsonValue(Password));
    request.insert(LoginConfirmationMethodKey, QJsonValue(methodName(Method)));
    request.insert(LoginConfirmationTokenKey, QJsonValue(Token));
    return QJsonDocument(request);
}

void TLoginConfirmationCodeRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument ReplyFromServerJD)
{
    if (UploadSuccessful) {
        const QJsonObject reply = ReplyFromServerJD.object();
        const bool success = reply.contains(LoginConfirmationCodeRequestSuccessfulKey)
            && reply[LoginConfirmationCodeRequestSuccessfulKey].toBool(false);
        Q_EMIT loginConfirmationCodeRequestSuccessful(success);
        return;
    }
    // Other refusals are not reported.
    switch (errorCode()) {
    case ErrorInvalidLoginConfirmationToken:
        Q_EMIT invalidLoginConfirmationToken();
        break;
    case ErrorAlreadyRequestedLoginConfirmationCodeRecently:
        Q_EMIT alreadyRequestedLoginConfirmationCodeRecently();
        break;
    case ErrorTooManyLoginConfirmationCodeRequests:
        Q_EMIT tooManyLoginConfirmationCodeRequests();
        break;
    default:
        break;
    }
}

}
