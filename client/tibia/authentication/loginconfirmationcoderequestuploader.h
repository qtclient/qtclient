#pragma once

#include "authentication/authenticationprocesscontroller.h"
#include "jsondocumentuploaderbase.h"

#include <QJsonDocument>
#include <QString>
#include <QUrl>

#include <memory>

namespace tibia {
namespace authentication {

/// CIP authentication::TLoginConfirmationCodeRequestUploader: asks the login
/// service to send a new login confirmation code by the chosen method.
class TLoginConfirmationCodeRequestUploader final : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    /// The login service's error codes for a refused request.
    enum : int
    {
        ErrorTooManyLoginConfirmationCodeRequests = 13,
        ErrorAlreadyRequestedLoginConfirmationCodeRecently = 14,
        ErrorInvalidLoginConfirmationToken = 15,
    };

    TLoginConfirmationCodeRequestUploader(const QUrl& Url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> Sender);
    ~TLoginConfirmationCodeRequestUploader() override;

    /// Posts the request for the account `EMail` / `Password` and the
    /// confirmation `Token` of the refused login.
    void requestLoginConfirmationCode(const QString& EMail, const QString& Password, const QString& Token,
                                      ELoginConfirmationMethod Method);
    /// The request document: its type, the account, the method's name (empty
    /// for an unknown method) and the confirmation token.
    static QJsonDocument createLoginConfirmationCodeRequest(const QString& EMail, const QString& Password,
                                                            const QString& Token, ELoginConfirmationMethod Method);

Q_SIGNALS:
    void tooManyLoginConfirmationCodeRequests();
    void alreadyRequestedLoginConfirmationCodeRecently();
    void invalidLoginConfirmationToken();
    void loginConfirmationCodeRequestSuccessful(bool Value);

public Q_SLOTS:
    void onFinished(bool UploadSuccessful, QJsonDocument ReplyFromServerJD);

private:
    QUrl m_url;
};

}
}
