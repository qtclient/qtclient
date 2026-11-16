#pragma once

#include <QAuthenticator>
#include <QByteArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace tibia {
namespace shared {

class TJsonDocumentHttpPostSender;

/// CIP `shared::TJsonDocumentUploaderBase`: uploads a JSON document through
/// a post sender; an upload succeeds when the server answers a JSON object
/// whose errorCode is 0.
class TJsonDocumentUploaderBase : public QObject
{
    Q_OBJECT

public:
    TJsonDocumentUploaderBase(std::shared_ptr<TJsonDocumentHttpPostSender> sender, QObject* parent = nullptr);
    ~TJsonDocumentUploaderBase() override;

    virtual void upload(const QUrl& url, const QJsonDocument& document);
    /// The network error, or the server's errorMessage, of the last answer.
    QString errorMessage() const;
    /// The server's errorCode of the last answer (0 for a network error).
    int errorCode() const;

Q_SIGNALS:
    void finished(bool UploadSuccessful, QJsonDocument ReplyFromServer);
    void authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);

public Q_SLOTS:
    void onFinished(QNetworkReply::NetworkError NetworkErrorCode, QString NetworkErrorString, QByteArray ReplyFromServer);

private:
    std::shared_ptr<TJsonDocumentHttpPostSender> m_sender;
    QString m_errorMessage;
    int m_errorCode = 0;
};

}
}
