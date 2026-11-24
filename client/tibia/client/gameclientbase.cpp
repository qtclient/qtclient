#include "client/gameclientbase.h"

#include "animation/phaseanimator.h"
#include "appearances/appearanceinstance.h"
#include "appearances/appearanceinstancetracker.h"
#include "app/clientoptions.h"
#include "appearances/genericstorage.h"
#include "assets/assetasyncloader.h"
#include "assets/assetfilesystemhelper.h"
#include "assets/assetmanager.h"
#include "assets/things.h"
#include "client/gamecore.h"
#include "config/clientoptions.h"
#include "config/clientsettings.h"
#include "config/tibiaconfig.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "game/gameactionrouter.h"
#include "game/passiveabilityinfostorage.h"
#include "game/traderstorage.h"
#include "imageproviders/imagestripimageprovider.h"
#include "proficiency/proficiencystorage.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/storeandresourcebalancehelpercontroller.h"
#include "minimap/minimapdiskio.h"
#include "minimap/minimapmanager.h"
#include "minimap/minimapmarkerstorage.h"
#include "minimap/minimaptilemanager.h"
#include "minimap/minimaptilestorage.h"
#include "minimap/minimapworldfeed.h"
#include "renderer/spritetextureprovider.h"
#include "sound/soundconfiguration.h"
#include "sound/soundcontroller.h"
#include "sound/soundengine.h"
#include "sound/soundmanager.h"
#include "sound/soundmetadatamanager.h"
#include "sound/soundplayermanager.h"
#include "shared/tibiafilesystemhelper.h"
#include "spells/spellinfostorage.h"
#include "sprites/resourcespritesmanager.h"
#include "sprites/spriteresolver.h"
#include "sprites/spritesheetcache.h"
#include "sprites/spritesheetdecompressorthreadpool.h"
#include "sprites/spritesheetmanager.h"
#include "sprites/spritesystem.h"
#include "assets/assethandler.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/worldmapstorage.h"
#include "filesystemhelper.h"
#include "randomnumbergenerator.h"

#include <QDebug>
#include <QDir>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>

#include <stdexcept>

