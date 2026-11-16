#pragma once

#include <QAuthenticator>
#include <QByteArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>
#include <optional>
#include <string>

namespace tibia {
namespace shared {

/// CIP `shared::TJsonDocumentHttpPostSender`: POSTs a JSON document and
/// reports the reply, or Cloudflare's challenge when the request was
/// mitigated.
class TJsonDocumentHttpPostSender : public QObject
{
    Q_OBJECT

public:
    /// The clearance a solved Cloudflare challenge grants: the cf_clearance
    /// cookie and the user agent that solved it.
    struct TCloudflareClearance
    {
        QNetworkCookie cookie;
        QString userAgent;
    };

    explicit TJsonDocumentHttpPostSender(QObject* parent = nullptr);
    ~TJsonDocumentHttpPostSender() override;

    /// POSTs `document` (compact JSON) to `url`.
    virtual void send(const QUrl& url, const QJsonDocument& document);
    void setCloudflareClearance(const TCloudflareClearance& clearance);
    bool hasCloudflareClearance() const;
    void resetCloudflareClearance();

Q_SIGNALS:
    void finished(QNetworkReply::NetworkError NetworkErrorCode, QString NetworkErrorString, QByteArray ReplyFromServer);
    void authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);
    void cloudflareMitigated(QNetworkReply* reply, std::string mitigatedHeaderValue);

public Q_SLOTS:
    void onFinished(QNetworkReply* Reply);

private:
    /// The cf-mitigated header of a 403 reply.
    static std::optional<std::string> cloudflareMitigation(QNetworkReply* reply);
    /// Sends the clearance cookie, or clears a stale one, then posts.
    void post(const QNetworkRequest& request, const QByteArray& body);

    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
    std::optional<TCloudflareClearance> m_cloudflareClearance;
};

}
}
