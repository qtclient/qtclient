#include "shared/sharedciptypes.h"

#include "client/gamecore.h"
#include "network/gameserverconnection.h"

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
    TImagestripResponse tImagestripResponse;
    TNullExperienceGainChangeTracker tNullExperienceGainChangeTracker;
    TNullExperiencePerHourProvider tNullExperiencePerHourProvider;
    TTibiaClipboardHelper tTibiaClipboardHelper;
    THashHelper tHashHelper;
    TRsaHelper tRsaHelper;
    TClockService tClockService;
    TRandomNumberGenerator tRandomNumberGenerator;
    TLocalFileInfo tLocalFileInfo;
    TDecompressionError tDecompressionError;
    TRPCServiceError tRpcServiceError;
    TCrashHandler tCrashHandler;
    TReadOnlyMemoryBlob tReadOnlyMemoryBlob;
    TLZMACompressedReadOnlyMemoryBlob tLzmaBlob;
    TZlibBaseWrapper tZlibBaseWrapper;
    TZlibInflateWrapper tZlibInflateWrapper;
    TZlibDeflateWrapper tZlibDeflateWrapper;
    TCompressionHelper tCompressionHelper;
    TCoordinate tCoordinate;
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

QByteArray THashHelper::sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
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

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    Q_UNUSED(parent);
    (void)typedKeep();
}

} // namespace tibia::shared
