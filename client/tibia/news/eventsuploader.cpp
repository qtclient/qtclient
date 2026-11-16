#include "eventsuploader.h"

#include "shared/clientservicesuploader.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkAccessManager>

#include <algorithm>
#include <limits>

namespace tibia::news {

namespace {

/// CIP: an integer clamped to [0, INT_MAX].
int boundedInt(const QJsonObject& object, const QString& key)
{
    const qint64 value = object.value(key).toInteger();
    return static_cast<int>(std::clamp<qint64>(value, 0, std::numeric_limits<int>::max()));
}

QDate serverDate(const int secondsSinceEpoch)
{
    return QDateTime::fromSecsSinceEpoch(secondsSinceEpoch, QTimeZone::UTC).toTimeZone(serverTimeZone()).date();
}

}

QTimeZone serverTimeZone()
{
    static const QTimeZone zone = [] {
#if QT_CONFIG(timezone)
        QTimeZone berlin(QByteArrayLiteral("Europe/Berlin"));
        // qtclient: a build may lack the zone database.
        if (berlin.isValid())
            return berlin;
#endif
        // qtclient: wasm Qt has no named zones; Berlin's standard time.
        return QTimeZone::fromSecondsAheadOfUtc(3600);
    }();
    return zone;
}

std::vector<TEventInformation> parseEventSchedule(const QJsonDocument& reply)
{
    std::vector<TEventInformation> events;
    const QJsonObject root = reply.object();
    if (!root.value(QStringLiteral("eventlist")).isArray())
        return events;
    const QJsonArray list = root.value(QStringLiteral("eventlist")).toArray();
    events.reserve(static_cast<size_t>(list.size()));
    for (const QJsonValue& value : list) {
        const QJsonObject object = value.toObject();
        const bool valid = object.value(QStringLiteral("startdate")).isDouble()
            && object.value(QStringLiteral("enddate")).isDouble() && object.value(QStringLiteral("name")).isString()
            && object.value(QStringLiteral("description")).isString()
            && object.value(QStringLiteral("colorlight")).isString()
            && object.value(QStringLiteral("colordark")).isString()
            && object.value(QStringLiteral("displaypriority")).isDouble()
            && object.value(QStringLiteral("isseasonal")).isBool();
        if (!valid)
            continue;
        TEventInformation event;
        event.startDate = serverDate(boundedInt(object, QStringLiteral("startdate")));
        event.endDate = serverDate(boundedInt(object, QStringLiteral("enddate")));
        event.name = object.value(QStringLiteral("name")).toString();
        event.description = object.value(QStringLiteral("description")).toString();
        event.colorLight = QColor::fromString(object.value(QStringLiteral("colorlight")).toString());
        event.colorDark = QColor::fromString(object.value(QStringLiteral("colordark")).toString());
        event.displayPriority = boundedInt(object, QStringLiteral("displaypriority"));
        event.isSeasonal = object.value(QStringLiteral("isseasonal")).toBool();
        events.push_back(std::move(event));
    }
    return events;
}

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
    m_events = parseEventSchedule(ReplyFromServerJD);
    Q_EMIT scheduleUpdated();
}

}
