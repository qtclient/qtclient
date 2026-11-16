#pragma once

#include <QByteArray>
#include <QHash>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>

#include <deque>
#include <memory>

class QNetworkAccessManager;

namespace launcher::host {

/// Download pool used by the launcher classes until CipSoft's
/// `shared::TDownloadPool` lands under apps/qtclient/shared/src (net slice).
/// Same contract as CipSoft's pool as the launcher uses it: GETs with at most
/// `maxParallelDownloads` in flight, retries, a transfer timeout, and
/// succeeded / failed / progress signals keyed by url.
class TDownloadPool final : public QObject
{
    Q_OBJECT
public:
    TDownloadPool(std::shared_ptr<QNetworkAccessManager> networkAccessManager, int maxParallelDownloads,
                  QObject* parent = nullptr);

    void download(const QUrl& url, int retries, qint64 transferTimeoutMs);
    void cancelAll();

Q_SIGNALS:
    void downloadSucceeded(const QUrl& url, const QByteArray& data);
    void downloadFailed(const QUrl& url, QNetworkReply::NetworkError code, const QString& errorString);
    void downloadProgress(const QUrl& url, qint64 downloadedBytes);

private:
    struct TWaiting {
        QUrl url;
        int retries = 0;
        qint64 transferTimeoutMs = 0;
    };
    void startWaiting();
    void start(const TWaiting& waiting);

    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
    int m_maxParallelDownloads = 1;
    std::deque<TWaiting> m_waiting;
    QHash<QNetworkReply*, TWaiting> m_active;
};

} // namespace launcher::host
