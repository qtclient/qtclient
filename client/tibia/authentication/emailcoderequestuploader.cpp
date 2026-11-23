#include "authentication/emailcoderequestuploader.h"

#include <QJsonObject>
#include <QJsonValue>

namespace tibia::authentication {

namespace {

const QString TypeKey = QStringLiteral("type");
const QString EmailCodeRequestSuccessfulKey = QStringLiteral("emailcoderequestsuccessful");
const QString EmailCodeRequestType = QStringLiteral("emailcoderequest");
const QString EmailKey = QStringLiteral("email");
const QString PasswordKey = QStringLiteral("password");

}

TEmailCodeRequestUploader::TEmailCodeRequestUploader(const QUrl& Url,
                                                     std::shared_ptr<shared::TJsonDocumentHttpPostSender> Sender)
    : shared::TJsonDocumentUploaderBase(std::move(Sender))
    , m_url(Url)
{
    connect(this, &shared::TJsonDocumentUploaderBase::finished, this, &TEmailCodeRequestUploader::onFinished);
}

TEmailCodeRequestUploader::~TEmailCodeRequestUploader() = default;

void TEmailCodeRequestUploader::requestEmailCode(const QString& EMail, const QString& Password)
{
    upload(m_url, createEmailCodeRequest(EMail, Password));
}

QJsonDocument TEmailCodeRequestUploader::createEmailCodeRequest(const QString& EMail, const QString& Password)
{
    QJsonObject request;
    request.insert(TypeKey, QJsonValue(EmailCodeRequestType));
    request.insert(EmailKey, QJsonValue(EMail));
    request.insert(PasswordKey, QJsonValue(Password));
    return QJsonDocument(request);
}

void TEmailCodeRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument ReplyFromServerJD)
{
    if (UploadSuccessful) {
        const QJsonObject reply = ReplyFromServerJD.object();
        const bool success = reply.contains(EmailCodeRequestSuccessfulKey) && reply[EmailCodeRequestSuccessfulKey].toBool(false);
        Q_EMIT emailCodeRequestCompleted(success);
        return;
    }
    if (errorCode() == ErrorTooManyEmailCodeRequests) {
        Q_EMIT tooManyEmailCodeRequests();
        return;
    }
    if (errorCode() == ErrorAlreadyRequestedEMailCodeRecently) {
        Q_EMIT alreadyRequestedEMailCodeRecently();
        return;
    }
    Q_EMIT emailCodeRequestCompleted(false);
}

}
