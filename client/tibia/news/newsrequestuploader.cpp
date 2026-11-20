#include "newsrequestuploader.h"

#include "iclockservice.h"
#include "news/newsstorage.h"
#include "shared/tibiafilesystemhelper.h"

#include <QDebug>
#include <QJsonObject>

#include <exception>

namespace tibia::news {

QJsonDocument newsQueryDocument(const TNewsQueryParameters& parameters)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("news"));
    object.insert(QStringLiteral("fromtimestamp"), static_cast<qint64>(parameters.maxKnownEditDate));
    object.insert(QStringLiteral("isreturner"), parameters.returner);
    object.insert(QStringLiteral("showrewardnews"), parameters.highPotential);
    return QJsonDocument(object);
}

QJsonDocument newsViewedDocument(const uint id)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("newsviewed"));
    object.insert(QStringLiteral("viewedid"), static_cast<qint64>(id));
    return QJsonDocument(object);
}

TNewsRequestUploader::TNewsRequestUploader(const QUrl& url, std::shared_ptr<TNewsStorage> storage,
                                           std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                                           std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                                           std::shared_ptr<shared::IClockService> clock, QObject* parent)
    : shared::TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_fileSystemHelper(std::move(fileSystemHelper))
    , m_clock(std::move(clock))
    , m_url(url)
    , m_storage(std::move(storage))
{
    connect(this, &shared::TJsonDocumentUploaderBase::finished, this, &TNewsRequestUploader::onFinished);
    // A changed returner state asks for every entry again.
    connect(m_storage.get(), &TNewsStorage::returnerInformationChanged, this,
            [this] { requestNews(storageParameters(), true); });
}

TNewsRequestUploader::~TNewsRequestUploader() = default;

TNewsQueryParameters TNewsRequestUploader::storageParameters() const
{
    return { m_storage->isReturner(), m_storage->showRewardNews(), static_cast<uint>(m_storage->version()) };
}

void TNewsRequestUploader::requestNews(const TNewsQueryParameters& parameters, const bool force)
{
    TNewsQueryParameters query = parameters;
    m_retries.try_emplace(query, 0u);
    if (force)
        query.maxKnownEditDate = 0;
    uint& tries = m_retries.try_emplace(query, 0u).first->second;
    if (tries < 3 || force) {
        ++tries;
        m_pending = true;
        upload(m_url, newsQueryDocument(query));
        return;
    }
    qWarning() << "Didn't send news query with params (HighPotential:" << (query.highPotential ? "true" : "false")
               << "Returner:" << (query.returner ? "true" : "false") << "MaxKnownEditDate:" << query.maxKnownEditDate
               << ") (URL:" << m_url << ") due to too many retries.";
}

void TNewsRequestUploader::requestUpdate()
{
    if (isCachedDocumentOutdated(m_fileSystemHelper->readCompendium(), m_clock->currentDateTime()))
        requestNews(storageParameters(), false);
}

void TNewsRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    // Several uploaders may share one TJsonDocumentHttpPostSender; ignore
    // replies that are not ours (e.g. newsviewed / eventschedule fan-out).
    if (!m_pending)
        return;
    const QJsonObject root = ReplyFromServerJD.object();
    const bool newsShaped = root.contains(QStringLiteral("gamenews")) || root.contains(QStringLiteral("maxeditdate"))
        || root.contains(QStringLiteral("categorycounts"));
    if (UploadSuccessful && !newsShaped)
        return;
    m_pending = false;
    if (!UploadSuccessful) {
        Q_EMIT newsQueryFailed(errorMessage());
        return;
    }
    try {
        TNewsData data = parseNewsData(ReplyFromServerJD);
        data.lastUpdateTimestamp = m_clock->currentDateTime().toSecsSinceEpoch();
        // A new edit date starts the retry counts over.
        if (m_storage->version() != data.maxEditDate)
            m_retries.clear();
        m_storage->mergeNewsData(data);
    } catch (const std::exception& e) {
        Q_EMIT newsQueryFailed(QString::fromUtf8(e.what()));
    }
}

TNewsViewsUploader::TNewsViewsUploader(const QUrl& url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                                       QObject* parent)
    : shared::TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_url(url)
{}

TNewsViewsUploader::~TNewsViewsUploader() = default;

void TNewsViewsUploader::reportNewsViewed(const uint id) { upload(m_url, newsViewedDocument(id)); }

}
