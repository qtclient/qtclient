#include "assets/assetmanager.h"

#include "assets/assetasyncloader.h"
#include "assets/contentcatalog.h"
#include "assets/soundcatalog.h"
#include "readonlymemoryblob.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>

#include <stdexcept>

namespace tibia::assets {

namespace {

const QString& contentCatalogFileName()
{
    static const QString name = QStringLiteral("catalog-content.json");
    return name;
}

const QString& soundCatalogFileName()
{
    static const QString name = QStringLiteral("catalog-sound.json");
    return name;
}

QJsonDocument jsonDocument(const tibia::shared::TReadOnlyMemoryBlob& blob)
{
    return QJsonDocument::fromJson(QString::fromUtf8(blob.getData()).toUtf8());
}

/// A catalog read from a JSON document; throws std::runtime_error when it is not valid.
template<typename Catalog>
std::shared_ptr<Catalog> readCatalog(const QJsonDocument& document)
{
    auto catalog = std::make_shared<Catalog>();
    catalog->load(document);
    if (!catalog->isValid())
        throw std::runtime_error("Invalid catalog data.");
    return catalog;
}

}

TAssetManager::TAssetManager(const QString& name, std::shared_ptr<TAssetAsyncLoader> loader, QObject* parent)
    : QObject(parent)
    , m_name(name)
    , m_loader(std::move(loader))
    , m_contentCatalog(std::make_shared<TContentCatalog>())
{
    connect(m_loader.get(), &TAssetAsyncLoader::assetLoaded, this, &TAssetManager::assetLoaded);
    connect(m_loader.get(), &TAssetAsyncLoader::assetLoadError, this, &TAssetManager::assetLoadError);
}

void TAssetManager::startLoading()
{
    if (m_state != Initial)
        throw std::runtime_error("m_State != INITIAL");
    m_state = LoadingCatalogs;
    Q_EMIT normalizedProgress({0, 1});
    checkComplete();
    m_loader->load(TAsset(TAsset::ContentCatalog, contentCatalogFileName()), true);
    m_loader->load(TAsset(TAsset::SoundCatalog, soundCatalogFileName()), true);
}

void TAssetManager::stopLoading()
{
    if (!m_loader)
        return;
    m_loader->stop();
    m_loader.reset();
}

void TAssetManager::onNewAdditionalAssetToLoad(const TAsset& AssetToLoad, const bool Prioritized)
{
    if (!m_loader) {
        reportError(QStringLiteral("Could not add asset '%1' to queue: Loader is already terminated").arg(AssetToLoad.file()));
        return;
    }
    if (m_loader->load(AssetToLoad, Prioritized))
        ++m_queuedAssetCount;
}

void TAssetManager::assetLoaded(const TAsset& Asset, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& ReadOnlyMemoryBlob)
{
    if (m_state == AppearancesLoaded || m_state == CatalogsLoaded) {
        m_loadedAssets.insert(Asset.file());
        Q_EMIT assetDataAccessible(Asset, ReadOnlyMemoryBlob);
        Q_EMIT normalizedProgress(progress());
        checkComplete();
        if (Asset.type() == TAsset::Appearances)
            m_state = AppearancesLoaded;
        return;
    }
    if (m_state != LoadingCatalogs)
        return;

    if (Asset.type() == TAsset::ContentCatalog)
        m_contentCatalog = readCatalog<TContentCatalog>(jsonDocument(*ReadOnlyMemoryBlob));
    else if (Asset.type() == TAsset::SoundCatalog)
        m_soundCatalog = readCatalog<TSoundCatalog>(jsonDocument(*ReadOnlyMemoryBlob));

    if (m_contentCatalog && m_soundCatalog && m_contentCatalog->isValid() && m_soundCatalog->isValid()) {
        m_state = CatalogsLoaded;
        queueContentAssets();
        queueSoundAssets();
        Q_EMIT spritesheetMetadataComplete(m_contentCatalog->getSpritesheetsMetadataManager());
    }
    Q_EMIT normalizedProgress(progress());
    checkComplete();
}

void TAssetManager::assetLoadError(const TAsset& Asset, const std::string& ErrorText)
{
    reportError(QStringLiteral("Failed to load asset '%1': %2").arg(Asset.file()).arg(QString::fromStdString(ErrorText)));
}

bool TAssetManager::waitForAppearances(QString* error)
{
    if (m_state == AppearancesLoaded || m_state == Complete)
        return true;
    if (m_state == Initial || !m_loader) {
        if (error)
            *error = QStringLiteral("The asset loading has not been started");
        return false;
    }
    QEventLoop loop;
    bool loaded = false;
    QString failure;
    const QMetaObject::Connection accessible =
        connect(this, &TAssetManager::assetDataAccessible, &loop,
                [&loop, &loaded](const TAsset& Asset, const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>&) {
                    if (Asset.type() != TAsset::Appearances)
                        return;
                    loaded = true;
                    loop.quit();
                });
    // Without both catalogs or the appearances the appearances never arrive.
    const QMetaObject::Connection failed =
        connect(m_loader.get(), &TAssetAsyncLoader::assetLoadError, &loop,
                [&loop, &failure](const TAsset& Asset, const std::string& ErrorText) {
                    if (Asset.type() != TAsset::ContentCatalog && Asset.type() != TAsset::SoundCatalog
                        && Asset.type() != TAsset::Appearances)
                        return;
                    failure = QStringLiteral("Failed to load asset '%1': %2").arg(Asset.file(), QString::fromStdString(ErrorText));
                    loop.quit();
                });
    loop.exec();
    disconnect(accessible);
    disconnect(failed);
    if (!loaded && error)
        *error = failure;
    return loaded;
}

void TAssetManager::reportError(const QString& message)
{
    qCritical() << message;
    Q_EMIT error(message);
}

tibia::shared::NormalizedProgress TAssetManager::progress() const
{
    if (m_state < AppearancesLoaded)
        return {0, 1};
    return {static_cast<int32_t>(m_loadedAssets.size()), static_cast<int32_t>(m_contentCatalog->assetCount())};
}

void TAssetManager::checkComplete()
{
    if (m_loadedAssets.isEmpty() || static_cast<size_t>(m_loadedAssets.size()) != m_queuedAssetCount)
        return;
    m_state = Complete;
    qDebug() << "Asset loading complete";
    Q_EMIT assetsComplete();
}

void TAssetManager::queueContentAssets()
{
    if (!m_loader)
        return;
    for (const TAsset& asset : m_contentCatalog->assets()) {
        if (m_loader->load(asset, asset.type() == TAsset::Appearances))
            ++m_queuedAssetCount;
    }
}

void TAssetManager::queueSoundAssets()
{
    if (!m_loader)
        return;
    for (const TAsset& asset : m_soundCatalog->assets()) {
        if (m_loader->load(asset, asset.type() == TAsset::Sounds))
            ++m_queuedAssetCount;
    }
}

}
