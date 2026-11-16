#include "installationmanager.h"

#include "launcherconfigurationhandler.h"
#include "launcherfilesystemhelper.h"
#include "package.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

namespace launcher {

namespace {

bool fileMatches(const QString& path, const TFileInPackage& file)
{
    // Files stored packed ("unpack": false) are described by their packed size and hash.
    const qint64 size = file.unpack ? file.unpackedSize : file.packedSize;
    const QString& expectedHash = file.unpack ? file.unpackedHash : file.packedHash;
    const QFileInfo info(path);
    if (!info.isFile() || info.size() != size)
        return false;
    if (expectedHash.isEmpty())
        return true;
    QFile handle(path);
    if (!handle.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&handle);
    return QString::fromLatin1(hash.result().toHex()).compare(expectedHash, Qt::CaseInsensitive) == 0;
}

} // namespace

TPackageDifference computePackageDifference(const QDir& directory, const TPackageConfiguration* installed,
                                            const TPackageConfiguration& published)
{
    TPackageDifference difference;
    QSet<QString> publishedFiles;
    for (const TFileInPackage& file : published.files()) {
        publishedFiles.insert(file.localFile);
        if (!fileMatches(directory.filePath(file.localFile), file))
            difference.filesToDownload.push_back(file);
    }
    if (installed) {
        for (const TFileInPackage& file : installed->files()) {
            if (!publishedFiles.contains(file.localFile))
                difference.filesToRemove.push_back(file);
        }
    }
    return difference;
}

TInstallationManager::TInstallationManager(std::shared_ptr<TLauncherConfigurationReader> configuration,
                                           std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                                           std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper)
    : m_configuration(std::move(configuration))
    , m_networkAccessManager(std::move(networkAccessManager))
    , m_filesystemHelper(std::move(filesystemHelper))
{
    if (m_configuration)
        initializeLocalPackageStatus();
}

TInstallationManager::~TInstallationManager() = default;

std::vector<std::shared_ptr<TPackage>> TInstallationManager::packages() const
{
    if (!m_configuration)
        return {};
    std::vector<std::shared_ptr<TPackage>> packages = m_configuration->packages();
    const auto& extra = m_configuration->extraPackages();
    packages.insert(packages.end(), extra.begin(), extra.end());
    return packages;
}

void TInstallationManager::initializeLocalPackageStatus()
{
    bool defaultFound = false;
    for (const std::shared_ptr<TPackage>& package : packages()) {
        package->setLocalStatus(computeLocalStatus(*package));
        const QDir directory = TLauncherFilesystemHelper::packageDirectory(*package);
        const QString marker = directory.filePath(TLauncherFilesystemHelper::defaultPackageMarkerFileName());
        const bool usable = package->localStatus() == TPackage::InstalledAndUsable
                            || package->localStatus() == TPackage::PartiallyInstalled;
        if (!defaultFound && usable && TLauncherFilesystemHelper::fileExists(marker)) {
            defaultFound = true;
            package->setDefault(true);
        }
    }
}

TPackage::EStatus TInstallationManager::computeLocalStatus(const TPackage& package) const
{
    const TPackageConfiguration* published = package.configurationForCurrentPlatform();
    if (!published)
        return TPackage::NotInstalled;
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(package);
    const QString complete = directory.filePath(TLauncherFilesystemHelper::packageMarkerFileName());
    const QString partial = directory.filePath(TLauncherFilesystemHelper::partialPackageMarkerFileName());
    if (!TLauncherFilesystemHelper::fileExists(complete))
        return TLauncherFilesystemHelper::fileExists(partial) ? TPackage::PartiallyInstalled : TPackage::NotInstalled;
    TPackageConfiguration installed(TLauncherFilesystemHelper::readFile(complete),
                                    TLauncherFilesystemHelper::localAssetVersion(package),
                                    QUrl::fromLocalFile(directory.absolutePath()),
                                    QUrl::fromLocalFile(directory.absolutePath()));
    const bool usable = installed.combinedVersion() == published->combinedVersion()
                        && published->assetsVersion() != QLatin1String("ignored");
    if (usable)
        return TPackage::InstalledAndUsable;
    // An outdated installation counts as partial until it is updated.
    TLauncherFilesystemHelper::renameFile(complete, partial);
    return TPackage::PartiallyInstalled;
}

std::shared_ptr<TPackageInstallation> TInstallationManager::installationOf(const TPackage& package) const
{
    for (const auto& installation : m_installations) {
        if (installation->package().get() == &package)
            return installation;
    }
    return nullptr;
}

void TInstallationManager::installOrUpdate(const std::shared_ptr<TPackage>& package)
{
    qDebug() << "Installation (Update) of package " << package->name() << " was requested.";
    if (installationOf(*package)) {
        qDebug() << "Installation of package " << package->name() << " is already in progress.";
        return;
    }
    if (!package->configurationForCurrentPlatform()) {
        qDebug() << "Matching PackageConfiguration for package " << package->name() << " could not be determined.";
        return;
    }
    auto installation = std::make_shared<TPackageInstallation>(package, m_networkAccessManager, m_filesystemHelper);
    connect(installation.get(), &TPackageInstallation::metaDataDownloadCompleted, this,
            &TInstallationManager::onMetaDataDownloadCompleted);
    // Host: a failed channel manifest finishes the installation before it is prepared.
    connect(installation.get(), &TPackageInstallation::installationFinished, this,
            &TInstallationManager::onInstallationFinished, Qt::UniqueConnection);
    m_installations.push_back(installation);
    installation->start();
}

void TInstallationManager::onMetaDataDownloadCompleted(std::shared_ptr<TPackage> package, QByteArray packageMetaData,
                                                       QByteArray assetMetaData)
{
    TPackageConfiguration* configuration = package->configurationForCurrentPlatform();
    if (!configuration || !configuration->load(packageMetaData, assetMetaData)) {
        qDebug() << "failed to initializing package configuration for build.";
        return;
    }
    prepareInstallation(package, packageMetaData, assetMetaData);
}

void TInstallationManager::prepareInstallation(const std::shared_ptr<TPackage>& package,
                                               const QByteArray& packageMetaData, const QByteArray& assetMetaData)
{
    const std::shared_ptr<TPackageInstallation> installation = installationOf(*package);
    if (!installation)
        return;
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(*package);
    auto installed = std::make_shared<TPackageConfiguration>(
        TLauncherFilesystemHelper::localPackageVersion(*package), TLauncherFilesystemHelper::localAssetVersion(*package),
        QUrl::fromLocalFile(directory.absolutePath()), QUrl::fromLocalFile(directory.absolutePath()));
    const bool hasInstalled = installed->load(TLauncherFilesystemHelper::localPackageMetaData(*package),
                                              TLauncherFilesystemHelper::localAssetMetaData(*package));
    const TPackageConfiguration* published = package->configurationForCurrentPlatform();
    markPartiallyInstalled(*package, packageMetaData, published->packageVersion(), assetMetaData,
                           published->assetsVersion());
    QString installedVersion = installed->combinedVersion();
    QString targetVersion = published->combinedVersion();
    installation->setVersions(installedVersion, targetVersion);
    installation->setUpdateMode(m_updateMode);
    connect(installation.get(), &TPackageInstallation::installationFinished, this,
            &TInstallationManager::onInstallationFinished, Qt::UniqueConnection);
    Q_EMIT installationOrUpdateStarted(installation.get());

    auto* watcher = new QFutureWatcher<TPackageDifference>(this);
    TPackageInstallation* target = installation.get();
    connect(watcher, &QFutureWatcher<TPackageDifference>::finished, this, [this, watcher, target]() {
        watcher->deleteLater();
        onRequiredChangesComputedForInstallation(target, watcher->result());
    });
    const TPackageConfiguration publishedCopy = *published;
    watcher->setFuture(QtConcurrent::run([directory, installed, hasInstalled, publishedCopy]() {
        return computePackageDifference(directory, hasInstalled ? installed.get() : nullptr, publishedCopy);
    }));
}

void TInstallationManager::markPartiallyInstalled(const TPackage& package, const QByteArray& packageMetaData,
                                                  const QString& packageVersion, const QByteArray& assetMetaData,
                                                  const QString& assetVersion)
{
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(package);
    const QString complete = directory.filePath(TLauncherFilesystemHelper::packageMarkerFileName());
    const QString partial = directory.filePath(TLauncherFilesystemHelper::partialPackageMarkerFileName());
    if (TLauncherFilesystemHelper::fileExists(complete))
        TLauncherFilesystemHelper::renameFile(complete, partial);
    else if (!TLauncherFilesystemHelper::fileExists(partial))
        TLauncherFilesystemHelper::writeFile(partial, packageVersion.toLocal8Bit());
    if (!packageMetaData.isEmpty())
        TLauncherFilesystemHelper::writeFile(directory.filePath(TLauncherFilesystemHelper::packageMetaDataFileName()),
                                             packageMetaData);
    if (!assetMetaData.isEmpty()) {
        TLauncherFilesystemHelper::writeFile(directory.filePath(TLauncherFilesystemHelper::assetMetaDataFileName()),
                                             assetMetaData);
        if (!assetVersion.isEmpty())
            TLauncherFilesystemHelper::writeFile(directory.filePath(TLauncherFilesystemHelper::assetVersionFileName()),
                                                 assetVersion.toLocal8Bit());
    }
    qInfo().noquote() << QStringLiteral("Package %1 with version %2 is now partially installed")
                             .arg(package.name(), packageVersion);
    const_cast<TPackage&>(package).setLocalStatus(TPackage::PartiallyInstalled);
}

void TInstallationManager::onRequiredChangesComputedForInstallation(TPackageInstallation* installationOrUpdate,
                                                                    TPackageDifference changes)
{
    installationOrUpdate->setFilesToDownload(changes.filesToDownload);
    installationOrUpdate->setFilesToRemove(changes.filesToRemove);
    installationOrUpdate->setLinks(changes.links);
    installationOrUpdate->startDownload();
}

void TInstallationManager::markFullyInstalled(TPackage& package, const QString& packageVersion)
{
    qDebug() << "Marking package" << package.name() << "fully installed.";
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(package);
    const QString partial = directory.filePath(TLauncherFilesystemHelper::partialPackageMarkerFileName());
    if (TLauncherFilesystemHelper::fileExists(partial))
        TLauncherFilesystemHelper::removeFile(partial);
    TLauncherFilesystemHelper::writeFile(directory.filePath(TLauncherFilesystemHelper::packageMarkerFileName()),
                                         packageVersion.toLocal8Bit());
    qInfo().noquote() << QStringLiteral("Package %1 with version %2 is now fully installed")
                             .arg(package.name(), packageVersion);
    package.setLocalStatus(TPackage::InstalledAndUsable);
}

void TInstallationManager::onInstallationFinished(std::shared_ptr<TPackage> packageBeingInstalled)
{
    for (auto it = m_installations.begin(); it != m_installations.end(); ++it) {
        if ((*it)->package() != packageBeingInstalled)
            continue;
        const std::shared_ptr<TPackageInstallation> installation = *it;
        if (installation->succeeded()) {
            if (const TPackageConfiguration* published = packageBeingInstalled->configurationForCurrentPlatform())
                markFullyInstalled(*packageBeingInstalled, published->packageVersion());
        }
        packageBeingInstalled->setUpToDate(true);
        Q_EMIT installationOrUpdateFinished(installation.get());
        m_installations.erase(it);
        break;
    }
    if (m_installations.empty())
        Q_EMIT allStartedUpdatesFinished();
}

void TInstallationManager::installAutomaticPackages()
{
    bool defaultChosen = false;
    for (const std::shared_ptr<TPackage>& package : packages()) {
        if (!package->installAutomatically())
            continue;
        installOrUpdate(package);
        if (!defaultChosen) {
            defaultChosen = true;
            setDefaultPackage(package);
        }
    }
}

std::shared_ptr<TPackage> TInstallationManager::defaultPackage() const
{
    for (const std::shared_ptr<TPackage>& package : packages()) {
        if (package->isDefault())
            return package;
    }
    return nullptr;
}

void TInstallationManager::clearDefaultPackage()
{
    for (const std::shared_ptr<TPackage>& package : packages()) {
        package->setLocalStatus(computeLocalStatus(*package));
        const QDir directory = TLauncherFilesystemHelper::packageDirectory(*package);
        TLauncherFilesystemHelper::removeFile(
            directory.filePath(TLauncherFilesystemHelper::defaultPackageMarkerFileName()));
        package->setDefault(false);
    }
}

void TInstallationManager::setDefaultPackage(const std::shared_ptr<TPackage>& package)
{
    clearDefaultPackage();
    if (!package)
        return;
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(*package);
    TLauncherFilesystemHelper::writeFile(directory.filePath(TLauncherFilesystemHelper::defaultPackageMarkerFileName()),
                                         QByteArray());
    package->setDefault(true);
}

std::shared_ptr<TPackage> TInstallationManager::packageByExecutable(const QString& executable) const
{
    const QString wanted = QFileInfo(executable).absoluteFilePath();
    for (const std::shared_ptr<TPackage>& package : packages()) {
        loadInstalledConfiguration(*package);
        if (QFileInfo(executablePath(*package)).absoluteFilePath() == wanted)
            return package;
    }
    return nullptr;
}

void TInstallationManager::loadInstalledConfiguration(const TPackage& package) const
{
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(package);
    if (!TLauncherFilesystemHelper::fileExists(directory.filePath(TLauncherFilesystemHelper::packageMarkerFileName())))
        return;
    if (TPackageConfiguration* configuration = package.configurationForCurrentPlatform())
        configuration->load(TLauncherFilesystemHelper::localPackageMetaData(package),
                            TLauncherFilesystemHelper::localAssetMetaData(package));
}

QString TInstallationManager::executablePath(const TPackage& package) const
{
    const TPackageConfiguration* configuration = package.configurationForCurrentPlatform();
    if (!configuration)
        return {};
    return TLauncherFilesystemHelper::packageDirectory(package).filePath(configuration->executable());
}

QStringList TInstallationManager::executableParameters(const TPackage& package) const
{
    const TPackageConfiguration* configuration = package.configurationForCurrentPlatform();
    return configuration ? configuration->parameters() : QStringList();
}

} // namespace launcher