namespace tibia::client {

namespace {

/// CIP: a file system helper whose data and base directory is `Path`, with a
/// file system helper of its own.
std::shared_ptr<shared::TTibiaFileSystemHelper> makeTibiaFileSystemHelper(const QString& Path)
{
    return std::make_shared<shared::TTibiaFileSystemHelper>(QDir(Path), std::make_shared<shared::TFileSystemHelper>());
}

}

TGameClientBase::TGameClientBase(QQmlEngine* Engine,
                                 std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                                 config::TClientOptions* ClientOptions,
                                 std::shared_ptr<config::TTibiaConfig> TibiaConfig)
    : m_pEngine(Engine)
    , m_pTibiaFileSystemHelper(std::move(TibiaFileSystemHelper))
    , m_pClientOptions(ClientOptions, [](config::TClientOptions*) {})
    , m_pTibiaConfig(TibiaConfig ? std::move(TibiaConfig) : config::processTibiaConfig())
    , m_pAppearancesManager(std::make_shared<appearances::TAppearancesManager>())
    , m_pCyclopediaMapStaticDataStorage(std::make_shared<cyclopedia::TCyclopediaMapStaticDataStorage>())
    , m_pSpellInfoStorage(std::make_unique<spells::TSpellInfoStorage>())
    , m_pPassiveAbilityInfoStorage(std::make_unique<game::TPassiveAbilityInfoStorage>())
    , m_pProficiencyStorage(std::make_unique<proficiency::TProficiencyStorage>(m_pAppearancesManager))
{
    appearances::TAppearancesManager::setInstance(m_pAppearancesManager.get());
    cyclopedia::TCyclopediaMapStaticDataStorage::setInstance(m_pCyclopediaMapStaticDataStorage.get());
    spells::TSpellInfoStorage::setInstance(m_pSpellInfoStorage.get());
    game::TPassiveAbilityInfoStorage::setInstance(m_pPassiveAbilityInfoStorage.get());
}

TGameClientBase::~TGameClientBase()
{
    if (m_pMinimapTileStorage) {
        setMinimapWorldmapProtocolHandler(nullptr);
        // Host: the sprite system goes back to its own resolver.
        if (m_pSpriteResolver)
            sprites::TSpriteSystem::instance().setResolver(nullptr);
        sprites::TSpriteSystem::instance().setSpritesheetManager(nullptr);
        m_pMinimapWorldFeed.reset();
        minimap::TMinimapMarkerStorage::setActive(nullptr);
        m_pCyclopediaMapStaticDataStorage->setMinimapMarkerStorage(nullptr);
        saveMinimapToDisk();
    }
    spells::TSpellInfoStorage::setInstance(nullptr);
    game::TPassiveAbilityInfoStorage::setInstance(nullptr);
    cyclopedia::TCyclopediaMapStaticDataStorage::setInstance(nullptr);
    appearances::TAppearanceInstanceFactory::install(nullptr);
    appearances::TAppearancesManager::setInstance(nullptr);
}

void TGameClientBase::initialize()
{
    initializeStorages();
    initializeSound();
    initializeQmlContext();
    initializeAssets();
    initializeSpriteResolver();
    initializeAppearanceInstanceFactory();
    initializeMinimap();
    emitInitialized();
}

void TGameClientBase::setMainWindow(QQuickWindow* MainWindow)
{
    m_pMainWindow = MainWindow;
    if (!MainWindow)
        return;
    initializeSpriteTextureProviderManager();
    connectToGameActionRouter();
    emitMainWindowInitialized();
}

void TGameClientBase::updateWithTimestamp(const qint64 Timestamp)
{
    if (m_pAppearanceInstanceGenericStoragesManager)
        m_pAppearanceInstanceGenericStoragesManager->updateWithTimestamp(Timestamp);
    // Host: qtclient's sound controller catches up with the frame here.
    if (m_pSoundController)
        m_pSoundController->update(Timestamp);
    if (m_pSoundManager)
        m_pSoundManager->updateWithTimestamp(Timestamp);
    renderer::updateHighlightStrength(Timestamp);
}

std::shared_ptr<config::TTibiaConfig> TGameClientBase::createResourcesTibiaConfig()
{
    const std::shared_ptr<shared::TTibiaFileSystemHelper> helper =
        makeTibiaFileSystemHelper(gamecore::resourcesDirectory().canonicalPath());
    return std::make_shared<config::TTibiaConfig>(*helper);
}

std::shared_ptr<sprites::TSpritesheetManager> TGameClientBase::createSpritesheetManager(const config::TTibiaConfig& TibiaConfig)
{
    auto decompressor = std::make_shared<sprites::TSpritesheetDecompressorThreadpool>();
    const auto capacity = static_cast<uint32_t>(TibiaConfig.clientSettingAtLeast(
        QStringLiteral("GRAPHICS"), QStringLiteral("spritesheetCacheSize"), sprites::TSpriteSystem::defaultCacheCapacity(), 2));
    qDebug() << "Using spritesheet cache size of" << capacity;
    return std::make_shared<sprites::TSpritesheetManager>(std::move(decompressor),
                                                          std::make_shared<sprites::TSpritesheetCache>(capacity));
}

std::shared_ptr<assets::TAssetFileSystemHelper> TGameClientBase::createAssetFileSystemHelper()
{
    // Host: the assets' and sounds' search roots of qtclient's data tree (CIP:
    // the directories of a file system helper on the Resources directory).
    const std::string assetsRoot = gamecore::assetsRoot(ClientVersion);
    gamecore::soundsRoot(ClientVersion);
    const QDir assetsDirectory(QString::fromStdString(tibia::files::data().getRealPath(assetsRoot)));
    const std::string soundsPath = tibia::files::data().getRealPath("sounds");
    const QDir soundsDirectory = soundsPath.empty() ? assetsDirectory : QDir(QString::fromStdString(soundsPath));
    tibiaFileSystemHelper()->setAssetsDirectories(assetsDirectory, soundsDirectory);
    return std::make_shared<assets::TAssetFileSystemHelper>(tibiaFileSystemHelper());
}

void TGameClientBase::initializeStorages()
{
    const std::shared_ptr<config::TTibiaConfig> resourcesTibiaConfig = createResourcesTibiaConfig();
    m_pSpritesheetManager = createSpritesheetManager(*resourcesTibiaConfig);
    // Host: the code that reaches the spritesheets process-wide finds the client's.
    sprites::TSpriteSystem::instance().setSpritesheetManager(m_pSpritesheetManager);
    m_pAssetFileSystemHelper = createAssetFileSystemHelper();
    m_pTraderStorage = std::make_shared<game::TTraderStorage>();
    m_pResourceSpritesManager = std::make_shared<sprites::TResourceSpritesManager>();
    m_pMinimapTileStorage = std::make_shared<minimap::TMinimapTileStorage>();
    m_pMinimapMarkerStorage = std::make_shared<minimap::TMinimapMarkerStorage>();
    m_pMinimapTileManagerDiskIO = std::make_shared<minimap::TMinimapDiskIOAsync>(tibiaFileSystemHelper());
    m_pMinimapTileManager = std::make_shared<minimap::TMinimapTileManager>(m_pMinimapTileManagerDiskIO, m_pMinimapTileStorage);
    // CIP: the animators draw their random phase durations from the
    // process-wide random number generator.
    animation::TAnimator::setRandomNumberGenerator(shared::TRandomNumberGenerator::instance());
    // CIP: the engine's image providers (the HUD flags and the other image
    // strips).
    imageproviders::registerImageProviders(m_pEngine);
    // Host: the code that still reaches the markers through these finds them.
    minimap::TMinimapMarkerStorage::setActive(m_pMinimapMarkerStorage.get());
    m_pCyclopediaMapStaticDataStorage->setMinimapMarkerStorage(m_pMinimapMarkerStorage.get());
}

void TGameClientBase::initializeMinimap()
{
    m_pMinimapTileDiskIO = std::make_shared<minimap::TMinimapDiskIOAsync>(tibiaFileSystemHelper());
    // Host: CipSoft's bundled minimap (Resources/minimap, next to the assets)
    // is copied in while the user's minimap has no tiles; reads fall back to
    // it. getRealPath, not getRealDir plus a name: a "/" search root must not
    // become "//..." (the WASM file system rejects that).
    std::string defaultMinimapPath = tibia::files::data().getRealPath("minimap");
    if (defaultMinimapPath.empty())
        defaultMinimapPath = tibia::files::data().getRealPath(gamecore::assetsRoot(ClientVersion) + "minimap");
    if (!defaultMinimapPath.empty()) {
        const QString defaultTilesDir = QString::fromStdString(defaultMinimapPath);
        m_pMinimapTileDiskIO->importDefaultTiles(defaultTilesDir);
        m_pMinimapTileDiskIO->waitForDone();
        m_pMinimapTileManagerDiskIO->setDefaultTilesDir(defaultTilesDir);
    }
    m_pMinimapManager = std::make_shared<minimap::TMinimapManager>(cyclopediaMapStaticDataStorage());
    m_pMinimapManager->registerInQmlContext(m_pEngine->rootContext());
    m_minimapMarkerStorageTimer.setSingleShot(true);

    minimap::TMinimapMarkerStorage* markerStorage = minimapMarkerStorage().get();
    minimap::TMinimapTileStorage* tileStorage = minimapTileStorage().get();
    minimap::TMinimapDiskIOAsync* diskIO = minimapTileDiskIO().get();
    QObject::connect(markerStorage, &minimap::TMinimapMarkerStorage::requestDelayedCallback, &m_minimapMarkerStorageTimer,
                     qOverload<>(&QTimer::start));
    QObject::connect(&m_minimapMarkerStorageTimer, &QTimer::timeout, markerStorage,
                     &minimap::TMinimapMarkerStorage::onDelayedCallback);
    QObject::connect(markerStorage, &minimap::TMinimapMarkerStorage::startSavingMinimapMarkerFileContentToDisk, diskIO,
                     &minimap::TMinimapDiskIOAsync::saveMinimapMarkerFileContentToDisk);
    QObject::connect(tileStorage, &minimap::TMinimapTileStorage::startLoadingMinimapTileFromDisk, diskIO,
                     &minimap::TMinimapDiskIOAsync::loadTile);
    QObject::connect(tileStorage, &minimap::TMinimapTileStorage::startSavingMinimapTileToDisk, diskIO,
                     &minimap::TMinimapDiskIOAsync::saveTile);
    QObject::connect(diskIO, &minimap::TMinimapDiskIOAsync::loadingSucceeded, tileStorage,
                     &minimap::TMinimapTileStorage::onLoadingSucceeded, Qt::QueuedConnection);
    QObject::connect(diskIO, &minimap::TMinimapDiskIOAsync::loadingFailed, tileStorage,
                     &minimap::TMinimapTileStorage::onLoadingFailed);
    QObject::connect(diskIO, &minimap::TMinimapDiskIOAsync::loadingMinimapMarkersSucceeded, markerStorage,
                     &minimap::TMinimapMarkerStorage::setMarkersFromMinimapMarkerFile);

    // Host: the tile storage reads the current session's fields.
    m_pMinimapWorldFeed = std::make_unique<minimap::TMinimapWorldFeed>(*tileStorage);
}

void TGameClientBase::setMinimapWorldmapProtocolHandler(worldmap::TWorldmapProtocolMessageHandler* Handler)
{
    if (m_pMinimapWorldmapProtocolHandler == Handler)
        return;
    if (m_pMinimapWorldmapProtocolHandler)
        m_pMinimapWorldmapProtocolHandler->setMinimapTileStorage(nullptr);
    m_pMinimapWorldmapProtocolHandler = Handler;
    if (!Handler || !m_pMinimapTileStorage)
        return;
    Handler->setMinimapTileStorage(m_pMinimapTileStorage.get());
    // The handler can hold a map already: take the floor and unseen fields of
    // its area as it would have.
    if (worldmap::TWorldMapStorage* storage = Handler->storage()) {
        const Position center = storage->centralPosition();
        if (center.isValid()) {
            m_pMinimapTileStorage->setPlayerZ(center.z);
            m_pMinimapTileStorage->markUnseen();
        }
    }
}

void TGameClientBase::saveMinimapToDisk()
{
    if (m_pMinimapMarkerStorage)
        m_pMinimapMarkerStorage->onDelayedCallback();
    if (m_pMinimapTileStorage)
        m_pMinimapTileStorage->saveAllTilesToDisk();
    if (m_pMinimapTileDiskIO)
        m_pMinimapTileDiskIO->waitForDone();
}

void TGameClientBase::initializeSpriteResolver()
{
    QObject::connect(spritesheetManager().get(), &sprites::TSpritesheetManager::spritesheetUncompressFailed,
                     m_pResourceSpritesManager.get(),
                     [this](const sprites::TSpritesheetID& SpritesheetID) { onSpritesheetUncompressFailed(SpritesheetID); });
    // CIP: the client's resolver draws from the spritesheet manager and the
    // minimap tiles of the client's tile manager. Host: the resolver is
    // installed in the sprite system for the code that reaches it
    // process-wide.
    m_pSpriteResolver = std::make_shared<sprites::TSpriteResolver>(spritesheetManager());
    m_pSpriteResolver->setMinimapTileManager(minimapTileManager());
    sprites::TSpriteSystem::instance().setResolver(m_pSpriteResolver);
    m_pEngine->rootContext()->setContextProperty(QStringLiteral("ResourceSpritesManager"), m_pResourceSpritesManager.get());
}

void TGameClientBase::initializeAppearanceInstanceFactory()
{
    m_pAppearanceInstanceFactory = std::make_shared<appearances::TAppearanceInstanceFactory>(m_pAppearancesManager);
    appearances::TAppearanceInstanceFactory::install(m_pAppearanceInstanceFactory);
    const std::shared_ptr<appearances::TAppearanceInstanceTracker> tracker = m_pAppearanceInstanceFactory->instanceTracker();
    QObject::connect(tracker.get(), &appearances::TAppearanceInstanceTracker::loadSpritesheetWithID,
                     spritesheetManager().get(), &sprites::TSpritesheetManager::getSpritesheet);
    m_pAppearanceInstanceGenericStoragesManager =
        std::make_shared<appearances::TAppearanceInstanceGenericStoragesManager>(appearanceInstanceFactory());
    m_pAppearanceInstanceGenericStoragesManager->registerInQmlContext(m_pEngine->rootContext());
}

void TGameClientBase::onSpritesheetUncompressFailed(const QString&) {}

void TGameClientBase::onAssetsComplete() {}

void TGameClientBase::onAssetsError() {}

void TGameClientBase::initializeQmlContext()
{
    // CIP: the process-wide mouse cursor controller (with the client options)
    // and text copy controller, both root context properties. Host: qtclient's
    // cursor reads the options as the host's option JSON.
    gamewindow::TMouseCursorController::createInstance(m_pEngine, app::ClientOptions::load().options());
    gamewindow::TTextCopyController::createInstance(m_pEngine);
    m_pStoreAndResourceBalanceHelperController = std::make_unique<gamewindow::TStoreAndResourceBalanceHelperController>();
    m_pStoreAndResourceBalanceHelperController->registerInRootContext(m_pEngine->rootContext());
    // CIP: the Appearance type helper reads the client's appearances.
    qmlcomponents::TAppearanceTypeHelperQmlService::instance()->setAppearancesManager(appearancesManager());
}

void TGameClientBase::initializeAssets()
{
    gamecore::setClientVersion(ClientVersion);
    const std::string assetsRoot = gamecore::assetsRoot(ClientVersion);
    m_pAssetAsyncLoader = std::make_shared<assets::TAssetAsyncLoader>(m_pAssetFileSystemHelper);
    m_pAssetManager = std::make_shared<assets::TAssetManager>(m_pAssetFileSystemHelper->assetsDirectory().absolutePath(),
                                                              m_pAssetAsyncLoader, nullptr);

    const std::shared_ptr<appearances::TAppearancesManager> appearances = appearancesManager();
    // Host: the login sends the assets' identifier (assets.json.sha256).
    appearances->readAssetIdentifier(assetsRoot);
    m_pAssetHandler = std::make_shared<assets::TAssetHandler>(
        spritesheetManager(), appearances, m_pSoundMetadataManager, m_pSoundManager,
        m_pTraderStorage, m_pProficiencyStorage.get(),
        [appearances](auto information) { appearances->setStaticInformation(std::move(information)); },
        [](auto areas) { appearances::TAppearancesManager::setStaticMapData(std::move(areas)); },
        cyclopediaMapStaticDataStorage());

    assets::TAssetManager* manager = assetManager().get();
    assets::TAssetHandler* handler = m_pAssetHandler.get();
    // Host: qtclient's sprite lookups read the metadata through the sprite
    // system; it gets it before the handler, so the appearances find their
    // sprite sizes.
    QObject::connect(manager, &assets::TAssetManager::spritesheetMetadataComplete, manager,
                     [](const std::shared_ptr<sprites::TSpritesheetsMetadataManager>& metadata) {
                         sprites::TSpriteSystem::instance().setMetadata(metadata);
                     });
    QObject::connect(manager, &assets::TAssetManager::spritesheetMetadataComplete, handler,
                     &assets::TAssetHandler::handleSpritesheetMetadata);
    QObject::connect(manager, &assets::TAssetManager::assetDataAccessible, handler, &assets::TAssetHandler::handleAssetData);
    QObject::connect(manager, &assets::TAssetManager::assetsComplete, manager, [this] { onAssetsComplete(); });
    QObject::connect(manager, &assets::TAssetManager::error, manager, [this] { onAssetsError(); });
    QObject::connect(handler, &assets::TAssetHandler::newAssetDataToLoad, manager,
                     &assets::TAssetManager::onNewAdditionalAssetToLoad);
    // CIP: new sound metadata reaches the sound manager (when the sound system is up).
    if (m_pSoundMetadataManager && m_pSoundManager)
        QObject::connect(m_pSoundMetadataManager.get(), &sound::TSoundMetadataManager::metadataChanged, m_pSoundManager.get(),
                         &sound::TSoundManager::onSoundMetadataChanged);
    QObject::connect(spritesheetManager().get(), &sprites::TSpritesheetManager::spritesheetMissing, m_pAssetAsyncLoader.get(),
                     &assets::TAssetAsyncLoader::priorityLoadOfSpritesheet, Qt::QueuedConnection);
    // Host: the Cyclopedia map's process-wide fallback storage asks this loader.
    sprites::TSpriteSystem::instance().setAssetLoader(m_pAssetAsyncLoader);
}

void TGameClientBase::initializeSpriteTextureProviderManager()
{
    m_pSpriteTextureProviderManager = std::make_shared<renderer::TSpriteTextureProviderManager>(m_pSpriteResolver);
    m_pEngine->rootContext()->setContextProperty(QStringLiteral("SpriteTextureProviderManager"),
                                                 m_pSpriteTextureProviderManager.get());
}

void TGameClientBase::connectToGameActionRouter()
{
    game::TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    sprites::TSpriteResolver* spriteResolver = this->spriteResolver().get();
    cyclopedia::TCyclopediaMapStaticDataStorage* mapStaticData = cyclopediaMapStaticDataStorage().get();
    minimap::TMinimapTileManager* tileManager = minimapTileManager().get();
    QObject::connect(tileManager, &minimap::TMinimapTileManager::publishGameAction, router,
                     &game::TInternalGameActionRouter::handleGameActionInternal);
    QObject::connect(spriteResolver, &sprites::TSpriteResolver::publishGameAction, router,
                     &game::TInternalGameActionRouter::handleGameActionInternal);
    QObject::connect(mapStaticData, &cyclopedia::TCyclopediaMapStaticDataStorage::publishGameAction, router,
                     &game::TInternalGameActionRouter::handleGameActionInternal);
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, tileManager,
                     &minimap::TMinimapTileManager::handleGameAction);
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal,
                     m_pSpriteTextureProviderManager.get(), &renderer::TSpriteTextureProviderManager::handleGameAction);
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, spriteResolver,
                     &sprites::TSpriteResolver::handleGameAction);
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, mapStaticData,
                     &cyclopedia::TCyclopediaMapStaticDataStorage::handleGameAction);
    if (m_pSoundController)
        QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, m_pSoundController.get(),
                         &sound::TSoundController::handleGameAction);
    // Host: qtclient's marker storage handles the marker dialog's actions.
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, minimapMarkerStorage().get(),
                     qOverload<const input::IGameActionPtr&>(&minimap::TMinimapMarkerStorage::handleGameAction));
}

