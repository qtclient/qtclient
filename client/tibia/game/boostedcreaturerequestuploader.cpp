#include "boostedcreaturerequestuploader.h"

#include "game/liveinformationrequestuploader.h"
#include "iclockservice.h"
#include "jsonhelper.h"
#include "jsonvalidator.h"
#include "news/newsstorage.h"
#include "shared/tibiafilesystemhelper.h"

#include <QJsonObject>

#include <limits>

namespace tibia::game {

namespace {

const QString CreatureRaceIdKey = QStringLiteral("creatureraceid");
const QString BossRaceIdKey = QStringLiteral("bossraceid");

void readRaceId(const QJsonObject& object, const QString& key, int& raceId)
{
    if (shared::TJsonObjectValidator(object).value(key).isInt().isValid())
        raceId = shared::jsonInt(object, key, 0, std::numeric_limits<int>::max(), 0);
}

}

TBoostedCreatureRequestUploader::TBoostedCreatureRequestUploader(
    const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
    std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper, std::shared_ptr<shared::IClockService> clock,
    QObject* parent)
    : shared::TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_fileSystemHelper(std::move(fileSystemHelper))
    , m_clock(std::move(clock))
    , m_url(url)
{
    connect(this, &shared::TJsonDocumentUploaderBase::finished, this, &TBoostedCreatureRequestUploader::onFinished);
}

TBoostedCreatureRequestUploader::~TBoostedCreatureRequestUploader() = default;

void TBoostedCreatureRequestUploader::readBoostedCreature(TBoostedCreatureInformation& information,
                                                          const QJsonDocument& document)
{
    const QJsonObject object = document.object();
    readRaceId(object, CreatureRaceIdKey, information.creatureRaceId);
    readRaceId(object, BossRaceIdKey, information.bossRaceId);
}

QJsonDocument TBoostedCreatureRequestUploader::boostedCreatureDocument(const TBoostedCreatureInformation& information)
{
    QJsonObject object;
    object.insert(CreatureRaceIdKey, information.creatureRaceId);
    object.insert(BossRaceIdKey, information.bossRaceId);
    return QJsonDocument(object);
}

void TBoostedCreatureRequestUploader::requestUpdate()
{
    const QJsonDocument cached = m_fileSystemHelper->readBoostedCreature();
    if (!news::isCachedDocumentOutdated(cached, m_clock->currentDateTime())) {
        readBoostedCreature(m_boostedCreature, cached);
        Q_EMIT boostedCreatureReceived();
        return;
    }
    QJsonObject query;
    query.insert(QStringLiteral("type"), QStringLiteral("boostedcreature"));
    upload(m_url, QJsonDocument(query));
}

void TBoostedCreatureRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    if (UploadSuccessful) {
        readBoostedCreature(m_boostedCreature, ReplyFromServerJD);
        // The cache keeps only the race ids, stamped with when they arrived.
        QJsonDocument cached = boostedCreatureDocument(m_boostedCreature);
        stampLastUpdateTimestamp(cached, m_clock->currentDateTime());
        m_fileSystemHelper->writeBoostedCreature(cached);
    }
    Q_EMIT boostedCreatureReceived();
}

}
