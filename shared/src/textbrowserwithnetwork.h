#pragma once

#include <QByteArray>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QTextBrowser>
#include <QUrl>

namespace shared {

// CIP contract: shared::QTextBrowserWithNetwork (upstream/contract/classes/shared.json).
// QTextBrowser alone cannot resolve remote http(s) images in setHtml(); CIP's
// subclass fetches them via QNetworkAccessManager and injects the bytes through
// loadResource. Used by the launcher hint feed.
class QTextBrowserWithNetwork : public QTextBrowser
{
    Q_OBJECT

public:
    explicit QTextBrowserWithNetwork(QWidget* parent = nullptr);

    QVariant loadResource(int type, const QUrl& name) override;

public Q_SLOTS:
    void onDownloadFinished(QUrl url, QByteArray data);
    void onDownloadFailed(QUrl url, QNetworkReply::NetworkError code, QString errorString);

private:
    void request(const QUrl& url);

    QNetworkAccessManager m_nam;
    QHash<QUrl, QByteArray> m_cache;
    QSet<QUrl> m_pending;
};

} // namespace shared
