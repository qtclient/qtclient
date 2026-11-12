#pragma once

// CIP-named shared surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QAuthenticator>
#include <QByteArray>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QUrl>

#include <array>
#include <cstdint>

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

/// CIP `shared::TCoordinate` (pin layout 0x10 + z at +0x10 in v2 add).
class TCoordinate final : public ::tibia::CipTyped
{
public:
    int x = 0;
    int y = 0;
    int z = 0;

    TCoordinate() = default;
    TCoordinate(int x_, int y_, int z_ = 0)
        : x(x_)
        , y(y_)
        , z(z_)
    {
    }

    /// CIP `TCoordinate::v2` — component-wise add.
    TCoordinate operator+(const TCoordinate& other) const
    {
        return TCoordinate(x + other.x, y + other.y, z + other.z);
    }

    bool operator==(const TCoordinate& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
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

class TDownloadPool final : public cip::shared::TDownloadPool
{
    Q_OBJECT
public:
    using cip::shared::TDownloadPool::TDownloadPool;
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

class TIODeviceReader final : public ::tibia::CipTyped {
public:
    TIODeviceReader() = default;
};

class TIODeviceWriter final : public ::tibia::CipTyped {
public:
    TIODeviceWriter() = default;
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

/// CIP `shared::TClipboardHelper` (v2 setText / v3 text via QGuiApplication::clipboard).
class TClipboardHelper final : public ::tibia::CipTyped
{
public:
    TClipboardHelper() = default;
    void setText(const QString& text) const;
    QString text() const;
};

/// CIP `shared::THashHelper` — CryptoPP SHA256 in the binary; product uses Qt.
class THashHelper final : public ::tibia::CipTyped
{
public:
    THashHelper() = default;
    static QByteArray sha256(const QByteArray& data);
};

/// CIP `shared::TRsaHelper` — wraps `network::` login RSA façade.
class TRsaHelper final : public ::tibia::CipTyped
{
public:
    TRsaHelper() = default;
    static void installLoginKey();
    static bool encrypt(uint8_t* msg, int size);
    static bool decrypt(uint8_t* msg, int size);
    static int blockSize();
};

/// CIP `shared::TXteaHelper` — wraps game-wire XTEA (`decryptGameXtea1405`).
class TXteaHelper final : public ::tibia::CipTyped
{
public:
    TXteaHelper() = default;
    static bool decryptInPlace(const std::array<uint32_t, 4>& key, QByteArray& body);
};

/// CIP `shared::TClockService` — engine clock in µs / ms (v2 = ns/1e6 → ms).
class TClockService final : public ::tibia::CipTyped
{
public:
    TClockService() = default;
    /// Microseconds on the game clock (product: `gamecore::nowMicros`).
    static qint64 nowMicros();
    /// Milliseconds (`nowMicros / 1000`), matching CIP v2 scale from ns.
    static qint64 nowMillis();
};

class TTibiaFileSystemHelper final : public ::tibia::CipTyped {
public:
    TTibiaFileSystemHelper() = default;
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

