#include "downloadpool.h"

#include "launcherutils.h"
#include "qnetworkaccessmanagerwrapper.h"
#include "qnetworkreplywrapper.h"

#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QRandomGenerator64>
#include <QTimer>

#include <algorithm>
#include <chrono>

namespace tibia::shared {

namespace {

bool s_preferIPv4 = false;

}

TDownloadPool::TDownloadPool(const int maxParallelDownloads,
    std::shared_ptr<QNetworkAccessManagerWrapper> networkAccessManager, QObject* parent)
    : QObject(parent)
    , m_networkAccessManager(std::move(networkAccessManager))
    , m_maxParallelDownloads(maxParallelDownloads)
{
    if (!m_networkAccessManager)
        return;
    m_networkAccessManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    if (isAuthenticationEnabled()) {
        connect(m_networkAccessManager.get(), &QNetworkAccessManagerWrapper::authenticationRequired, this,
            &TDownloadPool::onAuthenticationRequired);
    }
}

TDownloadPool::~TDownloadPool() = default;

void TDownloadPool::setPreferIPv4(const bool preferIPv4)
{
    s_preferIPv4 = preferIPv4;
}

bool TDownloadPool::isAuthenticationEnabled()
{
    return false;
}

void TDownloadPool::onAuthenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
    m_authenticationHandler.handleAuthenticationRequired(reply, authenticator);
}

QNetworkRequest TDownloadPool::createRequest(QUrl url)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(1000);
    const QString host = url.host(QUrl::FullyDecoded);
    if (host.isEmpty())
        return request;
    bool http2Allowed = true;
    if (s_preferIPv4 && TLauncherUtils::hasLocalIPv4()) {
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        const QHostInfo hostInfo = QHostInfo::fromName(host);
        QList<QHostAddress> ipv4Addresses;
        if (hostInfo.error() == QHostInfo::NoError) {
            const QList<QHostAddress> addresses = hostInfo.addresses();
            for (const QHostAddress& address : addresses) {
                if (address.protocol() == QAbstractSocket::IPv4Protocol)
                    ipv4Addresses.append(address);
            }
        }
        http2Allowed = ipv4Addresses.isEmpty();
        if (!ipv4Addresses.isEmpty()) {
            const auto index = QRandomGenerator64::global()->bounded(static_cast<quint64>(ipv4Addresses.size()));
            const QHostAddress address(ipv4Addresses[static_cast<qsizetype>(index)]);
            url.setHost(address.toString(), QUrl::StrictMode);
            request.setUrl(url);
            request.setPeerVerifyName(host);
            request.setRawHeader("Host", host.toUtf8());
        }
    }
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2Allowed);
    return request;
}

void TDownloadPool::removeWaiting(const QUrl& url)
{
    const auto it = std::find_if(m_waitingDownloads.begin(), m_waitingDownloads.end(),
        [&url](const TWaitingDownload& waiting) { return waiting.url == url; });
    if (it != m_waitingDownloads.end())
        m_waitingDownloads.erase(it);
}

bool TDownloadPool::isWaiting(const QUrl& url) const
{
    return std::any_of(m_waitingDownloads.begin(), m_waitingDownloads.end(),
        [&url](const TWaitingDownload& download) { return download.url == url; });
}

bool TDownloadPool::isActive(const QUrl& url) const
{
    return m_activeDownloads.contains(url);
}

bool TDownloadPool::isWaitingOrActive(const QUrl& url) const
{
    return isWaiting(url) || isActive(url);
}

void TDownloadPool::startWaitingDownloads()
{
    while (!m_waitingDownloads.empty() && activeDownloads() < maxParallelDownloads()) {
        const TWaitingDownload waiting = m_waitingDownloads.front();
        startDownload(waiting.url, waiting.retries, waiting.transferTimeoutMs);
        m_waitingDownloads.pop_front();
    }
}

void TDownloadPool::download(const QUrl& url, const int retries, const qint64 transferTimeoutMs)
{
    const QUrl normalized(url.toString(QUrl::FullyEncoded), QUrl::StrictMode);
    if (!normalized.isValid() || normalized.isRelative())
        Q_EMIT failed(url, QNetworkReply::ProtocolInvalidOperationError, QStringLiteral("Invalid URL"));
    removeWaiting(url);
    if (isWaitingOrActive(url))
        return;
    m_waitingDownloads.push_back({ url, retries, transferTimeoutMs });
    startWaitingDownloads();
}

void TDownloadPool::downloadWithPriority(const QUrl& url, const int retries, const qint64 transferTimeoutMs)
{
    removeWaiting(url);
    if (isWaitingOrActive(url))
        return;
    m_waitingDownloads.push_front({ url, retries, transferTimeoutMs });
    startWaitingDownloads();
}

void TDownloadPool::setMaxParallelDownloads(const int maxParallelDownloads)
{
    m_maxParallelDownloads = maxParallelDownloads;
    if (maxParallelDownloads > 0)
        startWaitingDownloads();
}

int TDownloadPool::maxParallelDownloads() const
{
    return m_maxParallelDownloads;
}

int TDownloadPool::activeDownloads() const
{
    return static_cast<int>(m_activeDownloads.size());
}

qsizetype TDownloadPool::pendingDownloads() const
{
    return m_activeDownloads.size() + static_cast<qsizetype>(m_waitingDownloads.size());
}

void TDownloadPool::setCache(QAbstractNetworkCache* cache)
{
    m_networkAccessManager->setCache(cache);
}

void TDownloadPool::startDownload(const QUrl& url, const int retries, const qint64 transferTimeoutMs)
{
    qInfo() << "StartDownload: " << url;
    QNetworkRequest request = createRequest(url);
    request.setTransferTimeout(static_cast<int>(transferTimeoutMs));
    QNetworkReplyWrapper* reply = m_networkAccessManager->get(request);
    reply->setUrl(url);
    reply->setRetries(retries);
    connect(reply, &QNetworkReplyWrapper::error, this, [this, reply] { onDownloadFinished(reply); });
    connect(reply, &QNetworkReplyWrapper::finished, this, [this, reply] { onDownloadFinished(reply); });
    connect(reply, &QNetworkReplyWrapper::progress, this,
        [this, reply](const qlonglong bytesReceived) { Q_EMIT progress(reply->url(), bytesReceived); });
    m_activeDownloads.insert(url, { reply, retries });
}

void TDownloadPool::onDownloadFinished(QNetworkReplyWrapper* reply)
{
    const QUrl url = reply->url();
    const int retries = reply->retries();
    if (!m_activeDownloads.contains(url) || m_activeDownloads[url].retries != retries)
        return;
    if (reply->replyError() != QNetworkReply::NoError) {
        const bool hasHost = !url.host(QUrl::FullyDecoded).isEmpty();
        if (retries > 0 && hasHost && m_activeDownloads.contains(url)) {
            const qint64 transferTimeoutMs = reply->transferTimeout();
            QTimer::singleShot(std::chrono::milliseconds(m_retryDelayMs), this, [this, url, retries, transferTimeoutMs] {
                if (!m_activeDownloads.contains(url) || m_activeDownloads[url].retries != retries)
                    return;
                if (m_activeDownloads.remove(url) < 1)
                    return;
                startDownload(url, retries - 1, transferTimeoutMs);
            });
            return;
        }
    }
    m_activeDownloads.remove(url);
    if (reply->replyError() == QNetworkReply::NoError)
        Q_EMIT success(url, reply->readAll());
    else
        Q_EMIT failed(url, reply->replyError(), reply->errorString());
    reply->deleteLater();
    startWaitingDownloads();
}

}
