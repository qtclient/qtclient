#include "newsrequestuploader.h"

#include "news/newsstorage.h"
#include "shared/clientservicesuploader.h"

#include <QNetworkAccessManager>

namespace tibia::news {

TNewsRequestUploader::TNewsRequestUploader(TNewsStorage* storage, QObject* parent)
    : cip::tibia::news::TNewsRequestUploader(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_storage(storage)
{
    connect(this, &TNewsRequestUploader::finished, this, &TNewsRequestUploader::onFinished);
}

void TNewsRequestUploader::requestUpdate()
{
    shared::postClientService(m_network, this, m_url, QStringLiteral("news"), {}, m_revision,
                              [this](bool ok, QJsonDocument doc) { Q_EMIT finished(ok, doc); });
}

void TNewsRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    if (!UploadSuccessful) {
        Q_EMIT newsQueryFailed(tr("Could not reach the news service."));
        return;
    }
    if (!ReplyFromServerJD.isObject()) {
        Q_EMIT newsQueryFailed(tr("The news service sent an invalid response."));
        return;
    }
    m_reply = ReplyFromServerJD.object();
    if (m_reply.contains(QStringLiteral("errorCode"))) {
        Q_EMIT newsQueryFailed(m_reply.value(QStringLiteral("errorMessage")).toString());
        return;
    }
    if (m_storage)
        m_storage->setFromNewsDocument(m_reply);
    Q_EMIT newsReceived();
}

}
