#pragma once

#include "assets/asset.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <cstdint>
#include <memory>
#include <string>

namespace tibia::shared {
class TReadOnlyMemoryBlob;

/// How far a job is: `value` of `maximum` (CIP shared::NormalizedProgress).
struct NormalizedProgress
{
    int32_t value = 0;
    int32_t maximum = 0;

    bool operator==(const NormalizedProgress& other) const = default;
};
}

namespace tibia::sprites {
class TSpritesheetsMetadataManager;
}

namespace tibia {
namespace assets {

class TAssetAsyncLoader;
class TContentCatalog;
class TSoundCatalog;

/// Drives the loading of the client's assets (CIP assets::TAssetManager): it
/// first loads the content and sound catalogs, then queues every asset they
/// list (appearances and sounds first) and hands each loaded asset on through
/// assetDataAccessible until all queued assets have arrived.
class TAssetManager : public QObject
{
    Q_OBJECT
public:
    /// Where the asset loading is.
    enum EState
    {
        /// Not started.
        Initial = 0,
        /// The two catalogs are being loaded.
        LoadingCatalogs = 1,
        /// The appearances asset has arrived.
        AppearancesLoaded = 2,
        /// Both catalogs are read; their assets are queued.
        CatalogsLoaded = 3,
        /// Every queued asset has arrived.
        Complete = 4,
    };

    TAssetManager(const QString& name, std::shared_ptr<TAssetAsyncLoader> loader, QObject* parent);

    /// Queues the content and sound catalogs (prioritized). Throws
    /// std::runtime_error unless the manager is still in its initial state.
    void startLoading();

    EState state() const { return m_state; }
    const std::shared_ptr<TContentCatalog>& contentCatalog() const { return m_contentCatalog; }
    const std::shared_ptr<TSoundCatalog>& soundCatalog() const { return m_soundCatalog; }
    /// Assets queued after the catalogs were read (including additional ones).
    size_t queuedAssetCount() const { return m_queuedAssetCount; }
    /// The files of the assets that arrived after the catalogs were read.
    const QSet<QString>& loadedAssets() const { return m_loadedAssets; }

Q_SIGNALS:
    void error(QString reason);
    void normalizedProgress(tibia::shared::NormalizedProgress Progress);
    void spritesheetMetadataComplete(std::shared_ptr<tibia::sprites::TSpritesheetsMetadataManager> pSpritesheetsMetadataManager);
    void assetDataAccessible(tibia::assets::TAsset Asset, std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> pData);
    void assetsComplete();

public Q_SLOTS:
    /// Queues one more asset; it counts towards completion when the loader takes it.
    void onNewAdditionalAssetToLoad(const tibia::assets::TAsset& AssetToLoad, bool Prioritized);
    void assetLoaded(const tibia::assets::TAsset& Asset, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& ReadOnlyMemoryBlob);
    void assetLoadError(const tibia::assets::TAsset& Asset, const std::string& ErrorText);

private:
    /// Logs the message and emits error.
    void reportError(const QString& message);
    /// (0, 1) until the appearances have arrived, then (loaded assets, content catalog assets).
    tibia::shared::NormalizedProgress progress() const;
    /// Every queued asset has arrived: Complete, assetsComplete.
    void checkComplete();
    /// Queues every asset of the content catalog, the appearances prioritized.
    void queueContentAssets();
    /// Queues every asset of the sound catalog, the sounds prioritized.
    void queueSoundAssets();

    /// Stored by the constructor; nothing in the class reads it.
    QString m_name;
    std::shared_ptr<TAssetAsyncLoader> m_loader;
    std::shared_ptr<TContentCatalog> m_contentCatalog;
    std::shared_ptr<TSoundCatalog> m_soundCatalog;
    EState m_state = Initial;
    QSet<QString> m_loadedAssets;
    size_t m_queuedAssetCount = 0;
};

}
}
