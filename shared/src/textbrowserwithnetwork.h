#pragma once

#include <QByteArray>
#include <QMap>
#include <QNetworkReply>
#include <QSize>
#include <QTextBrowser>
#include <QUrl>
#include <QVariant>

#include <memory>

namespace tibia {
namespace shared {

class TDownloadPool;

/// CIP `shared::QTextBrowserWithNetwork`: a text browser that fetches the
/// http(s)/ftp resources of its HTML through a download pool, shows a
/// transparent placeholder image until one arrives, then lays the HTML out
/// again. It is as high as its document.
class QTextBrowserWithNetwork : public QTextBrowser
{
    Q_OBJECT

public:
    QTextBrowserWithNetwork(std::shared_ptr<TDownloadPool> downloadPool, int retries, qint64 transferTimeoutMs,
        QWidget* parent = nullptr);
    ~QTextBrowserWithNetwork() override;

    QSize sizeHint() const override;
    int heightForWidth(int width) const override;
    QVariant loadResource(int type, const QUrl& name) override;

public Q_SLOTS:
    void onDownloadFinished(QUrl Url, QByteArray Data);
    void onDownloadFailed(QUrl Url, QNetworkReply::NetworkError Code, QString ErrorString);

private:
    std::shared_ptr<TDownloadPool> m_downloadPool;
    int m_retries = 0;
    qint64 m_transferTimeoutMs = 0;
    QMap<QUrl, QVariant> m_resources;
};

}
}
