#include "boostedcreaturerequestuploader.h"

#include "shared/clientservicesuploader.h"

#include <QNetworkAccessManager>

namespace tibia::game {

TBoostedCreatureRequestUploader::TBoostedCreatureRequestUploader(QObject* parent)
    : cip::tibia::game::TBoostedCreatureRequestUploader(parent)
    , m_network(new QNetworkAccessManager(this))
{
    connect(this, &TBoostedCreatureRequestUploader::finished, this, &TBoostedCreatureRequestUploader::onFinished);
}

void TBoostedCreatureRequestUploader::requestUpdate()
{
    shared::postClientService(m_network, this, m_url, QStringLiteral("boostedcreature"), {}, m_revision,
                              [this](bool ok, QJsonDocument doc) { Q_EMIT finished(ok, doc); });
}

void TBoostedCreatureRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    if (!UploadSuccessful || !ReplyFromServerJD.isObject())
        return;
    m_reply = ReplyFromServerJD.object();
    Q_EMIT boostedCreatureReceived();
}

}
