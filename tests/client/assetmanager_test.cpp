// CIP assets::TAssetManager: catalogs first, then every catalog asset, until all arrived.
#include "assets/assetasyncloader.h"
#include "assets/assetmanager.h"
#include "assets/contentcatalog.h"
#include "assets/soundcatalog.h"
#include "readonlymemoryblob.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>
#include <vector>

using tibia::assets::TAsset;
using tibia::assets::TAssetAsyncLoader;
using tibia::assets::TAssetManager;
using tibia::shared::NormalizedProgress;
using tibia::shared::TReadOnlyMemoryBlob;

namespace {

void ensureApplication()
{
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char name[] = "assetmanager_test";
        static char* argv[] = {name, nullptr};
        new QCoreApplication(argc, argv); // never destroyed (Qt statics outlive it)
    }
}

/// Records what is queued instead of loading it.
class TRecordingLoader final : public TAssetAsyncLoader
{
public:
    TRecordingLoader()
        : TAssetAsyncLoader(nullptr)
    {
    }

    bool load(const TAsset& asset, const bool prioritized) override
    {
        queued.emplace_back(asset, prioritized);
        return accept;
    }

    std::vector<std::pair<TAsset, bool>> queued;
    bool accept = true;
};

std::shared_ptr<TReadOnlyMemoryBlob> blob(const char* text)
{
    return std::make_shared<TReadOnlyMemoryBlob>(QByteArray(text));
}

struct Fixture
{
    Fixture()
    {
        ensureApplication();
        loader = std::make_shared<TRecordingLoader>();
        manager = std::make_unique<TAssetManager>(QStringLiteral("assets"), loader, nullptr);
        QObject::connect(manager.get(), &TAssetManager::normalizedProgress,
                         [this](const NormalizedProgress& progress) { progresses.push_back(progress); });
        QObject::connect(manager.get(), &TAssetManager::assetDataAccessible,
                         [this](const TAsset& asset, const std::shared_ptr<TReadOnlyMemoryBlob>&) { accessible.push_back(asset.file()); });
        QObject::connect(manager.get(), &TAssetManager::assetsComplete, [this] { ++completions; });
        QObject::connect(manager.get(), &TAssetManager::spritesheetMetadataComplete,
                         [this](const std::shared_ptr<tibia::sprites::TSpritesheetsMetadataManager>& metadata) {
                             metadataEmitted = metadata != nullptr;
                         });
        QObject::connect(manager.get(), &TAssetManager::error, [this](const QString& reason) { errors.push_back(reason); });
    }

    void loadCatalogs()
    {
        manager->startLoading();
        manager->assetLoaded(TAsset(TAsset::ContentCatalog, QStringLiteral("catalog-content.json")),
                             blob(R"([{"type":"appearances","file":"appearances.dat"},{"type":"staticdata","file":"staticdata.dat"}])"));
        manager->assetLoaded(TAsset(TAsset::SoundCatalog, QStringLiteral("catalog-sound.json")),
                             blob(R"([{"type":"sounds","file":"sounds.dat"}])"));
    }

    std::shared_ptr<TRecordingLoader> loader;
    std::unique_ptr<TAssetManager> manager;
    std::vector<NormalizedProgress> progresses;
    std::vector<QString> accessible;
    std::vector<QString> errors;
    int completions = 0;
    bool metadataEmitted = false;
};

}

TEST(AssetManager, StartQueuesBothCatalogsPrioritized)
{
    Fixture f;
    EXPECT_EQ(f.manager->state(), TAssetManager::Initial);
    f.manager->startLoading();
    EXPECT_EQ(f.manager->state(), TAssetManager::LoadingCatalogs);
    ASSERT_EQ(f.loader->queued.size(), 2u);
    EXPECT_EQ(f.loader->queued[0].first.type(), TAsset::ContentCatalog);
    EXPECT_EQ(f.loader->queued[0].first.file(), QStringLiteral("catalog-content.json"));
    EXPECT_TRUE(f.loader->queued[0].second);
    EXPECT_EQ(f.loader->queued[1].first.type(), TAsset::SoundCatalog);
    EXPECT_EQ(f.loader->queued[1].first.file(), QStringLiteral("catalog-sound.json"));
    EXPECT_TRUE(f.loader->queued[1].second);
    ASSERT_EQ(f.progresses.size(), 1u);
    EXPECT_EQ(f.progresses[0], (NormalizedProgress{0, 1}));
    EXPECT_THROW(f.manager->startLoading(), std::runtime_error);
}

