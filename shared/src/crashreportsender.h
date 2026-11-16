#pragma once

#include "crashreportdata.h"

#include <QByteArray>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>

#include <map>
#include <memory>

namespace tibia {
namespace shared {

/// CIP `shared::TCrashReportSender`: uploads a crash report as a multipart
/// form: the minidump and session dump files (when asked to) and one text
/// field per annotation (CipSoft adds Comment and ReportType). The upload URL
/// is CipSoft's [URLS] crashReportUrl; qtclient takes it
/// from the mod configuration (urls.crashReportPath).
class TCrashReportSender : public QObject
{
    Q_OBJECT

public:
    TCrashReportSender(QString minidumpPath, QString sessiondumpPath, std::shared_ptr<TCrashReportData> annotations);
    ~TCrashReportSender() override;

    const QString& minidumpPath() const;
    const QString& sessiondumpPath() const;
    const std::shared_ptr<TCrashReportData>& annotations() const;
    /// Posts the report to `url`; the dump parts only when `includeDumps`.
    void send(const QByteArray& minidump, const QByteArray& sessiondump, const QUrl& url, bool includeDumps);

public Q_SLOTS:
    void onCrashReportSendingFinished(const QNetworkReply* pServerReply);

private:
    std::unique_ptr<QHttpMultiPart> createMultiPart(const QByteArray& minidump, const QByteArray& sessiondump,
        bool includeDumps) const;

    QNetworkAccessManager m_networkAccessManager;
    QString m_minidumpPath;
    QString m_sessiondumpPath;
    std::shared_ptr<TCrashReportData> m_annotations;
    std::unique_ptr<QHttpMultiPart> m_multiPart;
};

}
}
