#pragma once

#include <cip_gen.h>

#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <list>
#include <memory>

class QAbstractNetworkCache;
class QNetworkAccessManager;

namespace tibia::shared {

/// CIP `shared::TDownloadPool`: GET downloads with at most `maxParallelDownloads`
/// in flight, a FIFO of waiting urls (prioritized ones go first) and retries
/// after a short delay for failed downloads.
class TDownloadPool final : public cip::shared::TDownloadPool
{
    Q_OBJECT
public:
    TDownloadPool(int maxParallelDownloads, std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                  QObject* parent = nullptr);
    ~TDownloadPool() override;

    /// Queues `url` (moved to the back when already queued).
    virtual void download(const QUrl& url, int retries, qint64 transferTimeoutMs);
    /// Queues `url` in front of the waiting ones.
    virtual void downloadWithPriority(const QUrl& url, int retries, qint64 transferTimeoutMs);
    virtual void setMaxParallelDownloads(int maxParallelDownloads);
    virtual int maxParallelDownloads() const;
    virtual int activeDownloads() const;
    /// Waiting plus running downloads.
    virtual qsizetype pendingDownloads() const;
    virtual void setCache(QAbstractNetworkCache* cache);

    /// qtclient: CIP's launcher switch that pins downloads to an IPv4 address.
    static void setPreferIPv4(bool preferIPv4);

Q_SIGNALS:
    void failed(const QUrl& url, QNetworkReply::NetworkError code, const QString& errorString);

private:
    struct TWaitingDownload {
        QUrl url;
        int retries = 0;
        qint64 transferTimeoutMs = 0;
    };
    struct TActiveDownload {
        QNetworkReply* reply = nullptr;
        int retries = 0;
    };

    static QNetworkRequest createRequest(QUrl url);
    void startDownload(const QUrl& url, int retries, qint64 transferTimeoutMs);
    void startWaitingDownloads();
    void removeWaiting(const QUrl& url);
    bool isWaitingOrActive(const QUrl& url) const;
    void onReplyFinished(QNetworkReply* reply, const QUrl& url, int retries);

    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
    QMap<QUrl, TActiveDownload> m_activeDownloads;
    int m_maxParallelDownloads = 0;
    std::list<TWaitingDownload> m_waitingDownloads;
    qint64 m_retryDelayMs = 250;
};

}
