#include "qnetworkreplywrapper.h"

#include <QNetworkRequest>

#include <algorithm>
#include <climits>

namespace tibia::shared {

QNetworkReplyWrapper::QNetworkReplyWrapper(QNetworkReply* reply)
    : QObject(reply)
    , m_reply(reply)
{
    if (!reply)
        return;
    connect(reply, &QNetworkReply::errorOccurred, this, [this](const QNetworkReply::NetworkError code) { Q_EMIT error(code); });
    connect(reply, &QNetworkReply::finished, this, [this] { Q_EMIT finished(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
        [this](const qint64 bytesReceived, qint64) { Q_EMIT progress(bytesReceived); });
}

QNetworkReply::NetworkError QNetworkReplyWrapper::replyError() const
{
    return m_reply->error();
}

QString QNetworkReplyWrapper::errorString() const
{
    return m_reply->errorString();
}

QByteArray QNetworkReplyWrapper::readAll()
{
    return m_reply->readAll();
}

QUrl QNetworkReplyWrapper::url() const
{
    if (!m_url.isEmpty())
        return m_url;
    return m_reply->request().url();
}

QVariant QNetworkReplyWrapper::attribute(const QNetworkRequest::Attribute code) const
{
    return m_reply->attribute(code);
}

bool QNetworkReplyWrapper::hasRawHeader(const QByteArray& headerName) const
{
    return m_reply->hasRawHeader(headerName);
}

QByteArray QNetworkReplyWrapper::rawHeader(const QByteArray& headerName) const
{
    return m_reply->rawHeader(headerName);
}

void QNetworkReplyWrapper::setUrl(const QUrl& url)
{
    m_url = url;
}

void QNetworkReplyWrapper::setRetries(const int retries)
{
    m_retries = retries;
}

int QNetworkReplyWrapper::retries() const
{
    return m_retries;
}

qint64 QNetworkReplyWrapper::transferTimeout() const
{
    return std::clamp<qint64>(m_reply->request().transferTimeoutAsDuration().count(), INT_MIN, INT_MAX);
}

}
