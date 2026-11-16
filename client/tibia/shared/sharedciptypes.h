#pragma once

// CIP-named shared surfaces (promoted from cipwire).

#include "shared/cip_typed.h"
#include "shared/clockservice.h"
#include "shared/coordinate.h"

#include <cip_gen.h>

#include <QAuthenticator>
#include <QByteArray>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QObject>

#include <string_view>
#include <QString>
#include <QtGlobal>
#include <QUrl>

#include <array>
#include <cstdint>
#include <vector>

namespace tibia {
namespace shared {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CipSoft network auth fan-in (moc `authenticationRequired`).
class QNetworkAccessManagerWrapper final : public cip::shared::QNetworkAccessManagerWrapper
{
    Q_OBJECT
public:
    using cip::shared::QNetworkAccessManagerWrapper::QNetworkAccessManagerWrapper;

Q_SIGNALS:
    void authenticationRequired(QNetworkReply* pReply, QAuthenticator* pAuthenticator);
};

/// CipSoft reply progress/finish (moc `error` / `finished` / `progress`).
class QNetworkReplyWrapper final : public cip::shared::QNetworkReplyWrapper
{
    Q_OBJECT
public:
    using cip::shared::QNetworkReplyWrapper::QNetworkReplyWrapper;

Q_SIGNALS:
    void error(QNetworkReply::NetworkError code);
    void finished();
    void progress(qlonglong BytesReceivedFromRequest);
};

/// CipSoft QTextBrowserWithNetwork download slots (contract moc).
/// Live QWidget implementation lives in apps/qtclient/shared/;
/// this stub owns Class::method keys for the client inventory walk.
class QTextBrowserWithNetwork final : public ::tibia::CipTyped
{
public:
    QTextBrowserWithNetwork() = default;
    void onDownloadFailed(QUrl Url, QNetworkReply::NetworkError Code, QString ErrorString);
    void onDownloadFinished(QUrl Url, QByteArray Data);
};

/// CIP `shared::TExtent` (width/height/depth volume).
class TExtent final : public ::tibia::CipTyped
{
public:
    int width = 0;
    int height = 0;
    int depth = 0;

    TExtent() = default;
    TExtent(int width_, int height_, int depth_ = 1)
        : width(width_)
        , height(height_)
        , depth(depth_)
    {
    }

    bool operator==(const TExtent& other) const
    {
        return width == other.width && height == other.height && depth == other.depth;
    }
};

class TCrashReportSender final : public cip::shared::TCrashReportSender
{
    Q_OBJECT
public:
    using cip::shared::TCrashReportSender::TCrashReportSender;
};

class TDynamicResourcesHelper final : public cip::tibia::shared::TDynamicResourcesHelper
{
    Q_OBJECT
public:
    using cip::tibia::shared::TDynamicResourcesHelper::TDynamicResourcesHelper;
};

class TExecutor final : public cip::shared::TExecutor
{
    Q_OBJECT
public:
    using cip::shared::TExecutor::TExecutor;
};

class TExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TExperienceGainChangeTracker() = default;
};

class THighlightTimer final : public cip::shared::THighlightTimer
{
    Q_OBJECT
public:
    using cip::shared::THighlightTimer::THighlightTimer;
};

class TImagestripResponse final : public ::tibia::CipTyped {
public:
    TImagestripResponse() = default;
};

class TJsonDocumentHttpPostSender final : public cip::shared::TJsonDocumentHttpPostSender
{
    Q_OBJECT
public:
    using cip::shared::TJsonDocumentHttpPostSender::TJsonDocumentHttpPostSender;
};

class TJsonDocumentUploaderBase final : public cip::shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    using cip::shared::TJsonDocumentUploaderBase::TJsonDocumentUploaderBase;
};

class TNetworkAccessManagerAuthenticationHandler final : public cip::shared::TNetworkAccessManagerAuthenticationHandler
{
    Q_OBJECT
public:
    using cip::shared::TNetworkAccessManagerAuthenticationHandler::TNetworkAccessManagerAuthenticationHandler;
};

class TNullExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TNullExperienceGainChangeTracker() = default;
};

class TNullExperiencePerHourProvider final : public ::tibia::CipTyped {
public:
    TNullExperiencePerHourProvider() = default;
};

// TOptimized1PixelBorderImageProvider — live QQuickImageProvider in
// imageproviders/optimizedborderimageprovider.h (CipSoft requestImage).

class TQtConnectionWrapper final : public cip::shared::TQtConnectionWrapper
{
    Q_OBJECT
public:
    using cip::shared::TQtConnectionWrapper::TQtConnectionWrapper;
};

class TRPCServiceClient final : public cip::shared::TRPCServiceClient
{
    Q_OBJECT
public:
    using cip::shared::TRPCServiceClient::TRPCServiceClient;
};

class TRPCServiceConnection final : public cip::shared::TRPCServiceConnection
{
    Q_OBJECT
public:
    using cip::shared::TRPCServiceConnection::TRPCServiceConnection;
};

class TRPCServiceServer final : public cip::shared::TRPCServiceServer
{
    Q_OBJECT
public:
    using cip::shared::TRPCServiceServer::TRPCServiceServer;
};

class TSingleThreadedLowPriorityExecutor final : public cip::shared::TSingleThreadedLowPriorityExecutor
{
    Q_OBJECT
public:
    using cip::shared::TSingleThreadedLowPriorityExecutor::TSingleThreadedLowPriorityExecutor;
};

class TSynchronousExecutor final : public cip::shared::TSynchronousExecutor
{
    Q_OBJECT
public:
    using cip::shared::TSynchronousExecutor::TSynchronousExecutor;
};

class TTibiaClipboardHelper final : public ::tibia::CipTyped {
public:
    TTibiaClipboardHelper() = default;
};

/// CIP `shared::TRPCServiceError`.
class TRPCServiceError final : public ::tibia::CipTyped
{
public:
    explicit TRPCServiceError(QString message = {})
        : m_message(std::move(message))
    {
    }
    const QString& message() const { return m_message; }

private:
    QString m_message;
};

/// CIP `shared::TCrashHandler` — product host owns real crash reporting.
class TCrashHandler final : public ::tibia::CipTyped
{
public:
    TCrashHandler() = default;
};

/// CIP `shared::TRingBufferStopWatch` — recent durations (see also worldmap frame ring).
class TRingBufferStopWatch final : public ::tibia::CipTyped
{
public:
    explicit TRingBufferStopWatch(size_t capacity = 10);
    void start();
    void stop();
    qint64 averageNs() const;

private:
    QElapsedTimer m_clock;
    qint64 m_startNs = 0;
    bool m_running = false;
    std::vector<qint64> m_samples;
    size_t m_next = 0;
    size_t m_count = 0;
};

/// CIP `shared::TQtConnectionManager` — host owns live QObject connections.
class TQtConnectionManager final : public ::tibia::CipTyped
{
public:
    TQtConnectionManager() = default;
};

}
}

namespace tibia {
namespace shared {
namespace executors_private {

class TaskExecutorObject final : public cip::shared::executors_private::TaskExecutorObject
{
    Q_OBJECT
public:
    using cip::shared::executors_private::TaskExecutorObject::TaskExecutorObject;
};

}
}
}