TEST(AssetManager, BothCatalogsQueueTheirAssets)
{
    Fixture f;
    f.manager->startLoading();
    f.manager->assetLoaded(TAsset(TAsset::ContentCatalog, QStringLiteral("catalog-content.json")),
                           blob(R"([{"type":"appearances","file":"appearances.dat"},{"type":"staticdata","file":"staticdata.dat"}])"));
    // The sound catalog is still missing.
    EXPECT_EQ(f.manager->state(), TAssetManager::LoadingCatalogs);
    EXPECT_EQ(f.loader->queued.size(), 2u);
    EXPECT_FALSE(f.metadataEmitted);

    f.manager->assetLoaded(TAsset(TAsset::SoundCatalog, QStringLiteral("catalog-sound.json")),
                           blob(R"([{"type":"sounds","file":"sounds.dat"}])"));
    EXPECT_EQ(f.manager->state(), TAssetManager::CatalogsLoaded);
    EXPECT_TRUE(f.metadataEmitted);
    ASSERT_EQ(f.loader->queued.size(), 5u);
    EXPECT_EQ(f.loader->queued[2].first.file(), QStringLiteral("appearances.dat"));
    EXPECT_TRUE(f.loader->queued[2].second);
    EXPECT_EQ(f.loader->queued[3].first.file(), QStringLiteral("staticdata.dat"));
    EXPECT_FALSE(f.loader->queued[3].second);
    EXPECT_EQ(f.loader->queued[4].first.file(), QStringLiteral("sounds.dat"));
    EXPECT_TRUE(f.loader->queued[4].second);
    EXPECT_EQ(f.manager->queuedAssetCount(), 3u);
    // Progress counts the content catalog's assets.
    EXPECT_EQ(f.progresses.back(), (NormalizedProgress{0, 2}));
}

TEST(AssetManager, AssetsArriveUntilComplete)
{
    Fixture f;
    f.loadCatalogs();
    f.manager->assetLoaded(TAsset(TAsset::StaticData, QStringLiteral("staticdata.dat")), blob("x"));
    EXPECT_EQ(f.manager->state(), TAssetManager::CatalogsLoaded);
    EXPECT_EQ(f.progresses.back(), (NormalizedProgress{1, 2}));

    f.manager->assetLoaded(TAsset(TAsset::Appearances, QStringLiteral("appearances.dat")), blob("x"));
    EXPECT_EQ(f.manager->state(), TAssetManager::AppearancesLoaded);
    EXPECT_EQ(f.progresses.back(), (NormalizedProgress{2, 2}));
    EXPECT_EQ(f.completions, 0);

    f.manager->assetLoaded(TAsset(TAsset::Sounds, QStringLiteral("sounds.dat")), blob("x"));
    EXPECT_EQ(f.manager->state(), TAssetManager::Complete);
    EXPECT_EQ(f.completions, 1);
    EXPECT_EQ(f.progresses.back(), (NormalizedProgress{3, 2}));
    EXPECT_EQ(f.accessible, (std::vector<QString>{QStringLiteral("staticdata.dat"), QStringLiteral("appearances.dat"),
                                                  QStringLiteral("sounds.dat")}));
}

TEST(AssetManager, AssetsBeforeTheCatalogsAreIgnored)
{
    Fixture f;
    f.manager->assetLoaded(TAsset(TAsset::StaticData, QStringLiteral("staticdata.dat")), blob("x"));
    EXPECT_TRUE(f.accessible.empty());
    EXPECT_TRUE(f.progresses.empty());
}

TEST(AssetManager, InvalidCatalogThrows)
{
    Fixture f;
    f.manager->startLoading();
    EXPECT_THROW(f.manager->assetLoaded(TAsset(TAsset::ContentCatalog, QStringLiteral("catalog-content.json")),
                                        blob(R"([{"file":"missing-type"}])")),
                 std::runtime_error);
}

TEST(AssetManager, LoadErrorsAreReported)
{
    Fixture f;
    f.manager->assetLoadError(TAsset(TAsset::Sprite, QStringLiteral("a.bmp")), "boom");
    ASSERT_EQ(f.errors.size(), 1u);
    EXPECT_EQ(f.errors[0], QStringLiteral("Failed to load asset 'a.bmp': boom"));
}

TEST(AssetManager, AdditionalAssetsCountWhenQueued)
{
    Fixture f;
    f.manager->onNewAdditionalAssetToLoad(TAsset(TAsset::MapImage, QStringLiteral("map.png")), true);
    EXPECT_EQ(f.manager->queuedAssetCount(), 1u);
    ASSERT_EQ(f.loader->queued.size(), 1u);
    EXPECT_TRUE(f.loader->queued[0].second);
    f.loader->accept = false;
    f.manager->onNewAdditionalAssetToLoad(TAsset(TAsset::MapImage, QStringLiteral("map.png")), false);
    EXPECT_EQ(f.manager->queuedAssetCount(), 1u);
}

TEST(AssetManager, AdditionalAssetWithoutLoaderIsAnError)
{
    ensureApplication();
    TAssetManager manager(QStringLiteral("assets"), nullptr, nullptr);
    std::vector<QString> errors;
    QObject::connect(&manager, &TAssetManager::error, [&errors](const QString& reason) { errors.push_back(reason); });
    manager.onNewAdditionalAssetToLoad(TAsset(TAsset::MapImage, QStringLiteral("map.png")), false);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0], QStringLiteral("Could not add asset 'map.png' to queue: Loader is already terminated"));
    EXPECT_EQ(manager.queuedAssetCount(), 0u);
}