std::shared_ptr<shared::TTibiaFileSystemHelper> TGameClientBase::tibiaFileSystemHelper() const
{
    if (!m_pTibiaFileSystemHelper)
        throw std::logic_error("m_pTibiaFileSystemHelper was not initialized yet");
    return m_pTibiaFileSystemHelper;
}

std::shared_ptr<appearances::TAppearancesManager> TGameClientBase::appearancesManager() const
{
    if (!m_pAppearancesManager)
        throw std::logic_error("m_pAppearancesManager was not initialized yet");
    return m_pAppearancesManager;
}

std::shared_ptr<assets::TAssetManager> TGameClientBase::assetManager() const
{
    if (!m_pAssetManager)
        throw std::logic_error("m_pAssetManager was not initialized yet");
    return m_pAssetManager;
}

std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> TGameClientBase::cyclopediaMapStaticDataStorage() const
{
    if (!m_pCyclopediaMapStaticDataStorage)
        throw std::logic_error("m_pCyclopediaMapStaticDataStorage was not initialized yet");
    return m_pCyclopediaMapStaticDataStorage;
}

std::shared_ptr<appearances::TAppearanceInstanceFactory> TGameClientBase::appearanceInstanceFactory() const
{
    if (!m_pAppearanceInstanceFactory)
        throw std::logic_error("m_pAppearanceInstanceFactory was not initialized yet");
    return m_pAppearanceInstanceFactory;
}

