#pragma once

#include "jsondocumentuploaderbase.h"

#include <QJsonDocument>
#include <QString>
#include <QUrl>

#include <memory>

namespace tibia {
namespace authentication {

/// CIP authentication::TEmailCodeRequestUploader: asks the login service to
/// send a new two-factor email login code to the account's address.
class TEmailCodeRequestUploader final : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    /// The login service's error codes for a refused request.
    enum : int
    {
        ErrorTooManyEmailCodeRequests = 9,
        ErrorAlreadyRequestedEMailCodeRecently = 10,
    };

    TEmailCodeRequestUploader(const QUrl& Url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> Sender);
    ~TEmailCodeRequestUploader() override;

    /// Posts the request for the account `EMail` / `Password`.
    void requestEmailCode(const QString& EMail, const QString& Password);
    /// The request document: its type, the email address and the password.
    static QJsonDocument createEmailCodeRequest(const QString& EMail, const QString& Password);

Q_SIGNALS:
    void emailCodeRequestCompleted(bool Success);
    void tooManyEmailCodeRequests();
    void alreadyRequestedEMailCodeRecently();

public Q_SLOTS:
    void onFinished(bool UploadSuccessful, QJsonDocument ReplyFromServerJD);

private:
    QUrl m_url;
};

}
}
