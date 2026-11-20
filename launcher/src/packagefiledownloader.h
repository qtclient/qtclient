#pragma once

#include "package.h"

#include <QDir>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>
#include <QUrl>

#include <map>
#include <memory>
#include <vector>

namespace tibia::shared {
class TDownloadPool;
}

namespace launcher {

class TLauncherFilesystemHelper;

using TFileSize = qint64;

enum EPackageDownloaderError { NoError = 0, DownloadFailed = 1, DiskAccessFailed = 2 };

/// Downloads the files of one package installation (and its package.json /
/// assets.json) through the download pool, unpacks and stores them.
class TPackageFileDownloader : public QObject
{
    Q_OBJECT
public:
    TPackageFileDownloader(std::shared_ptr<tibia::shared::TDownloadPool> downloadPool,
                           std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper);
    ~TPackageFileDownloader() override;

    void setTargetDirectory(const QDir& directory) { m_targetDirectory = directory; }
    void setUpdateMode(int updateMode) { m_updateMode = updateMode; }
    void setFiles(const std::vector<TFileInPackage>& files) { m_files = files; }
    void start();
    void downloadPackageMetaData(const QUrl& url);
    void downloadAssetMetaData(const QUrl& url);
    /// Host: downloads the manifest of a host channel (mods / minimap).
    void downloadHostChannelMetaData(const QUrl& url);

    /// True once at least one speed sample was taken.
    bool hasDownloadSpeed() const { return m_speedSamples > 0; }
    /// Average download speed in bytes per measuring interval.
    quint64 downloadSpeed() const { return static_cast<quint64>(m_averageSpeed); }

Q_SIGNALS:
    void packageMetaDataReceived(QByteArray data);
    void assetMetaDataReceived(QByteArray data);
    void hostChannelMetaDataReceived(QUrl url, QByteArray data);
    void hostChannelMetaDataFailed(QUrl url);
    void downloadCompleted();
    void downloadProgress(launcher::TFileSize bytesDownloaded, launcher::TFileSize bytesTotal);
    void downloadFailed(launcher::EPackageDownloaderError errorReason);

public Q_SLOTS:
    void onDownloadCompleted(const QUrl& url, const QByteArray& data);
    void onDownloadFailed(const QUrl& url, QNetworkReply::NetworkError code, const QString& errorString);
    void onDownloadProgress(const QUrl& url, launcher::TFileSize downloadedBytes);
    void onMeasureDownloadSpeed();

private:
    using TFileIterator = std::vector<TFileInPackage>::iterator;
    /// Pending file downloaded from `url` (pending files are sorted by url), or end().
    TFileIterator findPendingFile(const QUrl& url);
    void handleDownloadedFile(const QUrl& url, const QByteArray& data, bool unpacked);
    void saveFile(const TFileInPackage& file, const QByteArray& data);
    TFileSize downloadedBytes() const;
    TFileSize totalBytes() const;
    void checkFinished();

    std::shared_ptr<tibia::shared::TDownloadPool> m_downloadPool;
    std::shared_ptr<TLauncherFilesystemHelper> m_filesystemHelper;
    QDir m_targetDirectory;
    bool m_downloadFailed = false;
    bool m_diskAccessFailed = false;
    int m_updateMode = 0;
    std::vector<TFileInPackage> m_files;
    std::vector<TFileInPackage> m_pendingFiles;
    std::vector<TFileInPackage> m_completedFiles;
    std::map<QUrl, TFileSize> m_downloadedBytes;
    QTimer m_speedTimer;
    TFileSize m_lastDownloadedBytes = 0;
    quint64 m_speedSamples = 0;
    quint64 m_minimumSpeed = ~0ULL;
    quint64 m_maximumSpeed = 0;
    double m_averageSpeed = 0.0;
    QUrl m_packageMetaDataUrl;
    QUrl m_assetMetaDataUrl;
    std::vector<QUrl> m_hostChannelMetaDataUrls;
};

} // namespace launcher
