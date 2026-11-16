#pragma once

#include "app/hints.h"

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <stdexcept>
#include <vector>

namespace launcher {

namespace host {
class TDownloadPool;
}
class TLauncherFilesystemHelper;
class TPackage;
struct TPackageBuild;

using THint = tibia::app::Hint;

/// Thrown when the launcher configuration (tibiametadata.json) is unusable.
class TLauncherConfigurationException : public std::runtime_error
{
public:
    explicit TLauncherConfigurationException(const QString& message);
};

/// tibia::launcher::TLauncherConfigurationReader: the parsed tibiametadata.json.
class TLauncherConfigurationReader
{
public:
    /// Throws TLauncherConfigurationException when `data` is not JSON.
    TLauncherConfigurationReader(const QByteArray& data, const QUrl& baseUrl);

    const QUrl& baseUrl() const { return m_baseUrl; }
    const QByteArray& data() const { return m_data; }
    const std::shared_ptr<TPackage>& launcherPackage() const { return m_launcherPackage; }
    const std::vector<std::shared_ptr<TPackage>>& packages() const { return m_packages; }
    const std::vector<std::shared_ptr<TPackage>>& extraPackages() const { return m_extraPackages; }
    const QUrl& hardwareSurveyUrl() const { return m_hardwareSurveyUrl; }
    const QUrl& hintsUrl() const { return m_hintsUrl; }
    bool preventDefaultAutostart() const { return m_preventDefaultAutostart; }
    bool showPackageNames() const { return m_showPackageNames; }

private:
    void parse(const QJsonObject& object);

    QUrl m_baseUrl;
    QByteArray m_data;
    std::shared_ptr<TPackage> m_launcherPackage;
    std::vector<std::shared_ptr<TPackage>> m_packages;
    std::vector<std::shared_ptr<TPackage>> m_extraPackages;
    QUrl m_hardwareSurveyUrl;
    QUrl m_hintsUrl;
    bool m_preventDefaultAutostart = false;
    bool m_showPackageNames = false;
};

/// Downloads the launcher configuration, the version files of every package
/// build, and the hint feed.
class TLauncherConfigurationHandler : public QObject
{
    Q_OBJECT
public:
    TLauncherConfigurationHandler(std::shared_ptr<host::TDownloadPool> downloadPool,
                                  std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper);
    ~TLauncherConfigurationHandler() override;

    /// Starts downloading tibiametadata.json (the error timer bounds the wait).
    void downloadMainConfiguration();
    /// Downloads the version files of all package builds (`includeLauncher`: also the launcher's).
    void downloadPackageConfigurations(bool includeLauncher);
    /// Downloads the hint feed of the configuration.
    void downloadHints();

    const std::shared_ptr<TLauncherConfigurationReader>& configuration() const { return m_configuration; }
    std::shared_ptr<TPackage> launcherPackage() const;
    const THint& hint() const { return m_hint; }
    bool preventDefaultAutostart() const;
    bool showPackageNames() const;
    QUrl hardwareSurveyUrl() const;

Q_SIGNALS:
    void mainConfigurationLoaded();
    void mainConfigurationDownloadFailed();
    void diskAccessFailed(QString errorMessage);
    void packageConfigurationDownloadsFinished();
    void hintAreaDataReceived(launcher::THint hint);

public Q_SLOTS:
    void onConfigurationDownloadFinished(const QUrl& url, const QByteArray& data);
    void onConfigurationDownloadFailed(const QUrl& url, QNetworkReply::NetworkError code, const QString& errorString);
    void onErrorTimerExpired();

private:
    struct TPendingPackageConfiguration {
        TPackageBuild* build = nullptr;
        QUrl url;
        QUrl versionUrl;
        QUrl assetsUrl;
        QUrl assetsVersionUrl;
        QByteArray version;
        QByteArray assetsVersion;
    };
    bool loadMainConfiguration(const QByteArray& data);
    void mainConfigurationFailed();
    void finishPendingConfiguration(std::vector<TPendingPackageConfiguration>::iterator pending);

    std::shared_ptr<host::TDownloadPool> m_downloadPool;
    std::shared_ptr<TLauncherFilesystemHelper> m_filesystemHelper;
    QUrl m_mainConfigurationUrl;
    QTimer m_errorTimer;
    std::shared_ptr<TLauncherConfigurationReader> m_configuration;
    bool m_mainConfigurationPending = false;
    THint m_hint;
    std::vector<TPendingPackageConfiguration> m_pendingPackageConfigurations;
};

} // namespace launcher
