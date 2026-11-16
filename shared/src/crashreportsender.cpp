#include "crashreportsender.h"

#include <QDebug>
#include <QFileInfo>
#include <QHttpPart>
#include <QNetworkRequest>
#include <QVariant>

namespace tibia::shared {

namespace {

QHttpPart filePart(const QByteArray& body, const char* name, const QString& path)
{
    QHttpPart part;
    part.setBody(body);
    part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QString::fromUtf8("application/octet-stream")));
    const QString fileName = QString::fromUtf8(QUrl::toPercentEncoding(QFileInfo(path).fileName()));
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"").arg(QString::fromLatin1(name), fileName)));
    return part;
}

}

TCrashReportSender::TCrashReportSender(QString minidumpPath, QString sessiondumpPath,
    std::shared_ptr<TAnnotations> annotations)
    : QObject(nullptr)
    , m_minidumpPath(std::move(minidumpPath))
    , m_sessiondumpPath(std::move(sessiondumpPath))
    , m_annotations(std::move(annotations))
{
    connect(&m_networkAccessManager, &QNetworkAccessManager::finished, this,
        &TCrashReportSender::onCrashReportSendingFinished);
}

TCrashReportSender::~TCrashReportSender() = default;

const QString& TCrashReportSender::minidumpPath() const
{
    return m_minidumpPath;
}

const QString& TCrashReportSender::sessiondumpPath() const
{
    return m_sessiondumpPath;
}

const std::shared_ptr<TCrashReportSender::TAnnotations>& TCrashReportSender::annotations() const
{
    return m_annotations;
}

void TCrashReportSender::send(const QByteArray& minidump, const QByteArray& sessiondump, const QUrl& url,
    const bool includeDumps)
{
    QNetworkRequest request;
    request.setUrl(url);
    m_multiPart = createMultiPart(minidump, sessiondump, includeDumps);
    m_networkAccessManager.post(request, m_multiPart.get());
}

std::unique_ptr<QHttpMultiPart> TCrashReportSender::createMultiPart(const QByteArray& minidump,
    const QByteArray& sessiondump, const bool includeDumps) const
{
    auto multiPart = std::make_unique<QHttpMultiPart>(QHttpMultiPart::FormDataType);
    if (includeDumps) {
        multiPart->append(filePart(minidump, "minidump", m_minidumpPath));
        if (!sessiondump.isEmpty())
            multiPart->append(filePart(sessiondump, "sessiondump", m_sessiondumpPath));
    }
    if (m_annotations) {
        for (const auto& [key, value] : *m_annotations) {
            QHttpPart part;
            part.setBody(value.toUtf8());
            part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QString::fromUtf8("text/plain; charset=utf-8")));
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                QVariant(QStringLiteral("form-data; name=\"%1\"").arg(QString::fromUtf8(QUrl::toPercentEncoding(key)))));
            multiPart->append(part);
        }
    }
    return multiPart;
}

void TCrashReportSender::onCrashReportSendingFinished(const QNetworkReply* pServerReply)
{
    m_multiPart.reset();
    if (pServerReply->error() != QNetworkReply::NoError)
        qCritical() << "Uploading crash report failed:" << pServerReply->errorString();
}

}
