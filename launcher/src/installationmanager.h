#pragma once

#include "packageinstallation.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

class QNetworkAccessManager;

namespace launcher {

class TLauncherConfigurationReader;
class TLauncherFilesystemHelper;
class TPackage;

/// Tracks the installation state of all packages of the launcher
/// configuration and runs their installations and updates.
class TInstallationManager : public QObject
{
    Q_OBJECT
public:
    TInstallationManager(std::shared_ptr<TLauncherConfigurationReader> configuration,
                         std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                         std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper);
    ~TInstallationManager() override;

    /// Client packages of the configuration (then the extra ones).
    std::vector<std::shared_ptr<TPackage>> packages() const;
    /// Starts installing or updating `package` unless that already runs.
    void installOrUpdate(const std::shared_ptr<TPackage>& package);
    /// Installs every package marked `installautomatically`; the first becomes default.
    void installAutomaticPackages();
    std::shared_ptr<TPackage> defaultPackage() const;
    /// Makes `package` the default client (nullptr: no default).
    void setDefaultPackage(const std::shared_ptr<TPackage>& package);
    /// Client package whose executable is `executable` (absolute path), or nullptr.
    std::shared_ptr<TPackage> packageByExecutable(const QString& executable) const;
    /// Reads the installed package.json / assets.json into the package's configuration.
    void loadInstalledConfiguration(const TPackage& package) const;
    QString executablePath(const TPackage& package) const;
    QStringList executableParameters(const TPackage& package) const;
    void setUpdateMode(int updateMode) { m_updateMode = updateMode; }

    TPackage::EStatus computeLocalStatus(const TPackage& package) const;

Q_SIGNALS:
    void installationOrUpdateStarted(launcher::TPackageInstallation* installationInProgress);
    void installationOrUpdateFinished(launcher::TPackageInstallation* installationInProgress);
    void allStartedUpdatesFinished();
    void diskAccessFailed(QString errorMessage);

public Q_SLOTS:
    void onMetaDataDownloadCompleted(std::shared_ptr<launcher::TPackage> package, QByteArray packageMetaData,
                                     QByteArray assetMetaData);
    void onInstallationFinished(std::shared_ptr<launcher::TPackage> packageBeingInstalled);
    void onRequiredChangesComputedForInstallation(launcher::TPackageInstallation* installationOrUpdate,
                                                  launcher::TPackageDifference changes);

private:
    void initializeLocalPackageStatus();
    void prepareInstallation(const std::shared_ptr<TPackage>& package, const QByteArray& packageMetaData,
                             const QByteArray& assetMetaData);
    void markPartiallyInstalled(const TPackage& package, const QByteArray& packageMetaData,
                                const QString& packageVersion, const QByteArray& assetMetaData,
                                const QString& assetVersion);
    void markFullyInstalled(TPackage& package, const QString& packageVersion);
    void clearDefaultPackage();
    std::shared_ptr<TPackageInstallation> installationOf(const TPackage& package) const;

    std::shared_ptr<TLauncherConfigurationReader> m_configuration;
    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
    std::shared_ptr<TLauncherFilesystemHelper> m_filesystemHelper;
    std::vector<std::shared_ptr<TPackageInstallation>> m_installations;
    int m_updateMode = 0;
};

/// Compares an installed package configuration with the published one.
TPackageDifference computePackageDifference(const QDir& directory, const TPackageConfiguration* installed,
                                            const TPackageConfiguration& published);

} // namespace launcher
