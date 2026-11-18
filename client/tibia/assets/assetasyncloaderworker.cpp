#include "assets/assetasyncloaderworker.h"

#include "assets/assetasyncloader.h"
#include "assets/assetfilesystemhelper.h"

#include <QThread>

#include <exception>

namespace tibia::assets {

TAssetAsyncLoaderWorker::TAssetAsyncLoaderWorker(std::shared_ptr<TAssetAsyncLoaderWorkQueue> queue,
                                                 std::shared_ptr<TAssetFileSystemHelper> fileSystemHelper,
                                                 std::shared_ptr<bool> stopped)
    : m_queue(std::move(queue))
    , m_fileSystemHelper(std::move(fileSystemHelper))
    , m_stopped(std::move(stopped))
{
}

void TAssetAsyncLoaderWorker::startLoading()
{
    while (!*m_stopped) {
        TAsset asset;
        bool prioritized = false;
        if (!m_queue->takeNext(asset, prioritized))
            continue;
        QThread::currentThread()->setPriority(prioritized ? QThread::HighPriority : QThread::NormalPriority);
        TAssetLoadResult result;
        result.asset = asset;
        try {
            result.data = m_fileSystemHelper->openAsset(asset);
        } catch (const std::exception& error) {
            result.error = error.what();
        }
        m_queue->addResult(std::move(result));
        Q_EMIT assetLoaded();
    }
}

}
