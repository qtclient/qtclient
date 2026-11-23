#include "game/fpsdropslogger.h"

#include "app/graphicsrestart.h"
#include "client/clientciptypes.h"
#include "config/clientoptions.h"
#include "gamewindow/dialogmanager.h"
#include "jsondocumenthttppostsender.h"
#include "jsondocumentuploaderbase.h"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>

namespace tibia::game {

namespace {
/// Frames are checked for drops from this long after the logger was made on.
constexpr qint64 WarmUpMilliseconds = 60000;
/// The JSON report's data type.
constexpr int FrameDropDataType = 3;
} // namespace

void FPSDropsLogger::TFrameDurations::pushBack(const qint64 duration)
{
    if (m_storage.empty())
        return;
    if (full()) {
        m_storage[m_first] = duration;
        m_first = (m_first + 1) % m_storage.size();
        return;
    }
    m_storage[(m_first + m_size) % m_storage.size()] = duration;
    ++m_size;
}

void FPSDropsLogger::TFrameDurations::clear()
{
    m_first = (m_first + m_size) % m_storage.size();
    m_size = 0;
}

FPSDropsLogger::FPSDropsLogger(const QUrl& url, const config::TClientOptions* options, QObject* parent)
    : cip::tibia::game::FPSDropsLogger(parent)
    , m_url(url)
    , m_options(options)
    , m_creationTime(QDateTime::currentMSecsSinceEpoch())
{
    auto sender = std::make_shared<shared::TJsonDocumentHttpPostSender>();
    m_uploader = std::make_shared<shared::TJsonDocumentUploaderBase>(sender);
    if (gamewindow::TDialogStackController* dialogs = gamewindow::TDialogStackController::instance())
        connect(dialogs, &gamewindow::TDialogStackController::dialogVisible, this, &FPSDropsLogger::onDialogVisibleChanged);
}

FPSDropsLogger::~FPSDropsLogger() = default;

void FPSDropsLogger::onDialogVisibleChanged(const bool Visible)
{
    if (m_dialogVisible == Visible)
        return;
    m_dialogVisible = Visible;
    if (Visible) {
        m_frameDurations.clear();
        m_frameDurationSum = 0;
    }
}

void FPSDropsLogger::resetFrameDurations()
{
    m_frameDurations.clear();
    m_frameDurationSum = 0;
}

void FPSDropsLogger::addFrameDuration(const qint64 duration)
{
    if (m_dialogVisible)
        return;
    m_frameDurationSum += duration;
    // A full ring drops its oldest frame from the sum.
    if (m_frameDurations.full())
        m_frameDurationSum -= m_frameDurations.front();
    m_frameDurations.pushBack(duration);
    checkForFrameDrop();
}

void FPSDropsLogger::checkForFrameDrop()
{
    if (QDateTime::currentMSecsSinceEpoch() - m_creationTime < WarmUpMilliseconds)
        return;
    if (!m_frameDurations.full())
        return;
    const std::size_t count = m_frameDurations.size();
    const qint64 middle = m_frameDurations.at(count / 2);
    const double averageOfOthers = static_cast<double>(m_frameDurationSum - middle) / static_cast<double>(count - 1);
    if (!(averageOfOthers * 10.0 < static_cast<double>(middle)) || middle <= m_longestReportedDuration)
        return;
    m_longestReportedDuration = middle;
    m_uploader->upload(m_url, createReport());
}

void FPSDropsLogger::setPlayerPosition(const worldmap::TWorldMapCoordinate& position)
{
    m_playerPosition.x = position.x;
    m_playerPosition.y = position.y;
    m_playerPosition.z = position.z;
}

QJsonDocument FPSDropsLogger::createReport() const
{
    using O = cip::EClientOption;
    const QString clientId = client::TGameApplication::hardwareId();
    const QString renderer = app::graphicsrestart::rendererDisplayName(app::graphicsrestart::activeRenderer());
    const bool lightEffectsActive = m_options->boolOption(O::lightEffectsEnabled);
    const bool frameRateLimitActive = m_options->boolOption(O::frameRateLimitEnabled);
    const bool vsyncActive = m_options->boolOption(O::vSyncEnabled);
    const int frameRateLimit = m_options->intOption(O::frameRateLimit);
    const QString playerPosition =
        QStringLiteral("%1,%2,%3").arg(m_playerPosition.x).arg(m_playerPosition.y).arg(m_playerPosition.z);

    QJsonObject report;
    report.insert(QStringLiteral("dataType"), FrameDropDataType);
    report.insert(QStringLiteral("clientID"), clientId);
    report.insert(QStringLiteral("playerPosition"), playerPosition);
    report.insert(QStringLiteral("characterName"), m_characterName);
    report.insert(QStringLiteral("worldName"), m_worldName);
    report.insert(QStringLiteral("renderer"), renderer);
    report.insert(QStringLiteral("lightEffectsActive"), lightEffectsActive);
    report.insert(QStringLiteral("frameRateLimitActive"), frameRateLimitActive);
    report.insert(QStringLiteral("frameRateLimit"), frameRateLimit);
    report.insert(QStringLiteral("vsyncActive"), vsyncActive);

    // The spike is the middle frame; the averages are those of the frames
    // before and after it.
    const std::size_t count = m_frameDurations.size();
    const std::size_t spike = count / 2;
    report.insert(QStringLiteral("durationOfSpike"), m_frameDurations.at(spike));
    quint64 before = 0;
    for (std::size_t i = 0; i < spike; ++i)
        before += static_cast<quint64>(m_frameDurations.at(i));
    report.insert(QStringLiteral("averageDurationBeforeSpike"), static_cast<qint64>(before / spike));
    quint64 after = 0;
    for (std::size_t i = spike + 1; i < count; ++i)
        after += static_cast<quint64>(m_frameDurations.at(i));
    report.insert(QStringLiteral("averageDurationAfterSpike"), static_cast<qint64>(after / (count - spike - 1)));
    return QJsonDocument(report);
}

}
