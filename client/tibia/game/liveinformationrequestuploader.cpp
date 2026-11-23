#include "liveinformationrequestuploader.h"

#include "iclockservice.h"
#include "jsonhelper.h"
#include "jsonvalidator.h"
#include "shared/tibiafilesystemhelper.h"

#include <QJsonObject>
#include <QJsonValue>

#include <limits>

namespace tibia::game {

namespace {

const QString LastUpdateTimestampKey = QStringLiteral("lastupdatetimestamp");
const QString PlayersOnlineKey = QStringLiteral("playersonline");
const QString TwitchStreamsKey = QStringLiteral("twitchstreams");
const QString TwitchViewerKey = QStringLiteral("twitchviewer");
const QString GamingYouTubeStreamsKey = QStringLiteral("gamingyoutubestreams");
const QString GamingYouTubeViewerKey = QStringLiteral("gamingyoutubeviewer");

/// The value of `key` when it is an integer, clamped to [0, INT_MAX].
void readCount(const QJsonObject& object, const QString& key, int& count)
{
    if (shared::TJsonObjectValidator(object).value(key).isInt().isValid())
        count = shared::jsonInt(object, key, 0, std::numeric_limits<int>::max(), 0);
}

}

bool isCachedDocumentOlderThan(const QJsonDocument& document, const QDateTime& now, const qint64 maximumAgeSeconds)
{
    const QJsonObject object = document.object();
    if (!shared::TJsonObjectValidator(object).value(LastUpdateTimestampKey).isInt().isValid())
        return true;
    const int lastUpdate = object[LastUpdateTimestampKey].toInt(0);
    return lastUpdate < now.toSecsSinceEpoch() - maximumAgeSeconds;
}

void stampLastUpdateTimestamp(QJsonDocument& document, const QDateTime& now)
{
    QJsonObject object = document.object();
    object.insert(LastUpdateTimestampKey, now.toSecsSinceEpoch());
    document.setObject(object);
}

TLiveInformationRequestUploader::TLiveInformationRequestUploader(
    const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
    std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper, std::shared_ptr<shared::IClockService> clock,
    QObject* parent)
    : shared::TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_fileSystemHelper(std::move(fileSystemHelper))
    , m_clock(std::move(clock))
    , m_url(url)
{
    m_updateTimer.setSingleShot(true);
    connect(this, &shared::TJsonDocumentUploaderBase::finished, this, &TLiveInformationRequestUploader::onFinished);
    connect(&m_updateTimer, &QTimer::timeout, this, &TLiveInformationRequestUploader::requestUpdate);
}

TLiveInformationRequestUploader::~TLiveInformationRequestUploader() = default;

void TLiveInformationRequestUploader::readLiveInformation(TLiveInformation& information, const QJsonDocument& document)
{
    const QJsonObject object = document.object();
    readCount(object, PlayersOnlineKey, information.playersOnline);
    readCount(object, TwitchStreamsKey, information.twitchStreams);
    readCount(object, TwitchViewerKey, information.twitchViewers);
    readCount(object, GamingYouTubeStreamsKey, information.gamingYouTubeStreams);
    readCount(object, GamingYouTubeViewerKey, information.gamingYouTubeViewers);
}

QJsonDocument TLiveInformationRequestUploader::liveInformationDocument(const TLiveInformation& information)
{
    QJsonObject object;
    object.insert(PlayersOnlineKey, information.playersOnline);
    object.insert(TwitchStreamsKey, information.twitchStreams);
    object.insert(TwitchViewerKey, information.twitchViewers);
    object.insert(GamingYouTubeStreamsKey, information.gamingYouTubeStreams);
    object.insert(GamingYouTubeViewerKey, information.gamingYouTubeViewers);
    return QJsonDocument(object);
}

void TLiveInformationRequestUploader::requestUpdate()
{
    const QJsonDocument cached = m_fileSystemHelper->readOnlineNumbers();
    if (!isCachedDocumentOlderThan(cached, m_clock->currentDateTime(), 600)) {
        readLiveInformation(m_liveInformation, cached);
        Q_EMIT newLiveInformationReceived();
        return;
    }
    QJsonObject query;
    query.insert(QStringLiteral("type"), QStringLiteral("cacheinfo"));
    upload(m_url, QJsonDocument(query));
}

void TLiveInformationRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    if (UploadSuccessful) {
        readLiveInformation(m_liveInformation, ReplyFromServerJD);
        // The cache keeps only the numbers, stamped with when they arrived.
        QJsonDocument cached = liveInformationDocument(m_liveInformation);
        stampLastUpdateTimestamp(cached, m_clock->currentDateTime());
        m_fileSystemHelper->writeOnlineNumbers(cached);
    }
    Q_EMIT newLiveInformationReceived();
}

}
