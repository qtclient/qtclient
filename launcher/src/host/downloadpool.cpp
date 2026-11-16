#include "host/downloadpool.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTimer>

namespace launcher::host {

TDownloadPool::TDownloadPool(std::shared_ptr<QNetworkAccessManager> networkAccessManager, int maxParallelDownloads,
                             QObject* parent)
    : QObject(parent)
    , m_networkAccessManager(std::move(networkAccessManager))
    , m_maxParallelDownloads(qMax(1, maxParallelDownloads))
{
}

void TDownloadPool::download(const QUrl& url, int retries, qint64 transferTimeoutMs)
{
    m_waiting.push_back({url, retries, transferTimeoutMs});
    startWaiting();
}

void TDownloadPool::cancelAll()
{
    m_waiting.clear();
    const auto replies = m_active.keys();
    m_active.clear();
    for (QNetworkReply* reply : replies) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void TDownloadPool::startWaiting()
{
    while (!m_waiting.empty() && m_active.size() < m_maxParallelDownloads) {
        const TWaiting waiting = m_waiting.front();
        m_waiting.pop_front();
        start(waiting);
    }
}

void TDownloadPool::start(const TWaiting& waiting)
{
    QNetworkRequest request(waiting.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    if (waiting.transferTimeoutMs > 0)
        request.setTransferTimeout(static_cast<int>(waiting.transferTimeoutMs));
    QNetworkReply* reply = m_networkAccessManager->get(request);
    m_active.insert(reply, waiting);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, url = waiting.url](qint64 received, qint64) {
        Q_EMIT downloadProgress(url, received);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (!m_active.contains(reply))
            return;
        const TWaiting waiting = m_active.take(reply);
        if (reply->error() == QNetworkReply::NoError) {
            Q_EMIT downloadSucceeded(waiting.url, reply->readAll());
        } else if (waiting.retries > 0) {
            TWaiting retry = waiting;
            --retry.retries;
            QTimer::singleShot(250, this, [this, retry]() {
                m_waiting.push_back(retry);
                startWaiting();
            });
        } else {
            Q_EMIT downloadFailed(waiting.url, reply->error(), reply->errorString());
        }
        startWaiting();
    });
}

} // namespace launcher::host
