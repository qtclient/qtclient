#pragma once

#include "assets/asset.h"
#include "sprites/spritesheetsmetadatamanager.h"

#include <QMutex>
#include <QObject>
#include <QWaitCondition>

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class QThread;

namespace tibia::shared {
class TReadOnlyMemoryBlob;
}

namespace tibia {
namespace assets {

class TAssetFileSystemHelper;

/// One loaded asset: its bytes, or why they could not be read.
struct TAssetLoadResult
{
    TAsset asset;
    std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> data;
    std::string error;
};

/// The assets waiting to be loaded and the loaded ones waiting to be handed out,
/// shared by the loader and its worker threads (CIP assets::TAssetAsyncLoaderWorkQueue).
class TAssetAsyncLoaderWorkQueue
{
public:
    /// Queues an asset: a prioritized one goes to the priority queue; a second
    /// asset with the same file is dropped (false).
    bool add(const TAsset& asset, bool prioritized);
    /// Blocks until an asset waits; prioritized ones first. False once the queue was
    /// cleared with nothing left. `prioritized` tells where the asset came from.
    /// Without `wait` it returns false at once when nothing waits (host: browser build).
    bool takeNext(TAsset& asset, bool& prioritized, bool wait = true);
    /// Moves a waiting spritesheet to the priority queue.
    void prioritize(const sprites::TSpritesheetID& spritesheetId);
    /// Drops every waiting asset and wakes the workers.
    void clear();
    void addResult(TAssetLoadResult result);
    bool takeResult(TAssetLoadResult& result);

private:
    struct Hash
    {
        size_t operator()(const QString& key) const noexcept { return qHash(key); }
    };

    std::unordered_map<QString, TAsset, Hash> m_assets;
    std::deque<TAsset> m_prioritizedAssets;
    std::deque<TAssetLoadResult> m_results;
    QMutex m_assetsMutex;
    QMutex m_resultsMutex;
    QWaitCondition m_assetsAvailable;
};

/// Loads asset files on worker threads (CIP assets::TAssetAsyncLoader): min(4, ideal
/// thread count) workers take assets from a shared queue; finished loads are handed
/// out on this object's thread.
class TAssetAsyncLoader : public QObject
{
    Q_OBJECT
public:
    explicit TAssetAsyncLoader(std::shared_ptr<TAssetFileSystemHelper> fileSystemHelper);
    ~TAssetAsyncLoader() override;

    /// Queues an asset; false for a second asset with the same file.
    virtual bool load(const TAsset& asset, bool prioritized);
    /// Stops the workers once their current asset is loaded; waiting assets are dropped.
    virtual void stop();

Q_SIGNALS:
    void assetLoaded(const tibia::assets::TAsset& Asset, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& MemoryBlob);
    void assetLoadError(const tibia::assets::TAsset& Asset, const std::string& ErrorText);

public Q_SLOTS:
    void onWorkerThreadAssetLoaded();
    void priorityLoadOfSpritesheet(const tibia::sprites::TSpritesheetID& SpritesheetID);

private:
    void startWorkerThread();

    std::shared_ptr<TAssetFileSystemHelper> m_fileSystemHelper;
    std::vector<QThread*> m_threads;
    std::shared_ptr<TAssetAsyncLoaderWorkQueue> m_queue;
    std::shared_ptr<bool> m_stopped;
};

}
}
