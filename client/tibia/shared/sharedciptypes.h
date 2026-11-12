#pragma once

// CIP-named shared surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QAuthenticator>
#include <QByteArray>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QObject>
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

/// CIP `shared::TRandomNumberGenerator` (uniform int in [min,max]).
class TRandomNumberGenerator final : public ::tibia::CipTyped
{
public:
    TRandomNumberGenerator() = default;
    /// CIP asserts Min <= Max (randomnumbergenerator.h).
    static quint32 bounded(quint32 minInclusive, quint32 maxInclusive);
};

/// CIP `shared::TLocalFileInfo` — path holder (layout: QString at +0x8).
class TLocalFileInfo final : public ::tibia::CipTyped
{
public:
    TLocalFileInfo() = default;
    explicit TLocalFileInfo(QString path)
        : m_path(std::move(path))
    {
    }
    const QString& path() const { return m_path; }
    bool exists() const;

private:
    QString m_path;
};

/// CIP `shared::TFileSystemHelper`.
class TFileSystemHelper final : public ::tibia::CipTyped
{
public:
    TFileSystemHelper() = default;
    static bool exists(const QString& path);
};

/// CIP `shared::TDecompressionError`.
class TDecompressionError final : public ::tibia::CipTyped
{
public:
    explicit TDecompressionError(QString message = {})
        : m_message(std::move(message))
    {
    }
    const QString& message() const { return m_message; }

private:
    QString m_message;
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

/// CIP `shared::TReadOnlyMemoryBlob`.
class TReadOnlyMemoryBlob final : public ::tibia::CipTyped
{
public:
    TReadOnlyMemoryBlob() = default;
    explicit TReadOnlyMemoryBlob(QByteArray data)
        : m_data(std::move(data))
    {
    }
    const QByteArray& data() const { return m_data; }
    qsizetype size() const { return m_data.size(); }

private:
    QByteArray m_data;
};

/// CIP `shared::TLZMACompressedReadOnlyMemoryBlob`.
class TLZMACompressedReadOnlyMemoryBlob final : public ::tibia::CipTyped
{
public:
    TLZMACompressedReadOnlyMemoryBlob() = default;
    explicit TLZMACompressedReadOnlyMemoryBlob(QByteArray compressed)
        : m_compressed(std::move(compressed))
    {
    }
    const QByteArray& compressed() const { return m_compressed; }

private:
    QByteArray m_compressed;
};

/// CIP `shared::TJsonValueValidator` / object / array — thin QJson wrappers.
class TJsonValueValidator final : public ::tibia::CipTyped
{
public:
    TJsonValueValidator() = default;
    static bool isObject(const QByteArray& json);
    static bool isArray(const QByteArray& json);
};

class TJsonObjectValidator final : public ::tibia::CipTyped
{
public:
    TJsonObjectValidator() = default;
    static bool validate(const QByteArray& json);
};

class TJsonArrayValidator final : public ::tibia::CipTyped
{
public:
    TJsonArrayValidator() = default;
    static bool validate(const QByteArray& json);
};

/// CIP zlib wrappers — game wire inflate lives on `TGameserverNetworkPacketConnection`.
class TZlibBaseWrapper final : public ::tibia::CipTyped
{
public:
    TZlibBaseWrapper() = default;
};
class TZlibInflateWrapper final : public ::tibia::CipTyped
{
public:
    TZlibInflateWrapper() = default;
};
class TZlibDeflateWrapper final : public ::tibia::CipTyped
{
public:
    TZlibDeflateWrapper() = default;
};
class TCompressionHelper final : public ::tibia::CipTyped
{
public:
    TCompressionHelper() = default;
};

/// CIP `shared::TTimeLapseClockService` — offsettable clock over `TClockService`.
class TTimeLapseClockService final : public ::tibia::CipTyped
{
public:
    TTimeLapseClockService() = default;
    void setOffsetMicros(qint64 offset) { m_offsetMicros = offset; }
    qint64 nowMicros() const { return TClockService::nowMicros() + m_offsetMicros; }

private:
    qint64 m_offsetMicros = 0;
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

