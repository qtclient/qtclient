#include "liveinformationrequestuploader.h"

#include "shared/clientservicesuploader.h"

#include <QNetworkAccessManager>

namespace tibia::game {

TLiveInformationRequestUploader::TLiveInformationRequestUploader(QObject* parent)
    : cip::tibia::game::TLiveInformationRequestUploader(parent)
    , m_network(new QNetworkAccessManager(this))
{
    connect(this, &TLiveInformationRequestUploader::finished, this, &TLiveInformationRequestUploader::onFinished);
}

void TLiveInformationRequestUploader::requestUpdate()
{
    shared::postClientService(m_network, this, m_url, QStringLiteral("cacheinfo"), {}, m_revision,
                              [this](bool ok, QJsonDocument doc) { Q_EMIT finished(ok, doc); });
}

void TLiveInformationRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    if (!UploadSuccessful || !ReplyFromServerJD.isObject())
        return;
    m_reply = ReplyFromServerJD.object();
    Q_EMIT newLiveInformationReceived();
}

}