std::shared_ptr<game::TTraderStorage> TGameClientBase::traderStorage() const
{
    if (!m_pTraderStorage)
        throw std::logic_error("m_pTraderStorage was not initialized yet");
    return m_pTraderStorage;
}

std::shared_ptr<game::TGameActionRouter> TGameClientBase::gameActionRouter() const
{
    if (!m_pGameActionRouter)
        throw std::logic_error("m_pGameActionRouter was not initialized yet");
    return m_pGameActionRouter;
}

std::shared_ptr<minimap::TMinimapDiskIOAsync> TGameClientBase::minimapTileDiskIO() const
{
    if (!m_pMinimapTileDiskIO)
        throw std::logic_error("m_pMinimapTileDiskIO was not initialized yet");
    return m_pMinimapTileDiskIO;
}

std::shared_ptr<minimap::TMinimapTileStorage> TGameClientBase::minimapTileStorage() const
{
    if (!m_pMinimapTileStorage)
        throw std::logic_error("m_pMinimapTileStorage was not initialized yet");
    return m_pMinimapTileStorage;
}

std::shared_ptr<minimap::TMinimapMarkerStorage> TGameClientBase::minimapMarkerStorage() const
{
    if (!m_pMinimapMarkerStorage)
        throw std::logic_error("m_pMinimapMarkerStorage was not initialized yet");
    return m_pMinimapMarkerStorage;
}

