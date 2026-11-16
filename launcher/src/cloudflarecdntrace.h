#pragma once

#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace launcher {

namespace host {
class TDownloadPool;
}

/// The fields of a Cloudflare `/cdn-cgi/trace` answer the launcher shows.
struct TCdnTrace {
    QString location;
    QString timestamp;
    QString colocation;
    QString ip;
};

/// Parses `key=value` lines of a Cloudflare trace (fl, ts, colo, ip).
TCdnTrace parseCdnTrace(const QString& text);
/// "FL: %1, TS: %2, COLO: %3, IP: %4".
QString formatCdnTrace(const TCdnTrace& trace);

/// Fetches the CDN diagnostics trace shown when a download fails.
class TCloudflareCdnTrace : public QObject
{
    Q_OBJECT
public:
    TCloudflareCdnTrace(QUrl traceUrl, std::shared_ptr<host::TDownloadPool> downloadPool, int retries,
                        qint64 transferTimeoutMs);
    ~TCloudflareCdnTrace() override;

    /// Starts the request unless it is running or done.
    void request();
    bool received() const { return m_received; }
    bool failed() const { return m_failed; }
    bool inProgress() const { return m_inProgress; }
    const TCdnTrace& trace() const { return m_trace; }

Q_SIGNALS:
    void diagnosticsTraceReceived(bool failed);

public Q_SLOTS:
    void onDownloadSuccess(const QUrl& url, const QByteArray& data);
    void onDownloadFailed(const QUrl& url, QNetworkReply::NetworkError code, const QString& errorString);

private:
    QUrl m_traceUrl;
    std::shared_ptr<host::TDownloadPool> m_downloadPool;
    int m_retries = 0;
    qint64 m_transferTimeoutMs = 0;
    TCdnTrace m_trace;
    bool m_received = false;
    bool m_failed = false;
    bool m_inProgress = false;
};

} // namespace launcher
