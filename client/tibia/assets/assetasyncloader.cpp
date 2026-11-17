#include "assets/assetasyncloader.h"

#include "assets/assetasyncloaderworker.h"
#include "assets/assetfilesystemhelper.h"

#include <QDebug>
#include <QMutexLocker>
#include <QThread>

#include <algorithm>

namespace tibia::assets {

bool TAssetAsyncLoaderWorkQueue::add(const TAsset& asset, const bool prioritized)
{
    const QMutexLocker lock(&m_assetsMutex);
    bool added = true;
    if (!prioritized) {
        if (m_assets.contains(asset.file())) {
            qDebug() << "Duplicate Asset File detected: " << asset.file();
            added = false;
        } else {
            m_assets[asset.file()] = asset;
        }
    } else {
        m_prioritizedAssets.push_back(asset);
    }
    m_assetsAvailable.wakeAll();
    return added;
}

bool TAssetAsyncLoaderWorkQueue::takeNext(TAsset& asset, bool& prioritized, const bool wait)
{
    QMutexLocker lock(&m_assetsMutex);
    if (m_assets.empty() && wait)
        m_assetsAvailable.wait(&m_assetsMutex);
    if (m_prioritizedAssets.empty()) {
        if (m_assets.empty())
            return false;
        const auto first = m_assets.begin();
        asset = first->second;
        m_assets.erase(first);
        prioritized = false;
        return true;
    }
    asset = m_prioritizedAssets.front();
    m_prioritizedAssets.pop_front();
    prioritized = true;
    return true;
}

void TAssetAsyncLoaderWorkQueue::prioritize(const sprites::TSpritesheetID& spritesheetId)
{
    const QMutexLocker lock(&m_assetsMutex);
    const auto it = m_assets.find(spritesheetId);
    if (it == m_assets.end())
        return;
    m_prioritizedAssets.push_back(it->second);
    m_assets.erase(it);
}

void TAssetAsyncLoaderWorkQueue::clear()
{
    const QMutexLocker lock(&m_assetsMutex);
    m_assets.clear();
    m_assetsAvailable.wakeAll();
}

void TAssetAsyncLoaderWorkQueue::addResult(TAssetLoadResult result)
{
    const QMutexLocker lock(&m_resultsMutex);
    m_results.push_back(std::move(result));
}

bool TAssetAsyncLoaderWorkQueue::takeResult(TAssetLoadResult& result)
{
    const QMutexLocker lock(&m_resultsMutex);
    if (m_results.empty())
        return false;
    result = std::move(m_results.front());
    m_results.pop_front();
    return true;
}

namespace {

size_t workerThreadCount()
{
#if defined(__EMSCRIPTEN__)
    // Host: the browser runtime cannot start extra Qt threads (see load()).
    return 0;
#else
    static const size_t idealThreadCount = static_cast<size_t>(std::max(0, QThread::idealThreadCount()));
    static const size_t count = idealThreadCount == 0 ? 1 : std::min<size_t>(4, idealThreadCount);
    return count;
#endif
}

}

TAssetAsyncLoader::TAssetAsyncLoader(std::shared_ptr<TAssetFileSystemHelper> fileSystemHelper)
    : m_fileSystemHelper(std::move(fileSystemHelper))
    , m_queue(std::make_shared<TAssetAsyncLoaderWorkQueue>())
    , m_stopped(std::make_shared<bool>(false))
{
    for (size_t i = 0; i < workerThreadCount(); ++i)
        startWorkerThread();
}

TAssetAsyncLoader::~TAssetAsyncLoader()
{
    stop();
    for (QThread* thread : m_threads) {
        thread->quit();
        thread->wait();
        thread->deleteLater();
    }
}

void TAssetAsyncLoader::startWorkerThread()
{
    auto* thread = new QThread;
    auto* worker = new TAssetAsyncLoaderWorker(m_queue, m_fileSystemHelper, m_stopped);
    worker->moveToThread(thread);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::started, worker, &TAssetAsyncLoaderWorker::startLoading);
    connect(worker, &TAssetAsyncLoaderWorker::assetLoaded, this, &TAssetAsyncLoader::onWorkerThreadAssetLoaded);
    m_threads.push_back(thread);
    thread->start(QThread::NormalPriority);
}

bool TAssetAsyncLoader::load(const TAsset& asset, const bool prioritized)
{
    const bool queued = m_queue->add(asset, prioritized);
#if defined(__EMSCRIPTEN__)
    // Host: without worker threads the browser build loads on this thread, one
    // asset per event loop pass so the page stays responsive.
    if (queued || prioritized) {
        QMetaObject::invokeMethod(this, [this] {
            TAsset next;
            bool nextPrioritized = false;
            if (*m_stopped || !m_queue->takeNext(next, nextPrioritized, false))
                return;
            TAssetLoadResult result;
            result.asset = next;
            try {
                result.data = m_fileSystemHelper->openAsset(next);
            } catch (const std::exception& error) {
                result.error = error.what();
            }
            m_queue->addResult(std::move(result));
            onWorkerThreadAssetLoaded();
        }, Qt::QueuedConnection);
    }
#endif
    return queued;
}

void TAssetAsyncLoader::stop()
{
    *m_stopped = true;
    m_queue->clear();
}

void TAssetAsyncLoader::onWorkerThreadAssetLoaded()
{
    TAssetLoadResult result;
    while (m_queue->takeResult(result)) {
        if (result.error.empty())
            Q_EMIT assetLoaded(result.asset, result.data);
        else if (result.asset.isLoadErrorReported())
            Q_EMIT assetLoadError(result.asset, result.error);
    }
}

void TAssetAsyncLoader::priorityLoadOfSpritesheet(const sprites::TSpritesheetID& spritesheetId)
{
    m_queue->prioritize(spritesheetId);
}

}