std::shared_ptr<minimap::TMinimapTileManager> TGameClientBase::minimapTileManager() const
{
    if (!m_pMinimapTileManager)
        throw std::logic_error("m_pMinimapTileManager was not initialized yet");
    return m_pMinimapTileManager;
}

std::shared_ptr<sprites::TSpriteResolver> TGameClientBase::spriteResolver() const
{
    if (!m_pSpriteResolver)
        throw std::logic_error("m_pSpriteResolver was not initialized yet");
    return m_pSpriteResolver;
}

std::shared_ptr<sprites::TSpritesheetManager> TGameClientBase::spritesheetManager() const
{
    if (!m_pSpritesheetManager)
        throw std::logic_error("m_pSpritesheetManager was not initialized yet");
    return m_pSpritesheetManager;
}

gamewindow::TStoreAndResourceBalanceHelperController* TGameClientBase::storeAndResourceBalanceHelperController() const
{
    if (!m_pStoreAndResourceBalanceHelperController)
        throw std::logic_error("m_pStoreAndResourceBalanceHelperController was not initialized yet");
    return m_pStoreAndResourceBalanceHelperController.get();
}

std::shared_ptr<sound::TSoundMetadataManager> TGameClientBase::soundMetadataManager() const
{
    if (!m_pSoundMetadataManager)
        throw std::logic_error("m_pSoundMetadataManager was not initialized yet");
    return m_pSoundMetadataManager;
}

