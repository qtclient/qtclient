#include "clientservicesuploader.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace tibia::shared {

void postClientService(QNetworkAccessManager* network,
                       QObject* context,
                       const QUrl& url,
                       const QString& serviceType,
                       QJsonObject body,
                       const QString& clientRevision,
                       std::function<void(bool, QJsonDocument)> done)
{
    if (!network || url.isEmpty() || !done) {
        if (done)
            done(false, {});
        return;
    }

    body.insert(QStringLiteral("type"), serviceType);
    if (!clientRevision.isEmpty())
        body.insert(QStringLiteral("clientRevision"), clientRevision);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!clientRevision.isEmpty())
        request.setRawHeader("X-Tibia-Client", clientRevision.toUtf8());
    request.setTransferTimeout(30000);

    QNetworkReply* reply = network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, context, [reply, done = std::move(done)] {
        reply->deleteLater();
        QJsonParseError parseError{};
        const auto doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            done(false, {});
            return;
        }
        done(true, doc);
    });
}

}
