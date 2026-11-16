#include "launcherconfigurationhandler.h"

#include "downloadpool.h"
#include "qnetworkaccessmanagerwrapper.h"
#include "launcherfilesystemhelper.h"
#include "package.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

namespace launcher {

namespace {

const QString kCachedConfigurationFileName = QStringLiteral("launchermetadata.json");

} // namespace

TLauncherConfigurationException::TLauncherConfigurationException(const QString& message)
    : std::runtime_error(message.toStdString())
{
}

// --- TLauncherConfigurationReader --------------------------------------------------------

TLauncherConfigurationReader::TLauncherConfigurationReader(const QByteArray& data, const QUrl& baseUrl)
    : m_baseUrl(baseUrl)
    , m_data(data)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        throw TLauncherConfigurationException(error.errorString());
    parse(document.object());
}

void TLauncherConfigurationReader::parse(const QJsonObject& object)
{
    if (object.value(QStringLiteral("launcher")).isObject()) {
        m_launcherPackage = std::make_shared<TPackage>(TPackage::LauncherPackageType,
                                                       object.value(QStringLiteral("launcher")).toObject(), m_baseUrl);
    }
    m_packages.clear();
    if (object.value(QStringLiteral("packages")).isArray()) {
        int type = 0;
        for (const QJsonValue& package : object.value(QStringLiteral("packages")).toArray())
            m_packages.push_back(std::make_shared<TPackage>(type++, package.toObject(), m_baseUrl));
    }
    m_hardwareSurveyUrl = QUrl(object.value(QStringLiteral("hardwaresurvey")).toString());
    m_hintsUrl = QUrl(object.value(QStringLiteral("hints")).toString());
    m_preventDefaultAutostart = object.value(QStringLiteral("preventdefaultautostart")).toBool();
    m_showPackageNames = object.value(QStringLiteral("showpackagenames")).toBool();
}

// --- TLauncherConfigurationHandler -------------------------------------------------------

TLauncherConfigurationHandler::TLauncherConfigurationHandler(std::shared_ptr<tibia::shared::TDownloadPool> downloadPool,
                                                             std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper)
    : m_downloadPool(std::move(downloadPool))
    , m_filesystemHelper(std::move(filesystemHelper))
    , m_mainConfigurationUrl(m_filesystemHelper->launcherConfigurationUrl())
{
    connect(m_downloadPool.get(), &tibia::shared::TDownloadPool::success, this,
            &TLauncherConfigurationHandler::onConfigurationDownloadFinished);
    connect(m_downloadPool.get(), &tibia::shared::TDownloadPool::failed, this,
            &TLauncherConfigurationHandler::onConfigurationDownloadFailed);
    m_errorTimer.setSingleShot(true);
    m_errorTimer.setInterval(m_filesystemHelper->downloadErrorTimeout());
    connect(&m_errorTimer, &QTimer::timeout, this, &TLauncherConfigurationHandler::onErrorTimerExpired);
}

TLauncherConfigurationHandler::~TLauncherConfigurationHandler() = default;

std::shared_ptr<TPackage> TLauncherConfigurationHandler::launcherPackage() const
{
    return m_configuration ? m_configuration->launcherPackage() : nullptr;
}

bool TLauncherConfigurationHandler::preventDefaultAutostart() const
{
    return m_configuration && m_configuration->preventDefaultAutostart();
}

bool TLauncherConfigurationHandler::showPackageNames() const
{
    return m_configuration && m_configuration->showPackageNames();
}

QUrl TLauncherConfigurationHandler::hardwareSurveyUrl() const
{
    return m_configuration ? m_configuration->hardwareSurveyUrl() : QUrl();
}

void TLauncherConfigurationHandler::downloadMainConfiguration()
{
    m_mainConfigurationPending = true;
    if (m_filesystemHelper->downloadErrorTimeout() > 0)
        m_errorTimer.start();
    if (!m_mainConfigurationUrl.isValid() || m_mainConfigurationUrl.isEmpty()) {
        mainConfigurationFailed();
        return;
    }
    if (m_mainConfigurationUrl.isLocalFile()) {
        // Host: a tibiametadata.json next to the launcher (release sidecar).
        onConfigurationDownloadFinished(m_mainConfigurationUrl,
                                        TLauncherFilesystemHelper::readFile(m_mainConfigurationUrl.toLocalFile()));
        return;
    }
    m_downloadPool->download(m_mainConfigurationUrl, m_filesystemHelper->retries(),
                             m_filesystemHelper->configurationTransportTimeout());
}

