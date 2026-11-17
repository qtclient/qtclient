#include "packagefiledownloader.h"

#include "downloadpool.h"
#include "qnetworkaccessmanagerwrapper.h"
#include "launcherfilesystemhelper.h"
#include "unpack.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace launcher {

namespace {

bool urlLess(const TFileInPackage& file, const QUrl& url)
{
    return file.url.toString().compare(url.toString()) < 0;
}

} // namespace

TPackageFileDownloader::TPackageFileDownloader(std::shared_ptr<tibia::shared::TDownloadPool> downloadPool,
                                               std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper)
    : m_downloadPool(std::move(downloadPool))
    , m_filesystemHelper(std::move(filesystemHelper))
{
    m_speedTimer.setInterval(1000);
    connect(m_downloadPool.get(), &tibia::shared::TDownloadPool::success, this,
            &TPackageFileDownloader::onDownloadCompleted);
    connect(m_downloadPool.get(), &tibia::shared::TDownloadPool::failed, this,
            &TPackageFileDownloader::onDownloadFailed);
    connect(m_downloadPool.get(), &tibia::shared::TDownloadPool::progress, this,
            &TPackageFileDownloader::onDownloadProgress);
    connect(&m_speedTimer, &QTimer::timeout, this, &TPackageFileDownloader::onMeasureDownloadSpeed);
}

TPackageFileDownloader::~TPackageFileDownloader() = default;

void TPackageFileDownloader::start()
{
    m_pendingFiles = m_files;
    // Host: content-addressed store urls. Equal blobs share a store url, so a
    // fragment (not sent to the server) keeps every pending file's url unique.
    QSet<QUrl> seen;
    for (TFileInPackage& file : m_pendingFiles) {
        QUrl url = m_filesystemHelper->fileDownloadUrl(file);
        if (url != file.url && seen.contains(url))
            url.setFragment(file.localFile);
        seen.insert(url);
        file.url = url;
    }
    std::sort(m_pendingFiles.begin(), m_pendingFiles.end(), [](const TFileInPackage& a, const TFileInPackage& b) {
        return a.url.toString().compare(b.url.toString()) < 0;
    });
    m_downloadFailed = false;
    m_diskAccessFailed = false;
    m_downloadedBytes.clear();
    m_speedTimer.start();
    m_lastDownloadedBytes = 0;
    m_speedSamples = 0;
    for (const TFileInPackage& file : m_pendingFiles)
        m_downloadPool->download(file.url, m_filesystemHelper->retries(), m_filesystemHelper->chunkTransportTimeout());
}

void TPackageFileDownloader::downloadPackageMetaData(const QUrl& url)
{
    m_packageMetaDataUrl = url;
    m_downloadPool->download(url, m_filesystemHelper->retries(), m_filesystemHelper->configurationTransportTimeout());
}

void TPackageFileDownloader::downloadAssetMetaData(const QUrl& url)
{
    m_assetMetaDataUrl = url;
    m_downloadPool->download(url, m_filesystemHelper->retries(), m_filesystemHelper->configurationTransportTimeout());
}

void TPackageFileDownloader::downloadHostChannelMetaData(const QUrl& url)
{
    m_hostChannelMetaDataUrls.push_back(url);
    m_downloadPool->download(url, m_filesystemHelper->retries(), m_filesystemHelper->configurationTransportTimeout());
}

TPackageFileDownloader::TFileIterator TPackageFileDownloader::findPendingFile(const QUrl& url)
{
    const auto file = std::lower_bound(m_pendingFiles.begin(), m_pendingFiles.end(), url, urlLess);
    if (file != m_pendingFiles.end() && file->url == url)
        return file;
    return m_pendingFiles.end();
}

void TPackageFileDownloader::onDownloadCompleted(const QUrl& url, const QByteArray& data)
{
    handleDownloadedFile(url, data, false);
}

void TPackageFileDownloader::handleDownloadedFile(const QUrl& url, const QByteArray& data, bool unpacked)
{
    if (url == m_packageMetaDataUrl) {
        Q_EMIT packageMetaDataReceived(data);
        return;
    }
    if (url == m_assetMetaDataUrl) {
        Q_EMIT assetMetaDataReceived(data);
        return;
    }
    if (const auto channel = std::find(m_hostChannelMetaDataUrls.begin(), m_hostChannelMetaDataUrls.end(), url);
        channel != m_hostChannelMetaDataUrls.end()) {
        m_hostChannelMetaDataUrls.erase(channel);
        Q_EMIT hostChannelMetaDataReceived(url, data);
        return;
    }
    const auto file = findPendingFile(url);
    if (file == m_pendingFiles.end())
        return;
    // Host: unpack and write on a pool thread; the main thread only books the
    // result. On /play every file-system call from the main thread is a round
    // trip to the OPFS worker that suspends the event loop, and so every
    // download, for its duration (mkpath + QSaveFile is a dozen of them per
    // file); a pool thread blocks on it cheaply instead.
    enum class TSaved { Ok, UnpackFailed, WriteFailed };
    auto* watcher = new QFutureWatcher<TSaved>();
    connect(watcher, &QFutureWatcher<TSaved>::finished, this, [this, watcher, url]() {
        watcher->deleteLater();
        const TSaved saved = watcher->isCanceled() ? TSaved::UnpackFailed : watcher->result();
        const auto done = findPendingFile(url);
        if (done == m_pendingFiles.end())
            return;
        if (saved == TSaved::UnpackFailed) {
            qWarning() << "Failed to decompress file downloaded from" << url.toString();
            m_pendingFiles.erase(done);
            m_downloadFailed = true;
            checkFinished();
            return;
        }
        if (saved == TSaved::WriteFailed)
            m_diskAccessFailed = true;
        m_completedFiles.push_back(*done);
        m_pendingFiles.erase(done);
        checkFinished();
    });
    const bool unpack = !unpacked && file->unpack;
    watcher->setFuture(QtConcurrent::run(
        [file = *file, data, unpack, url, targetDirectory = m_targetDirectory, updateMode = m_updateMode]() {
            QByteArray output;
            if (unpack) {
                QString error;
                if (!unpackLzmaAlone(data, output, &error))
                    return TSaved::UnpackFailed;
            } else {
                output = data;
            }
            qInfo() << "Downloading" << url.toString() << "was finished, now saving to disk.";
            return saveFile(targetDirectory, updateMode, file, output) ? TSaved::Ok : TSaved::WriteFailed;
        }));
}

