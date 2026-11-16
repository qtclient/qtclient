#pragma once

#include <QByteArray>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>
#include <QVariant>

namespace tibia {
namespace shared {

/// CIP `shared::QNetworkReplyWrapper`: a QNetworkReply behind virtual
/// accessors, forwarding the reply's error, finish and download progress.
/// It is a child of the reply.
class QNetworkReplyWrapper : public QObject
{
    Q_OBJECT

public:
    explicit QNetworkReplyWrapper(QNetworkReply* reply);
    ~QNetworkReplyWrapper() override = default;

    virtual QNetworkReply::NetworkError replyError() const;
    virtual QString errorString() const;
    virtual QByteArray readAll();
    /// The overriding URL if set, else the URL the reply was requested for.
    virtual QUrl url() const;
    QVariant attribute(QNetworkRequest::Attribute code) const;
    bool hasRawHeader(const QByteArray& headerName) const;
    QByteArray rawHeader(const QByteArray& headerName) const;
    /// Overrides the URL url() reports.
    void setUrl(const QUrl& url);
    /// The retries left for the download this reply belongs to.
    void setRetries(int retries);
    int retries() const;
    /// The request's transfer timeout in milliseconds, clamped to int.
    qint64 transferTimeout() const;

Q_SIGNALS:
    void error(QNetworkReply::NetworkError code);
    void finished();
    void progress(qlonglong BytesReceivedFromRequest);

private:
    QNetworkReply* m_reply = nullptr;
    QUrl m_url;
    int m_retries = 0;
};

}
}