void TLauncherConfigurationHandler::downloadPackageConfigurations(bool includeLauncher)
{
    if (!m_configuration)
        return;
    std::vector<std::shared_ptr<TPackage>> packages = m_configuration->packages();
    if (includeLauncher && m_configuration->launcherPackage())
        packages.insert(packages.begin(), m_configuration->launcherPackage());
    for (const std::shared_ptr<TPackage>& package : packages) {
        for (const auto& build : package->builds()) {
            if (build->os != currentOperatingSystem())
                continue;
            TPendingPackageConfiguration pending;
            pending.build = build.get();
            // The setting busts caches in front of the CDN for the four urls of a build.
            const bool nocache = m_filesystemHelper->appendNocacheToPackageUrls();
            const auto packageUrl = [nocache](const QUrl& url) {
                return nocache ? tibia::shared::TDownloadPool::appendNocache(url) : url;
            };
            pending.url = packageUrl(build->url);
            pending.versionUrl = packageUrl(build->versionUrl);
            pending.assetsUrl = packageUrl(build->assetsUrl);
            pending.assetsVersionUrl = packageUrl(build->assetsVersionUrl);
            // Host channels count only with assets (their files install with them).
            if (build->assetsUrl.isValid() && !build->assetsUrl.isEmpty()) {
                for (const THostChannel& channel : TLauncherFilesystemHelper::enabledHostChannels(*build)) {
                    if (channel.versionUrl.isValid() && !channel.versionUrl.isEmpty())
                        pending.hostChannelVersionUrls.emplace_back(channel.name, channel.versionUrl);
                }
            }
            m_pendingPackageConfigurations.push_back(pending);
        }
    }
    if (m_pendingPackageConfigurations.empty()) {
        Q_EMIT packageConfigurationDownloadsFinished();
        return;
    }
    // Copy: finishing an entry erases it from the list.
    const auto pending = m_pendingPackageConfigurations;
    for (const TPendingPackageConfiguration& configuration : pending) {
        bool requested = false;
        std::vector<QUrl> urls{configuration.versionUrl, configuration.assetsVersionUrl};
        for (const auto& channel : configuration.hostChannelVersionUrls)
            urls.push_back(channel.second);
        for (const QUrl& url : urls) {
            if (!url.isValid() || url.isEmpty())
                continue;
            requested = true;
            m_downloadPool->download(url, m_filesystemHelper->retries(),
                                     m_filesystemHelper->configurationTransportTimeout());
        }
        if (!requested) {
            for (auto it = m_pendingPackageConfigurations.begin(); it != m_pendingPackageConfigurations.end(); ++it) {
                if (it->build == configuration.build) {
                    finishPendingConfiguration(it);
                    break;
                }
            }
        }
    }
}

void TLauncherConfigurationHandler::downloadHints()
{
    if (!m_configuration || !m_configuration->hintsUrl().isValid() || m_configuration->hintsUrl().isEmpty())
        return;
    m_downloadPool->download(m_configuration->hintsUrl(), m_filesystemHelper->retries(),
                             m_filesystemHelper->configurationTransportTimeout());
}

bool TLauncherConfigurationHandler::loadMainConfiguration(const QByteArray& data)
{
    try {
        m_configuration = std::make_shared<TLauncherConfigurationReader>(
            data, m_mainConfigurationUrl.adjusted(QUrl::RemoveFilename));
    } catch (const TLauncherConfigurationException& exception) {
        qCritical() << "Launcher configuration could not be parsed:" << exception.what();
        return false;
    }
    return true;
}

void TLauncherConfigurationHandler::onConfigurationDownloadFinished(const QUrl& url, const QByteArray& data)
{
    if (data.isEmpty()) {
        onConfigurationDownloadFailed(url, QNetworkReply::UnknownContentError, QStringLiteral("Data stream is empty"));
        return;
    }
    if (url == m_mainConfigurationUrl && m_mainConfigurationPending) {
        m_errorTimer.stop();
        qInfo() << "Finished download of main configuration from " << url.toString();
        if (!loadMainConfiguration(data)) {
            mainConfigurationFailed();
            return;
        }
        m_mainConfigurationPending = false;
        // Keep the last good configuration for offline starts.
        const QString cache = TLauncherFilesystemHelper::dataDirectory().filePath(kCachedConfigurationFileName);
        if (!TLauncherFilesystemHelper::writeFile(cache, data))
            Q_EMIT diskAccessFailed(cache);
        Q_EMIT mainConfigurationLoaded();
        return;
    }
    if (m_configuration && url == m_configuration->hintsUrl()) {
        const std::optional<THint> hint =
            tibia::shared::selectHint(tibia::shared::parseHints(data), QDateTime::currentDateTimeUtc());
        if (!hint)
            return;
        m_hint = *hint;
        Q_EMIT hintAreaDataReceived(m_hint);
        return;
    }
    for (auto it = m_pendingPackageConfigurations.begin(); it != m_pendingPackageConfigurations.end(); ++it) {
        const bool hostChannel = takeHostChannelVersion(*it, url, &data);
        if (!hostChannel && url != it->versionUrl && url != it->assetsVersionUrl)
            continue;
        qInfo() << "Finished downloading file for package " << describeBuild(*it->build) << ": " << url.toString();
        if (url == it->versionUrl)
            it->version = data;
        if (url == it->assetsVersionUrl)
            it->assetsVersion = data;
        if (isComplete(*it)) {
            qInfo() << "Package configuration for" << describeBuild(*it->build) << "loaded completely";
            finishPendingConfiguration(it);
        }
        return;
    }
}

