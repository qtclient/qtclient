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
    TClockService tClockService;
    TRPCServiceError tRpcServiceError;
    TCrashHandler tCrashHandler;
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
