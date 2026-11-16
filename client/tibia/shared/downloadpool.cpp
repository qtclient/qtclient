#include "shared/downloadpool.h"

#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkAccessManager>
#ifndef __EMSCRIPTEN__
#include <QNetworkInterface>
#endif
#include <QRandomGenerator64>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <climits>

namespace tibia::shared {

namespace {

bool s_preferIPv4 = false;

/// CIP `TLauncherUtils::hasLocalIPv4` (cached): an IPv4 interface that reaches the internet.
bool hasLocalIPv4()
{
    static bool checked = false;
    static bool result = false;
    if (checked)
        return result;
    result = false;
#ifdef __EMSCRIPTEN__
    // qtclient: the browser picks the address family; there are no local
    // interfaces or raw sockets to probe.
    checked = true;
    return result;
#else
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol || address == QHostAddress(QHostAddress::LocalHost))
            continue;
        qInfo() << "ipv4 interface found";
        QTcpSocket dns;
        dns.connectToHost(QStringLiteral("8.8.8.8"), 53, QIODevice::ReadWrite, QAbstractSocket::AnyIPProtocol);
        bool connected = dns.waitForConnected(750);
        if (connected) {
            dns.disconnectFromHost();
        } else {
            QTcpSocket web;
            web.connectToHost(QStringLiteral("1.1.1.1"), 80, QIODevice::ReadWrite, QAbstractSocket::AnyIPProtocol);
            connected = web.waitForConnected(750);
            if (connected)
                web.disconnectFromHost();
        }
        if (connected) {
            qInfo() << "ipv4 connection succeeded";
            result = true;
        }
        break;
    }
    qInfo() << "use ipv4: " << (result ? "true" : "false");
    checked = true;
    return result;
#endif
}

}

TDownloadPool::TDownloadPool(const int maxParallelDownloads, std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                             QObject* parent)
    : cip::shared::TDownloadPool(parent)
    , m_networkAccessManager(std::move(networkAccessManager))
    , m_maxParallelDownloads(maxParallelDownloads)
{
    if (m_networkAccessManager)
        m_networkAccessManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

TDownloadPool::~TDownloadPool() = default;

void TDownloadPool::setPreferIPv4(const bool preferIPv4)
{
    s_preferIPv4 = preferIPv4;
}

QNetworkRequest TDownloadPool::createRequest(QUrl url)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(1000);
    const QString host = url.host(QUrl::FullyDecoded);
    if (host.isEmpty())
        return request;
    bool http2Allowed = true;
    if (s_preferIPv4 && hasLocalIPv4()) {
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

bool TDownloadPool::isWaitingOrActive(const QUrl& url) const
{
    const bool waiting = std::any_of(m_waitingDownloads.begin(), m_waitingDownloads.end(),
                                     [&url](const TWaitingDownload& download) { return download.url == url; });
    return waiting || m_activeDownloads.contains(url);
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
    QNetworkReply* reply = m_networkAccessManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, retries]() { onReplyFinished(reply, url, retries); });
    connect(reply, &QNetworkReply::errorOccurred, this,
            [this, reply, url, retries]() { onReplyFinished(reply, url, retries); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, url](const qint64 bytesReceived, qint64) { Q_EMIT progress(url, bytesReceived); });
    m_activeDownloads.insert(url, { reply, retries });
}

void TDownloadPool::onReplyFinished(QNetworkReply* reply, const QUrl& url, const int retries)
{
    const auto active = m_activeDownloads.constFind(url);
    if (active == m_activeDownloads.cend() || active->retries != retries)
        return;
    if (reply->error() != QNetworkReply::NoError) {
        const bool hasHost = !url.host(QUrl::FullyDecoded).isEmpty();
        if (retries > 0 && hasHost && m_activeDownloads.contains(url)) {
            const qint64 transferTimeoutMs = std::clamp<qint64>(reply->request().transferTimeoutAsDuration().count(),
                                                                 INT_MIN, INT_MAX);
            QTimer::singleShot(std::chrono::milliseconds(m_retryDelayMs), this, [this, url, retries, transferTimeoutMs]() {
                const auto pending = m_activeDownloads.constFind(url);
                if (pending == m_activeDownloads.cend() || pending->retries != retries)
                    return;
                if (m_activeDownloads.remove(url) > 0)
                    startDownload(url, retries - 1, transferTimeoutMs);
            });
            return;
        }
    }
    if (m_activeDownloads.remove(url) < 1)
        return;
    if (reply->error() == QNetworkReply::NoError)
        Q_EMIT success(url, reply->readAll());
    else
        Q_EMIT failed(url, reply->error(), reply->errorString());
    reply->deleteLater();
    startWaitingDownloads();
}

}
