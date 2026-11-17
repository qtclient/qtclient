#include "sessiondump/sessiondumpcatalogloader.h"

#include "downloadpool.h"
#include "sessiondump/sessiondumpcatalog.h"

#include <QDebug>
#include <QJsonDocument>

namespace tibia::sessiondump {

TSessiondumpCatalogLoader::TSessiondumpCatalogLoader(std::shared_ptr<shared::TDownloadPool> downloadPool)
    : QObject(nullptr)
    , m_downloadPool(std::move(downloadPool))
{
    if (!m_downloadPool)
        return;
    connect(m_downloadPool.get(), &shared::TDownloadPool::success, this, &TSessiondumpCatalogLoader::onDownloadSuccess);
    connect(m_downloadPool.get(), &shared::TDownloadPool::failed, this, &TSessiondumpCatalogLoader::onDownloadFailed);
}

TSessiondumpCatalogLoader::~TSessiondumpCatalogLoader() = default;

void TSessiondumpCatalogLoader::loadCatalog(const QUrl& url)
{
    m_catalogUrl = url;
    m_downloadPool->download(url, 0, 1000);
}

void TSessiondumpCatalogLoader::onDownloadSuccess(QUrl Url, QByteArray Data)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(Data, &error);
    if (error.error != QJsonParseError::NoError) {
        qCritical() << QStringLiteral("Sessiondump catalog '%1' could not be parsed to json").arg(Url.toString());
        return;
    }
    Q_EMIT catalogAvailable(std::make_shared<TSessiondumpCatalog>(document));
}

void TSessiondumpCatalogLoader::onDownloadFailed(QUrl Url, QNetworkReply::NetworkError, QString ErrorString)
{
    qCritical() << QStringLiteral("Sessiondump catalog '%1' could not be downloaded: %2").arg(Url.toString(), ErrorString);
}

}
