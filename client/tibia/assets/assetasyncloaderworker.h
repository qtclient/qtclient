#pragma once

#include <QObject>

#include <memory>

namespace tibia {
namespace assets {

class TAssetAsyncLoaderWorkQueue;
class TAssetFileSystemHelper;

/// Loads queued assets on its thread until the loader stops (CIP assets::TAssetAsyncLoaderWorker).
class TAssetAsyncLoaderWorker : public QObject
{
    Q_OBJECT
public:
    TAssetAsyncLoaderWorker(std::shared_ptr<TAssetAsyncLoaderWorkQueue> queue,
                            std::shared_ptr<TAssetFileSystemHelper> fileSystemHelper, std::shared_ptr<bool> stopped);

Q_SIGNALS:
    void assetLoaded();

public Q_SLOTS:
    /// Runs until the loader stops: a prioritized asset raises the thread priority.
    void startLoading();

private:
    std::shared_ptr<TAssetAsyncLoaderWorkQueue> m_queue;
    std::shared_ptr<TAssetFileSystemHelper> m_fileSystemHelper;
    std::shared_ptr<bool> m_stopped;
};

}
}
