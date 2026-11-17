#pragma once

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QTimer>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QSplashScreen;

namespace tibia::systemsurvey {
class TSystemSurveyUploader;
}

namespace tibia::shared {
class TDownloadPool;
}

namespace launcher {

class TCloudflareCdnTrace;
class TInstallationManager;
class TLauncherConfigurationHandler;
class TLauncherFilesystemHelper;
class TLauncherMainWindow;
class TLauncherSelfUpdater;
class TPackage;

/// Lock file that keeps a second launcher from starting (flock on "<dir>/.running").
class TApplicationLock
{
public:
    explicit TApplicationLock(const QString& directory);
    ~TApplicationLock();
    bool isLocked() const { return m_file.isOpen(); }
    void release();

private:
    QFile m_file;
};

/// The launcher application: fetches the configuration, self-updates,
/// installs/updates client packages and starts the chosen client.
class TLauncherApplication : public QApplication
{
    Q_OBJECT
public:
    enum ESplashState { SplashDefault = 0, SplashUpdatingLauncher = 1, SplashLaunchingClient = 2 };

    TLauncherApplication(int& argc, char** argv);
    ~TLauncherApplication() override;

    /// False when another launcher already runs.
    bool isOnlyInstance() const;
    /// Starts downloading the launcher configuration.
    void start();

private:
    void onMainConfigurationLoaded();
    /// Creates the survey uploader for the survey url and posts this machine's survey.
    void startSystemSurvey();
    void onMainConfigurationDownloadFailed();
    void onDiskAccessFailed(const QString& errorMessage);
    void onPackageConfigurationDownloadsFinished();
    void onSelfUpdateFinished();
    void startPackageManagement();
    void showMainWindow();
    void onAllStartedUpdatesFinished();
    void onClientPackageButtonClicked(const std::shared_ptr<TPackage>& package);
    void onDefaultChanged(const std::shared_ptr<TPackage>& package, bool isDefault);
    void onEulaAccepted();
    void installPrerequisites();
    void launchPackage(const std::shared_ptr<TPackage>& package);
    /// Host: the client revision sent at login (latest/release.json, else `fallback`).
    void hostResolveLoginRevision(const QString& fallback, std::function<void(QString)> then);
    void showSplash(ESplashState state);
    void hideSplash();
    void setUpLogging(const QString& logFile, qint64 maxSize);

    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
    std::shared_ptr<tibia::shared::TDownloadPool> m_downloadPool;
    std::shared_ptr<TLauncherFilesystemHelper> m_filesystemHelper;
    std::shared_ptr<TCloudflareCdnTrace> m_cdnTrace;
    std::unique_ptr<TLauncherConfigurationHandler> m_configurationHandler;
    std::unique_ptr<TLauncherSelfUpdater> m_selfUpdater;
    std::unique_ptr<TInstallationManager> m_installationManager;
    std::unique_ptr<TLauncherMainWindow> m_mainWindow;
    std::unique_ptr<tibia::systemsurvey::TSystemSurveyUploader> m_systemSurveyUploader;
    std::unique_ptr<QSplashScreen> m_splash;
    std::unique_ptr<TApplicationLock> m_lock;
    QCommandLineParser m_parser;
    QCommandLineOption m_firstLaunchOption;
    QCommandLineOption m_skipSelfUpdateOption;
    QCommandLineOption m_showUiOption;
    QCommandLineOption m_patchOption;
    QElapsedTimer m_clientStartTimer;
    QTimer m_exitTimer;
};

} // namespace launcher
