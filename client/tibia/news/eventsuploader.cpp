#include "eventsuploader.h"

#include "iclockservice.h"
#include "news/newsstorage.h"
#include "shared/tibiafilesystemhelper.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <exception>
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

TEventsUploader::TEventsUploader(const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                                 std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                                 std::shared_ptr<shared::IClockService> clock, QObject* parent)
    : shared::TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_url(url)
    , m_fileSystemHelper(std::move(fileSystemHelper))
    , m_clock(std::move(clock))
{
    connect(this, &shared::TJsonDocumentUploaderBase::finished, this, &TEventsUploader::onFinished);
}

TEventsUploader::~TEventsUploader() = default;

void TEventsUploader::requestUpdate()
{
    const QJsonDocument cached = m_fileSystemHelper->readEventSchedule();
    if (isCachedDocumentOutdated(cached, m_clock->currentDateTime())) {
        requestEventSchedule();
        return;
    }
    m_events = parseEventSchedule(cached);
    Q_EMIT updated(m_events);
}

void TEventsUploader::requestEventSchedule()
{
    QJsonObject query;
    query.insert(QStringLiteral("type"), QStringLiteral("eventschedule"));
    upload(m_url, QJsonDocument(query));
}

void TEventsUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    try {
        if (UploadSuccessful) {
            // Ignore non-schedule replies if a shared sender ever returns.
            if (!ReplyFromServerJD.object().contains(QStringLiteral("eventlist"))
                && !ReplyFromServerJD.object().contains(QStringLiteral("lastupdatetimestamp")))
                return;
            m_events = parseEventSchedule(ReplyFromServerJD);
            // The cache remembers when it was fetched.
            QJsonObject stamped = ReplyFromServerJD.object();
            stamped.insert(QStringLiteral("lastupdatetimestamp"), m_clock->currentDateTime().toSecsSinceEpoch());
            m_fileSystemHelper->writeEventSchedule(QJsonDocument(stamped));
        }
        Q_EMIT updated(m_events);
    } catch (const std::exception& e) {
        qWarning() << "Event schedule update failed:" << e.what();
    }
}

void TEventsStorage::onEventsUpdated(const std::vector<TEventInformation>& StortedEventList)
{
    if (&m_events != &StortedEventList)
        m_events = StortedEventList;
}

}
