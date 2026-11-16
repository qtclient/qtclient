#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QNetworkAccessManager;

namespace launcher {

class TInstallationManager;
class TLauncherFilesystemHelper;
class TPackage;
class TPackageInstallation;

/// Version of the running launcher.
QString launcherVersion();

/// Updates the launcher's own package in place, then asks for a restart.
class TLauncherSelfUpdater : public QObject
{
    Q_OBJECT
public:
    TLauncherSelfUpdater(std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                         std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper);
    ~TLauncherSelfUpdater() override;

    /// True when `launcherPackage` is missing or publishes the running version.
    bool isUpToDate(const TPackage* launcherPackage) const;
    void update(const std::shared_ptr<TPackage>& launcherPackage);

Q_SIGNALS:
    void updateDownloadFinishedAndRestartRequired();
    void updateFailed();
    void diskAccessFailed(QString errorMessage);

public Q_SLOTS:
    void onInstallationOrUpdateFinished(launcher::TPackageInstallation* installationInProgress);

private:
    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
    std::shared_ptr<TLauncherFilesystemHelper> m_filesystemHelper;
    std::unique_ptr<TInstallationManager> m_installationManager;
};

} // namespace launcher