void TGameClientBase::initializeSound()
{
    const sound::TSoundSettings settings;
    m_pSoundConfiguration = std::make_shared<sound::TSoundConfiguration>(settings, config::TClientOptions::instance());
    if (!settings.noSound)
        sound::TSoundEngine::instance().initialize(m_pSoundConfiguration);
    if (!sound::TSoundEngine::instance().isInitialized())
        return;
    m_pSoundMetadataManager = std::make_shared<sound::TSoundMetadataManager>(nullptr, m_pSoundConfiguration->settings().ambienceDelayFactor);
    // qtclient's sound files live in the data search path's `sounds/`.
    m_pSoundManager = std::make_shared<sound::TSoundManager>(m_pSoundMetadataManager, "sounds/", settings.soundCacheSeconds);
    m_pSoundPlayerManager = std::make_shared<sound::TSoundPlayerManager>(settings.soundPlayerCount);
    m_pSoundController = std::make_shared<sound::TSoundController>(m_pSoundMetadataManager, m_pSoundManager,
                                                                   m_pSoundPlayerManager, m_pSoundConfiguration);
    QObject::connect(m_pSoundManager.get(), &sound::TSoundManager::playableSoundsChanged, m_pSoundPlayerManager.get(),
                     &sound::TSoundPlayerManager::onSoundManagerPlayableSoundsChanged);
}

}
