#include "shared/sharedciptypes.h"

#include "client/gamecore.h"
#include "network/gameserverconnection.h"
#include "network/logincrypt.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QRandomGenerator>

#include <numeric>

namespace tibia::shared {

namespace {
struct CipTypedKeep {
    TExperienceGainChangeTracker tExperienceGainChangeTracker;
    TIODeviceReader tIODeviceReader;
    TIODeviceWriter tIODeviceWriter;
    TImagestripResponse tImagestripResponse;
    TNullExperienceGainChangeTracker tNullExperienceGainChangeTracker;
    TNullExperiencePerHourProvider tNullExperiencePerHourProvider;
    TTibiaClipboardHelper tTibiaClipboardHelper;
    TTibiaFileSystemHelper tTibiaFileSystemHelper;
    TClipboardHelper tClipboardHelper;
    THashHelper tHashHelper;
    TRsaHelper tRsaHelper;
    TXteaHelper tXteaHelper;
    TClockService tClockService;
    TRandomNumberGenerator tRandomNumberGenerator;
    TLocalFileInfo tLocalFileInfo;
    TFileSystemHelper tFileSystemHelper;
    TDecompressionError tDecompressionError;
    TRPCServiceError tRpcServiceError;
    TCrashHandler tCrashHandler;
    TReadOnlyMemoryBlob tReadOnlyMemoryBlob;
    TLZMACompressedReadOnlyMemoryBlob tLzmaBlob;
    TJsonValueValidator tJsonValueValidator;
    TJsonObjectValidator tJsonObjectValidator;
    TJsonArrayValidator tJsonArrayValidator;
    TZlibBaseWrapper tZlibBaseWrapper;
    TZlibInflateWrapper tZlibInflateWrapper;
    TZlibDeflateWrapper tZlibDeflateWrapper;
    TCompressionHelper tCompressionHelper;
    TTimeLapseClockService tTimeLapseClockService;
    TRingBufferStopWatch tRingBufferStopWatch;
    TQtConnectionManager tQtConnectionManager;
    TCoordinate tCoordinate;
    TExtent tExtent;
    QTextBrowserWithNetwork qTextBrowserWithNetwork;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void QTextBrowserWithNetwork::onDownloadFailed(QUrl Url, QNetworkReply::NetworkError Code, QString ErrorString)
{
    Q_UNUSED(Url);
    Q_UNUSED(Code);
    Q_UNUSED(ErrorString);
}

void QTextBrowserWithNetwork::onDownloadFinished(QUrl Url, QByteArray Data)
{
    Q_UNUSED(Url);
    Q_UNUSED(Data);
}

void TClipboardHelper::setText(const QString& text) const
{
    if (QClipboard* clip = QGuiApplication::clipboard())
        clip->setText(text);
}

QString TClipboardHelper::text() const
{
    if (QClipboard* clip = QGuiApplication::clipboard())
        return clip->text();
    return {};
}

QByteArray THashHelper::sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

void TRsaHelper::installLoginKey()
{
    network::installLoginRsa();
}

bool TRsaHelper::encrypt(uint8_t* msg, int size)
{
    return network::rsaEncrypt(msg, size);
}

bool TRsaHelper::decrypt(uint8_t* msg, int size)
{
    return network::rsaDecrypt(msg, size);
}

int TRsaHelper::blockSize()
{
    return network::rsaBlockSize();
}

bool TXteaHelper::decryptInPlace(const std::array<uint32_t, 4>& key, QByteArray& body)
{
    return network::decryptGameXtea1405(key, body);
}

qint64 TClockService::nowMicros()
{
    return client::gamecore::nowMicros();
}

qint64 TClockService::nowMillis()
{
    return nowMicros() / 1000;
}

quint32 TRandomNumberGenerator::bounded(quint32 minInclusive, quint32 maxInclusive)
{
    Q_ASSERT(minInclusive <= maxInclusive);
    return QRandomGenerator::global()->bounded(minInclusive, maxInclusive + 1u);
}

bool TLocalFileInfo::exists() const
{
    return QFileInfo::exists(m_path);
}

bool TFileSystemHelper::exists(const QString& path)
{
    return QFileInfo::exists(path);
}

bool TJsonValueValidator::isObject(const QByteArray& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    return doc.isObject();
}

bool TJsonValueValidator::isArray(const QByteArray& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    return doc.isArray();
}

bool TJsonObjectValidator::validate(const QByteArray& json)
{
    return TJsonValueValidator::isObject(json);
}

bool TJsonArrayValidator::validate(const QByteArray& json)
{
    return TJsonValueValidator::isArray(json);
}

TRingBufferStopWatch::TRingBufferStopWatch(size_t capacity)
    : m_samples(capacity ? capacity : 1, 0)
{
    m_clock.start();
}

void TRingBufferStopWatch::start()
{
    m_startNs = m_clock.nsecsElapsed();
    m_running = true;
}

void TRingBufferStopWatch::stop()
{
    if (!m_running || m_samples.empty())
        return;
    m_samples[m_next] = m_clock.nsecsElapsed() - m_startNs;
    m_next = (m_next + 1) % m_samples.size();
    m_count = std::min(m_count + 1, m_samples.size());
    m_running = false;
}

qint64 TRingBufferStopWatch::averageNs() const
{
    if (m_count == 0)
        return 0;
    return std::accumulate(m_samples.begin(), m_samples.begin() + static_cast<std::ptrdiff_t>(m_count), qint64{ 0 })
        / static_cast<qint64>(m_count);
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // CipSoft network wrapper surfaces — live under the product owner for moc.
    new QNetworkAccessManagerWrapper(parent);
    new QNetworkReplyWrapper(parent);
}

} // namespace tibia::shared
