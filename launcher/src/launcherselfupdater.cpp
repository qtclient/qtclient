#include "launcherselfupdater.h"

#include "installationmanager.h"
#include "launcherfilesystemhelper.h"
#include "package.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QUrl>

namespace launcher {

namespace {

/// Host: a launcher built in the monorepo (git hash / "dev" version, local
/// CDN, Dev Play) must never replace itself with the tip's launcher.
bool hostSelfUpdateAllowed()
{
    if (qEnvironmentVariableIntValue("QTCLIENT_DISABLE_SELF_UPDATE") == 1)
        return false;
    if (!qEnvironmentVariableIsEmpty("QTCLIENT_DEV_JSON"))
        return false;
    const QString version = launcherVersion();
    if (version == QLatin1String("dev") || version == QLatin1String("embedded"))
        return false;
    const QUrl cdn(qEnvironmentVariable("QTCLIENT_CDN_URL"));
    const QHostAddress host(cdn.host());
    if (cdn.host() == QLatin1String("localhost") || (!host.isNull() && host.isLoopback()))
        return false;
    return true;
}

} // namespace

QString launcherVersion()
{
    return QStringLiteral(TIBIA_LAUNCHER_VERSION);
}

TLauncherSelfUpdater::TLauncherSelfUpdater(std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                                           std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper)
    : m_networkAccessManager(std::move(networkAccessManager))
    , m_filesystemHelper(std::move(filesystemHelper))
{
}

TLauncherSelfUpdater::~TLauncherSelfUpdater() = default;

bool TLauncherSelfUpdater::isUpToDate(const TPackage* launcherPackage) const
{
    qDebug() << "Current Launcher Version " << launcherVersion();
    if (!launcherPackage || !hostSelfUpdateAllowed())
        return true;
    const TPackageConfiguration* configuration = launcherPackage->configurationForCurrentPlatform();
    // Without a published version the launcher counts as up to date.
    if (!configuration)
        return true;
    return configuration->packageVersion() == launcherVersion();
}

void TLauncherSelfUpdater::update(const std::shared_ptr<TPackage>& launcherPackage)
{
    if (!launcherPackage)
        return;
    const TPackageConfiguration* configuration = launcherPackage->configurationForCurrentPlatform();
    const QString targetVersion = configuration ? configuration->packageVersion()
                                                : QStringLiteral(" invalid package version");
    qDebug() << "Starting Launcher self update from version " << launcherVersion() << " to version" << targetVersion;
    m_launcherPackage = launcherPackage;
    m_installationManager = std::make_unique<TInstallationManager>(nullptr, m_networkAccessManager, m_filesystemHelper);
    connect(m_installationManager.get(), &TInstallationManager::installationOrUpdateFinished, this,
            &TLauncherSelfUpdater::onInstallationOrUpdateFinished);
    m_installationManager->setUpdateMode(1);
    m_installationManager->installOrUpdate(launcherPackage);
}

void TLauncherSelfUpdater::onInstallationOrUpdateFinished(TPackageInstallation* installationInProgress)
{
    if (installationInProgress->succeeded()) {
        Q_EMIT updateDownloadFinishedAndRestartRequired();
        return;
    }
    // Put the previous launcher executable back.
    if (m_launcherPackage) {
        if (const TPackageConfiguration* configuration = m_launcherPackage->configurationForCurrentPlatform();
            configuration && !configuration->executable().isEmpty()) {
            const QString executable =
                TLauncherFilesystemHelper::packageDirectory(*m_launcherPackage).filePath(configuration->executable());
            restoreBackup(executable);
        }
    }
    qCritical() << "Updating Tibia failed: Error" << int(installationInProgress->errorReason());
    Q_EMIT updateFailed();
}

void TLauncherSelfUpdater::restoreBackup(const QString& path)
{
    const QString backup = path + TLauncherFilesystemHelper::backupFileSuffix();
    const QFileInfo info(backup);
    if (!info.exists() || !info.isFile())
        return;
    if (TLauncherFilesystemHelper::fileExists(path))
        TLauncherFilesystemHelper::removeFile(path);
    if (!QFile::rename(backup, path))
        qCritical() << "Failed to rename file" << backup << "to" << path;
}

void TLauncherSelfUpdater::removeBackupFiles() const
{
    const QString root = TLauncherFilesystemHelper::applicationDirectory().absolutePath();
    QDirIterator files(root, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString file = files.next();
        if (file.endsWith(TLauncherFilesystemHelper::backupFileSuffix(), Qt::CaseSensitive))
            TLauncherFilesystemHelper::removeFile(file);
    }
}

} // namespace launcher
