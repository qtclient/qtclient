#include "eventsuploader.h"

#include "shared/clientservicesuploader.h"

#include <QNetworkAccessManager>

namespace tibia::news {

TEventsUploader::TEventsUploader(QObject* parent)
    : cip::tibia::news::TEventsUploader(parent)
    , m_network(new QNetworkAccessManager(this))
{
    connect(this, &TEventsUploader::finished, this, &TEventsUploader::onFinished);
}

void TEventsUploader::requestUpdate()
{
    shared::postClientService(m_network, this, m_url, QStringLiteral("eventschedule"), {}, m_revision,
                              [this](bool ok, QJsonDocument doc) { Q_EMIT finished(ok, doc); });
}

void TEventsUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    if (!UploadSuccessful || !ReplyFromServerJD.isObject())
        return;
    const QJsonObject reply = ReplyFromServerJD.object();
    m_lastUpdateTimestamp = reply.value(QStringLiteral("lastupdatetimestamp")).toInteger();
    m_eventList = reply.value(QStringLiteral("eventlist")).toArray();
    Q_EMIT scheduleUpdated();
}

}
