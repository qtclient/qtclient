#include "packageinstallation.h"

#include "host/downloadpool.h"
#include "launcherfilesystemhelper.h"
#include "package.h"

#include <QDebug>
#include <QFile>

namespace launcher {

namespace {

/// Package metadata must be fetched when nothing is installed or the installed
/// package version differs from the published one.
bool needsPackageMetaData(const TPackage& package)
{
    const TPackageConfiguration* configuration = package.configurationForCurrentPlatform();
    if (TLauncherFilesystemHelper::localPackageMetaData(package).isEmpty() || !configuration)
        return true;
    return QString::fromLocal8Bit(TLauncherFilesystemHelper::localPackageVersion(package))
           != configuration->packageVersion();
}

bool needsAssetMetaData(const TPackage& package)
{
    const TPackageBuild* build = package.buildForCurrentPlatform();
    if (!build || !build->assetsUrl.isValid() || build->assetsUrl.isEmpty())
        return false;
    const TPackageConfiguration* configuration = package.configurationForCurrentPlatform();
    if (TLauncherFilesystemHelper::localAssetMetaData(package).isEmpty() || !configuration)
        return true;
    return QString::fromLocal8Bit(TLauncherFilesystemHelper::localAssetVersion(package))
           != configuration->assetsVersion();
}

} // namespace

TPackageInstallation::TPackageInstallation(std::shared_ptr<TPackage> package,
                                           std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                                           std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper)
    : m_package(std::move(package))
    , m_filesystemHelper(std::move(filesystemHelper))
{
    auto pool = std::make_shared<host::TDownloadPool>(std::move(networkAccessManager), 6);
    m_downloader = new TPackageFileDownloader(pool, m_filesystemHelper);
    connect(m_downloader, &TPackageFileDownloader::packageMetaDataReceived, this,
            &TPackageInstallation::onPackageMetaDataReceived);
    connect(m_downloader, &TPackageFileDownloader::assetMetaDataReceived, this,
            &TPackageInstallation::onAssetMetaDataReceived);
    connect(m_downloader, &TPackageFileDownloader::downloadCompleted, this, &TPackageInstallation::onDownloadCompleted);
    connect(m_downloader, &TPackageFileDownloader::downloadFailed, this, &TPackageInstallation::onDownloadFailed);
    m_downloader->setTargetDirectory(TLauncherFilesystemHelper::packageDirectory(*m_package));
}

TPackageInstallation::~TPackageInstallation()
{
    if (m_downloader) {
        m_downloader->deleteLater();
        m_downloader = nullptr;
    }
}

void TPackageInstallation::start()
{
    const TPackageBuild* build = m_package->buildForCurrentPlatform();
    if (!needsPackageMetaData(*m_package)) {
        m_packageMetaData = TLauncherFilesystemHelper::localPackageMetaData(*m_package);
    } else {
        qDebug() << "downloading package meta data for package " << m_package->name() << " from " << build->url;
        m_downloader->downloadPackageMetaData(build->url);
        m_waitingForPackageMetaData = true;
    }
    if (!needsAssetMetaData(*m_package)) {
        m_assetMetaData = TLauncherFilesystemHelper::localAssetMetaData(*m_package);
        if (!m_waitingForPackageMetaData && !m_waitingForAssetMetaData)
            Q_EMIT metaDataDownloadCompleted(m_package, m_packageMetaData, m_assetMetaData);
    } else {
        qDebug() << "downloading asset meta data for package " << m_package->name() << " from " << build->assetsUrl;
        m_downloader->downloadAssetMetaData(build->assetsUrl);
        m_waitingForAssetMetaData = true;
    }
}

void TPackageInstallation::onPackageMetaDataReceived(QByteArray data)
{
    m_packageMetaData = data;
    m_waitingForPackageMetaData = false;
    if (!m_waitingForAssetMetaData)
        Q_EMIT metaDataDownloadCompleted(m_package, m_packageMetaData, m_assetMetaData);
}

void TPackageInstallation::onAssetMetaDataReceived(QByteArray data)
{
    m_assetMetaData = data;
    m_waitingForAssetMetaData = false;
    if (!m_waitingForPackageMetaData)
        Q_EMIT metaDataDownloadCompleted(m_package, m_packageMetaData, m_assetMetaData);
}

void TPackageInstallation::onDownloadCompleted()
{
    // Downloads are in place; create the links the configuration asks for.
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(*m_package);
    bool linkFailed = false;
    for (const auto& [target, linkPaths] : m_links) {
        const QString targetPath = directory.filePath(target);
        for (const QString& linkPath : linkPaths) {
            const QString path = directory.filePath(linkPath);
            TLauncherFilesystemHelper::removeFile(path);
            linkFailed |= !QFile::link(targetPath, path);
        }
    }
    m_succeeded = !linkFailed;
    m_errorReason = linkFailed ? DiskAccessFailed : NoError;
    Q_EMIT installationFinished(m_package);
}

void TPackageInstallation::onDownloadFailed(EPackageDownloaderError errorReason)
{
    m_succeeded = false;
    m_errorReason = errorReason;
    Q_EMIT installationFinished(m_package);
}

void TPackageInstallation::setVersions(QString& installedVersion, QString& targetVersion)
{
    std::swap(m_installedVersion, installedVersion);
    std::swap(m_targetVersion, targetVersion);
}

void TPackageInstallation::setFilesToDownload(const std::vector<TFileInPackage>& files)
{
    m_filesToDownload = files;
}

void TPackageInstallation::setFilesToRemove(const std::vector<TFileInPackage>& files)
{
    m_filesToRemove = files;
}

void TPackageInstallation::setLinks(const std::map<QString, QStringList>& links)
{
    m_links = links;
}

void TPackageInstallation::startDownload()
{
    removeFiles();
    downloadFiles();
}

void TPackageInstallation::removeFiles()
{
    const QDir directory = TLauncherFilesystemHelper::packageDirectory(*m_package);
    for (const TFileInPackage& file : m_filesToRemove)
        TLauncherFilesystemHelper::removeFile(directory.filePath(file.localFile));
}

void TPackageInstallation::downloadFiles()
{
    if (m_filesToDownload.empty()) {
        m_succeeded = true;
        m_errorReason = NoError;
        Q_EMIT installationFinished(m_package);
        return;
    }
    m_downloader->setUpdateMode(m_updateMode);
    m_downloader->setFiles(m_filesToDownload);
    m_downloader->start();
    qInfo() << "Starting download of" << m_filesToDownload.size() << "files for package" << m_package->name();
    for (const TFileInPackage& file : m_filesToDownload)
        qInfo() << "Queued file for download:" << file.url.toString();
}

} // namespace launcher
