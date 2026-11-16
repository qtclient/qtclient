#pragma once

#include "packagefiledownloader.h"

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <map>
#include <memory>
#include <vector>

class QNetworkAccessManager;

namespace launcher {

class TPackage;
class TLauncherFilesystemHelper;

/// Files to fetch, files to delete and links to create to bring a package
/// from its installed configuration to the current one.
struct TPackageDifference {
    std::vector<TFileInPackage> filesToDownload;
    std::vector<TFileInPackage> filesToRemove;
    /// Link target → paths that link to it.
    std::map<QString, QStringList> links;
};

/// One running installation or update of a package.
class TPackageInstallation : public QObject
{
    Q_OBJECT
public:
    TPackageInstallation(std::shared_ptr<TPackage> package, std::shared_ptr<QNetworkAccessManager> networkAccessManager,
                         std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper);
    ~TPackageInstallation() override;

    const std::shared_ptr<TPackage>& package() const { return m_package; }
    TPackageFileDownloader* downloader() const { return m_downloader; }
    bool succeeded() const { return m_succeeded; }
    EPackageDownloaderError errorReason() const { return m_errorReason; }

    /// Fetches (or reads the installed) package.json / assets.json of the build.
    void start();
    void setUpdateMode(int updateMode) { m_updateMode = updateMode; }
    void setVersions(QString& installedVersion, QString& targetVersion);
    void setFilesToDownload(const std::vector<TFileInPackage>& files);
    void setFilesToRemove(const std::vector<TFileInPackage>& files);
    void setLinks(const std::map<QString, QStringList>& links);
    /// Deletes obsolete files, then downloads the missing ones.
    void startDownload();

Q_SIGNALS:
    void metaDataDownloadCompleted(std::shared_ptr<launcher::TPackage> package, QByteArray packageMetaData,
                                   QByteArray assetMetaData);
    void installationFinished(std::shared_ptr<launcher::TPackage> packageBeingInstalled);

public Q_SLOTS:
    void onPackageMetaDataReceived(QByteArray data);
    void onAssetMetaDataReceived(QByteArray data);
    void onDownloadCompleted();
    void onDownloadFailed(launcher::EPackageDownloaderError errorReason);
    void onHostChannelMetaDataReceived(QUrl url, QByteArray data);
    void onHostChannelMetaDataFailed(QUrl url);

private:
    /// Emits metaDataDownloadCompleted once package, assets and host channel
    /// manifests are in (channel files merged into the assets).
    void emitMetaDataIfComplete();
    void removeFiles();
    void downloadFiles();

    std::shared_ptr<TPackage> m_package;
    TPackageFileDownloader* m_downloader = nullptr;
    std::shared_ptr<TLauncherFilesystemHelper> m_filesystemHelper;
    bool m_succeeded = false;
    EPackageDownloaderError m_errorReason = NoError;
    int m_updateMode = 0;
    QString m_installedVersion;
    QString m_targetVersion;
    std::vector<TFileInPackage> m_filesToRemove;
    std::vector<TFileInPackage> m_filesToDownload;
    std::map<QString, QStringList> m_links;
    bool m_waitingForPackageMetaData = false;
    QByteArray m_packageMetaData;
    bool m_waitingForAssetMetaData = false;
    QByteArray m_assetMetaData;
    int m_waitingForHostChannels = 0;
    std::vector<std::pair<QUrl, QByteArray>> m_hostChannelManifests;
};

} // namespace launcher