bool TLauncherConfigurationHandler::isComplete(const TPendingPackageConfiguration& pending)
{
    const bool versionDone = !pending.versionUrl.isValid() || pending.versionUrl.isEmpty() || !pending.version.isEmpty();
    const bool assetsDone = !pending.assetsVersionUrl.isValid() || pending.assetsVersionUrl.isEmpty()
                            || !pending.assetsVersion.isEmpty();
    return versionDone && assetsDone && pending.hostChannelVersionUrls.empty();
}

bool TLauncherConfigurationHandler::takeHostChannelVersion(TPendingPackageConfiguration& pending, const QUrl& url,
                                                           const QByteArray* data)
{
    for (auto channel = pending.hostChannelVersionUrls.begin(); channel != pending.hostChannelVersionUrls.end();
         ++channel) {
        if (channel->second != url)
            continue;
        // A failed channel version leaves the channel out; the client still installs.
        if (data)
            pending.hostChannelVersions.emplace_back(channel->first, *data);
        else
            qWarning() << "Version of channel" << channel->first << "could not be downloaded; skipping it.";
        pending.hostChannelVersionUrls.erase(channel);
        return true;
    }
    return false;
}

void TLauncherConfigurationHandler::finishPendingConfiguration(
    std::vector<TPendingPackageConfiguration>::iterator pending)
{
    pending->build->configuration = std::make_unique<TPackageConfiguration>(
        pending->version, hostCompositeAssetsVersion(pending->assetsVersion, pending->hostChannelVersions),
        pending->url, pending->assetsUrl);
    pending->build->activeHostChannels.clear();
    for (const auto& channel : pending->hostChannelVersions)
        pending->build->activeHostChannels.append(channel.first);
    m_pendingPackageConfigurations.erase(pending);
    if (m_pendingPackageConfigurations.empty())
        Q_EMIT packageConfigurationDownloadsFinished();
}

void TLauncherConfigurationHandler::onConfigurationDownloadFailed(const QUrl& url, QNetworkReply::NetworkError,
                                                                  const QString& errorString)
{
    if (url == m_mainConfigurationUrl) {
        qCritical() << "Download of main configuration failed:" << errorString;
        m_errorTimer.stop();
        mainConfigurationFailed();
        return;
    }
    for (auto it = m_pendingPackageConfigurations.begin(); it != m_pendingPackageConfigurations.end(); ++it) {
        if (takeHostChannelVersion(*it, url, nullptr)) {
            if (isComplete(*it))
                finishPendingConfiguration(it);
            return;
        }
        if (url != it->url && url != it->versionUrl && url != it->assetsUrl && url != it->assetsVersionUrl)
            continue;
        qCritical() << "Download of package configuration for " << describeBuild(*it->build) << "from configuration"
                    << url.toString() << "failed:" << errorString;
        m_pendingPackageConfigurations.erase(it);
        if (m_pendingPackageConfigurations.empty())
            Q_EMIT packageConfigurationDownloadsFinished();
        return;
    }
}

void TLauncherConfigurationHandler::onErrorTimerExpired()
{
    m_errorTimer.stop();
    mainConfigurationFailed();
}

void TLauncherConfigurationHandler::mainConfigurationFailed()
{
    if (!m_mainConfigurationPending)
        return;
    m_mainConfigurationPending = false;
    // Fall back to the configuration of the last successful start.
    const QString cache = TLauncherFilesystemHelper::dataDirectory().filePath(kCachedConfigurationFileName);
    const QByteArray cached = TLauncherFilesystemHelper::readFile(cache);
    if (!cached.isEmpty() && loadMainConfiguration(cached)) {
        Q_EMIT mainConfigurationLoaded();
        return;
    }
    Q_EMIT mainConfigurationDownloadFailed();
}

} // namespace launcher