bool TPackageFileDownloader::saveFile(const QDir& targetDirectory, int updateMode, const TFileInPackage& file,
                                      const QByteArray& data)
{
    const QString path = targetDirectory.filePath(file.localFile);
    // A self-update keeps the replaced file as "<file>.old" until the next start.
    if (updateMode == 1 && TLauncherFilesystemHelper::fileExists(path))
        TLauncherFilesystemHelper::renameFile(path, path + TLauncherFilesystemHelper::backupFileSuffix());
    if (!TLauncherFilesystemHelper::writeFile(path, data)) {
        qCritical() << "Writing" << path << "failed.";
        return false;
    }
    if (file.executable) {
        QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner | QFileDevice::ExeGroup
                                        | QFileDevice::ExeOther);
    }
    return true;
}

void TPackageFileDownloader::onDownloadFailed(const QUrl& url, QNetworkReply::NetworkError, const QString& errorString)
{
    if (const auto channel = std::find(m_hostChannelMetaDataUrls.begin(), m_hostChannelMetaDataUrls.end(), url);
        channel != m_hostChannelMetaDataUrls.end()) {
        qCritical() << "Download of" << url.toString() << "failed:" << errorString;
        m_hostChannelMetaDataUrls.erase(channel);
        Q_EMIT hostChannelMetaDataFailed(url);
        return;
    }
    const auto file = findPendingFile(url);
    if (file == m_pendingFiles.end())
        return;
    qCritical() << "Download of" << url.toString() << "failed:" << errorString;
    m_pendingFiles.erase(file);
    m_downloadFailed = true;
    checkFinished();
}

TFileSize TPackageFileDownloader::downloadedBytes() const
{
    TFileSize sum = 0;
    for (const auto& [url, bytes] : m_downloadedBytes)
        sum += bytes;
    return sum;
}

TFileSize TPackageFileDownloader::totalBytes() const
{
    TFileSize total = 0;
    for (const TFileInPackage& file : m_files)
        total += file.packedSize;
    return total;
}

void TPackageFileDownloader::onDownloadProgress(const QUrl& url, TFileSize bytes)
{
    if (findPendingFile(url) == m_pendingFiles.end())
        return;
    m_downloadedBytes[url] = bytes;
    Q_EMIT downloadProgress(downloadedBytes(), totalBytes());
}

void TPackageFileDownloader::onMeasureDownloadSpeed()
{
    const TFileSize downloaded = downloadedBytes();
    const quint64 sample = static_cast<quint64>(downloaded - m_lastDownloadedBytes);
    m_lastDownloadedBytes = downloaded;
    if (m_speedSamples == 0) {
        m_averageSpeed = static_cast<double>(sample);
        m_minimumSpeed = sample;
        m_maximumSpeed = sample;
    } else {
        const quint64 samples = m_speedSamples + 1;
        m_averageSpeed = (static_cast<double>(m_speedSamples) * m_averageSpeed + static_cast<double>(sample))
                         / static_cast<double>(samples);
        m_minimumSpeed = std::min(m_minimumSpeed, sample);
        m_maximumSpeed = std::max(m_maximumSpeed, sample);
        m_speedSamples = samples;
        if (samples < 31)
            return;
        // Restart the window from the current average.
        const quint64 average = static_cast<quint64>(m_averageSpeed);
        m_averageSpeed = static_cast<double>(average);
        m_minimumSpeed = average;
        m_maximumSpeed = average;
    }
    m_speedSamples = 1;
}

void TPackageFileDownloader::checkFinished()
{
    if (!m_pendingFiles.empty())
        return;
    m_speedSamples = 0;
    m_speedTimer.stop();
    Q_EMIT downloadProgress(downloadedBytes(), totalBytes());
    if (m_downloadFailed)
        Q_EMIT downloadFailed(DownloadFailed);
    else if (m_diskAccessFailed)
        Q_EMIT downloadFailed(DiskAccessFailed);
    else
        Q_EMIT downloadCompleted();
}

} // namespace launcher
