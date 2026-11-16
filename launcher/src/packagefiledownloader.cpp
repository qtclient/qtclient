#include "packagefiledownloader.h"

#include "host/downloadpool.h"
#include "launcherfilesystemhelper.h"
#include "unpack.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
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

TPackageFileDownloader::TPackageFileDownloader(std::shared_ptr<host::TDownloadPool> downloadPool,
                                               std::shared_ptr<TLauncherFilesystemHelper> filesystemHelper)
    : m_downloadPool(std::move(downloadPool))
    , m_filesystemHelper(std::move(filesystemHelper))
{
    m_speedTimer.setInterval(1000);
    connect(m_downloadPool.get(), &host::TDownloadPool::downloadSucceeded, this,
            &TPackageFileDownloader::onDownloadCompleted);
    connect(m_downloadPool.get(), &host::TDownloadPool::downloadFailed, this,
            &TPackageFileDownloader::onDownloadFailed);
    connect(m_downloadPool.get(), &host::TDownloadPool::downloadProgress, this,
            &TPackageFileDownloader::onDownloadProgress);
    connect(&m_speedTimer, &QTimer::timeout, this, &TPackageFileDownloader::onMeasureDownloadSpeed);
}

TPackageFileDownloader::~TPackageFileDownloader() = default;

void TPackageFileDownloader::start()
{
    m_pendingFiles = m_files;
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
    const auto file = findPendingFile(url);
    if (file == m_pendingFiles.end())
        return;
    if (!unpacked && file->unpack) {
        // Unpack off the main thread, then come back here with the result.
        using TResult = std::pair<bool, QByteArray>;
        auto* watcher = new QFutureWatcher<TResult>();
        connect(watcher, &QFutureWatcher<TResult>::finished, this, [this, watcher, url]() {
            watcher->deleteLater();
            const TResult result = watcher->isCanceled() ? TResult{false, {}} : watcher->result();
            if (result.first) {
                handleDownloadedFile(url, result.second, true);
                return;
            }
            qWarning() << "Failed to decompress file downloaded from" << url.toString();
            const auto failed = findPendingFile(url);
            if (failed != m_pendingFiles.end())
                m_pendingFiles.erase(failed);
            m_downloadFailed = true;
            checkFinished();
        });
        watcher->setFuture(QtConcurrent::run([data]() {
            QByteArray output;
            QString error;
            const bool ok = unpackLzmaAlone(data, output, &error);
            return std::pair<bool, QByteArray>(ok, output);
        }));
        return;
    }
    qInfo() << "Downloading" << url.toString() << "was finished, now saving to disk.";
    saveFile(*file, data);
    m_completedFiles.push_back(*file);
    m_pendingFiles.erase(file);
    checkFinished();
}

void TPackageFileDownloader::saveFile(const TFileInPackage& file, const QByteArray& data)
{
    const QString path = m_targetDirectory.filePath(file.localFile);
    if (!TLauncherFilesystemHelper::writeFile(path, data)) {
        qCritical() << "Writing" << path << "failed.";
        m_diskAccessFailed = true;
        return;
    }
    if (file.executable) {
        QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner | QFileDevice::ExeGroup
                                        | QFileDevice::ExeOther);
    }
}

void TPackageFileDownloader::onDownloadFailed(const QUrl& url, QNetworkReply::NetworkError, const QString& errorString)
{
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
