#pragma once

#include <QNetworkReply>
#include <QObject>
#include <QUrl>

#include <memory>

namespace tibia::shared {
class TDownloadPool;
}

namespace tibia {
namespace sessiondump {

class TSessiondumpCatalog;

/// CIP `sessiondump::TSessiondumpCatalogLoader`: downloads a sessiondump
/// catalog through the download pool and publishes it parsed.
class TSessiondumpCatalogLoader : public QObject
{
    Q_OBJECT
public:
    explicit TSessiondumpCatalogLoader(std::shared_ptr<shared::TDownloadPool> downloadPool);
    ~TSessiondumpCatalogLoader() override;

    /// Starts the download of the catalog at `url` (no retries, 1 s transfer timeout).
    virtual void loadCatalog(const QUrl& url);

Q_SIGNALS:
    void catalogAvailable(std::shared_ptr<tibia::sessiondump::TSessiondumpCatalog> pSessiondumpCatalog);

public Q_SLOTS:
    void onDownloadSuccess(QUrl Url, QByteArray Data);
    void onDownloadFailed(QUrl Url, QNetworkReply::NetworkError Code, QString ErrorString);

private:
    QUrl m_catalogUrl;
    std::shared_ptr<shared::TDownloadPool> m_downloadPool;
};

}
}
