#pragma once

#include "jsondocumentuploaderbase.h"
#include "news/newsstorage.h"

#include <QColor>
#include <QDate>
#include <QJsonDocument>
#include <QString>
#include <QTimeZone>
#include <QUrl>

#include <memory>
#include <vector>

namespace tibia::shared {
class IClockService;
class TTibiaFileSystemHelper;
}

namespace tibia {
namespace news {

/// One entry of the event schedule (CIP: news::TEventInformation). Dates are
/// server dates (Europe/Berlin).
struct TEventInformation
{
    QDate startDate;
    QDate endDate;
    QString name;
    QString description;
    QColor colorLight;
    QColor colorDark;
    int displayPriority = 0;
    bool isSeasonal = false;
};

/// CIP: the `eventlist` of an eventschedule reply; malformed entries are skipped.
std::vector<TEventInformation> parseEventSchedule(const QJsonDocument& reply);

/// CIP news::TEventsUploader: the event schedule from the disk cache
/// (eventschedule.json) while it is fresh, else from the client services.
class TEventsUploader final : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TEventsUploader(const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                    std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                    std::shared_ptr<shared::IClockService> clock, QObject* parent = nullptr);
    ~TEventsUploader() override;
    /// An uploader with a sender of its own.
    static std::shared_ptr<TEventsUploader> create(const QUrl& url, std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                                                   std::shared_ptr<shared::IClockService> clock);

    /// Queries `{type:"eventschedule"}`.
    void requestEventSchedule();

Q_SIGNALS:
    void updated(const std::vector<tibia::news::TEventInformation>& EventList);

public Q_SLOTS:
    void requestUpdate();
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD);

private:
    QUrl m_url;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;
    std::shared_ptr<shared::IClockService> m_clock;
    std::vector<TEventInformation> m_events;
};

/// CIP news::TEventsStorage: the last event schedule.
class TEventsStorage final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    const std::vector<TEventInformation>& events() const { return m_events; }

public Q_SLOTS:
    void onEventsUpdated(const std::vector<tibia::news::TEventInformation>& StortedEventList);

private:
    std::vector<TEventInformation> m_events;
};

}
}
