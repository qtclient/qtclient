#pragma once

#include "networkaccessmanagerauthenticationhandler.h"

#include <QAbstractNetworkCache>
#include <QByteArray>
#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QUrl>

#include <list>
#include <memory>

namespace tibia {
namespace shared {

class QNetworkAccessManagerWrapper;
class QNetworkReplyWrapper;

/// CIP `shared::TDownloadPool`: GET downloads with at most
/// `maxParallelDownloads` in flight, a FIFO of waiting urls (prioritized ones
/// go first) and retries after a short delay for failed downloads.
class TDownloadPool : public QObject
{
    Q_OBJECT

public:
    TDownloadPool(int maxParallelDownloads, std::shared_ptr<QNetworkAccessManagerWrapper> networkAccessManager,
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

    /// Pins downloads to one of the host's IPv4 addresses when the machine
    /// reaches the internet over IPv4 (the launcher's switch).
    static void setPreferIPv4(bool preferIPv4);
    /// `url` with a `nocache=<32 random alphanumerics>` query item (invalid urls unchanged).
    static QUrl appendNocache(QUrl url);

Q_SIGNALS:
    void success(QUrl url, QByteArray data);
    void failed(QUrl url, QNetworkReply::NetworkError code, QString errorString);
    void progress(QUrl Url, qlonglong BytesReceivedFromRequest);

private:
    struct TWaitingDownload
    {
        QUrl url;
        int retries = 0;
        qint64 transferTimeoutMs = 0;
    };
    struct TActiveDownload
    {
        QNetworkReplyWrapper* reply = nullptr;
        int retries = 0;
    };

    /// CipSoft compiles network authentication out: false.
    static bool isAuthenticationEnabled();
    static QNetworkRequest createRequest(QUrl url);
    void startDownload(const QUrl& url, int retries, qint64 transferTimeoutMs);
    void startWaitingDownloads();
    void removeWaiting(const QUrl& url);
    bool isWaiting(const QUrl& url) const;
    bool isActive(const QUrl& url) const;
    bool isWaitingOrActive(const QUrl& url) const;
    void onDownloadFinished(QNetworkReplyWrapper* reply);
    void onAuthenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);

    std::shared_ptr<QNetworkAccessManagerWrapper> m_networkAccessManager;
    QMap<QUrl, TActiveDownload> m_activeDownloads;
    int m_maxParallelDownloads = 0;
    std::list<TWaitingDownload> m_waitingDownloads;
    TNetworkAccessManagerAuthenticationHandler m_authenticationHandler;
    qint64 m_retryDelayMs = 250;
};

}
}
