#include "jsondocumenthttppostsender.h"

#include <QNetworkCookieJar>
#include <QNetworkRequest>
#include <QVariant>

namespace tibia::shared {

TJsonDocumentHttpPostSender::TJsonDocumentHttpPostSender(QObject* parent)
    : QObject(parent)
{
    m_networkAccessManager = std::make_shared<QNetworkAccessManager>();
    connect(m_networkAccessManager.get(), &QNetworkAccessManager::finished, this, &TJsonDocumentHttpPostSender::onFinished);
    connect(m_networkAccessManager.get(), &QNetworkAccessManager::authenticationRequired, this,
        &TJsonDocumentHttpPostSender::authenticationRequired);
}

TJsonDocumentHttpPostSender::~TJsonDocumentHttpPostSender() = default;

void TJsonDocumentHttpPostSender::send(const QUrl& url, const QJsonDocument& document)
{
    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QString::fromUtf8("application/json")));
    request.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QString::fromUtf8("form-data; name=\"metadata\"")));
    if (m_cloudflareClearance)
        request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(m_cloudflareClearance->userAgent));
    post(request, document.toJson(QJsonDocument::Compact));
}

void TJsonDocumentHttpPostSender::post(const QNetworkRequest& request, const QByteArray& body)
{
    if (!m_cloudflareClearance) {
        const QNetworkCookie staleClearance(QByteArray("cf_clearance"), QByteArray());
        m_networkAccessManager->cookieJar()->deleteCookie(staleClearance);
    } else {
        m_networkAccessManager->cookieJar()->insertCookie(m_cloudflareClearance->cookie);
    }
    m_networkAccessManager->post(request, body);
}

std::optional<std::string> TJsonDocumentHttpPostSender::cloudflareMitigation(QNetworkReply* reply)
{
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool mitigated = reply->hasRawHeader("cf-mitigated");
    QByteArray mitigatedHeaderValue;
    if (mitigated)
        mitigatedHeaderValue = reply->rawHeader("cf-mitigated");
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    Q_UNUSED(contentType);
    if (mitigated && statusCode == 403)
        return mitigatedHeaderValue.toStdString();
    return std::nullopt;
}

void TJsonDocumentHttpPostSender::onFinished(QNetworkReply* Reply)
{
    const std::optional<std::string> mitigation = cloudflareMitigation(Reply);
    if (mitigation && *mitigation == "challenge")
        Q_EMIT cloudflareMitigated(Reply, *mitigation);
    else
        Q_EMIT finished(Reply->error(), Reply->errorString(), Reply->readAll());
    Reply->deleteLater();
}

void TJsonDocumentHttpPostSender::setCloudflareClearance(const TCloudflareClearance& clearance)
{
    m_cloudflareClearance = clearance;
}

bool TJsonDocumentHttpPostSender::hasCloudflareClearance() const
{
    return m_cloudflareClearance.has_value();
}

void TJsonDocumentHttpPostSender::resetCloudflareClearance()
{
    m_cloudflareClearance.reset();
}

}
