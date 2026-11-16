#include "cloudflarecdntrace.h"

#include "host/downloadpool.h"

#include <QStringList>

namespace launcher {

TCdnTrace parseCdnTrace(const QString& text)
{
    TCdnTrace trace;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const QStringList pair = line.split(QLatin1Char('='));
        if (pair.size() != 2)
            continue;
        if (pair[0].compare(QLatin1String("fl")) == 0)
            trace.location = pair[1].trimmed();
        else if (pair[0].compare(QLatin1String("ts")) == 0)
            trace.timestamp = pair[1].trimmed();
        else if (pair[0].compare(QLatin1String("colo")) == 0)
            trace.colocation = pair[1].trimmed();
        else if (pair[0].compare(QLatin1String("ip")) == 0)
            trace.ip = pair[1].trimmed();
    }
    return trace;
}

QString formatCdnTrace(const TCdnTrace& trace)
{
    return QStringLiteral("FL: %1, TS: %2, COLO: %3, IP: %4")
        .arg(trace.location)
        .arg(trace.timestamp)
        .arg(trace.colocation)
        .arg(trace.ip);
}

TCloudflareCdnTrace::TCloudflareCdnTrace(QUrl traceUrl, std::shared_ptr<host::TDownloadPool> downloadPool, int retries,
                                         qint64 transferTimeoutMs)
    : m_traceUrl(std::move(traceUrl))
    , m_downloadPool(std::move(downloadPool))
    , m_retries(retries)
    , m_transferTimeoutMs(transferTimeoutMs)
{
    connect(m_downloadPool.get(), &host::TDownloadPool::downloadSucceeded, this,
            &TCloudflareCdnTrace::onDownloadSuccess);
    connect(m_downloadPool.get(), &host::TDownloadPool::downloadFailed, this, &TCloudflareCdnTrace::onDownloadFailed);
}

TCloudflareCdnTrace::~TCloudflareCdnTrace() = default;

void TCloudflareCdnTrace::request()
{
    if (m_received || m_inProgress)
        return;
    if (!m_traceUrl.isValid() || m_traceUrl.isEmpty()) {
        m_failed = true;
        m_inProgress = false;
        Q_EMIT diagnosticsTraceReceived(true);
        return;
    }
    m_failed = false;
    m_inProgress = true;
    m_downloadPool->download(m_traceUrl, m_retries, m_transferTimeoutMs);
}

void TCloudflareCdnTrace::onDownloadSuccess(const QUrl& url, const QByteArray& data)
{
    if (url != m_traceUrl)
        return;
    m_trace = parseCdnTrace(QString::fromUtf8(data));
    m_received = true;
    m_failed = false;
    m_inProgress = false;
    Q_EMIT diagnosticsTraceReceived(false);
}

void TCloudflareCdnTrace::onDownloadFailed(const QUrl& url, QNetworkReply::NetworkError, const QString&)
{
    if (url != m_traceUrl)
        return;
    m_received = false;
    m_failed = true;
    m_inProgress = false;
    Q_EMIT diagnosticsTraceReceived(true);
}

} // namespace launcher
