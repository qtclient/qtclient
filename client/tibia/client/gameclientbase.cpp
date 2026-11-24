#include "client/gameclientbase.h"

#include "appearances/appearanceinstance.h"
#include "appearances/appearanceinstancetracker.h"
#include "appearances/genericstorage.h"
#include "assets/assetasyncloader.h"
#include "assets/things.h"
#include "client/gamecore.h"
#include "config/clientoptions.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "game/gameactionrouter.h"
#include "game/passiveabilityinfostorage.h"
#include "game/traderstorage.h"
#include "proficiency/proficiencystorage.h"
#include "gamewindow/storeandresourcebalancehelpercontroller.h"
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
#include "sprites/spritesheetmanager.h"
#include "sprites/spritesystem.h"

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>

#include <stdexcept>

namespace tibia::client {

TGameClientBase::TGameClientBase(QQmlEngine* Engine,
                                 std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                                 config::TClientOptions* ClientOptions)
    : m_pEngine(Engine)
    , m_pTibiaFileSystemHelper(std::move(TibiaFileSystemHelper))
    , m_pClientOptions(ClientOptions)
    , m_pAppearancesManager(std::make_shared<appearances::TAppearancesManager>())
    , m_pCyclopediaMapStaticDataStorage(std::make_shared<cyclopedia::TCyclopediaMapStaticDataStorage>())
    , m_pPassiveAbilityInfoStorage(std::make_unique<game::TPassiveAbilityInfoStorage>())
    , m_pProficiencyStorage(std::make_unique<proficiency::TProficiencyStorage>(m_pAppearancesManager))
{
    appearances::TAppearancesManager::setInstance(m_pAppearancesManager.get());
    cyclopedia::TCyclopediaMapStaticDataStorage::setInstance(m_pCyclopediaMapStaticDataStorage.get());
    game::TPassiveAbilityInfoStorage::setInstance(m_pPassiveAbilityInfoStorage.get());
}

TGameClientBase::~TGameClientBase()
{
    if (m_pSpellInfoStorage)
        spells::TSpellInfoStorage::setInstance(nullptr);
    game::TPassiveAbilityInfoStorage::setInstance(nullptr);
    cyclopedia::TCyclopediaMapStaticDataStorage::setInstance(nullptr);
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

void TGameClientBase::initializeStorages()
{
    m_pTraderStorage = std::make_shared<game::TTraderStorage>();
    m_pResourceSpritesManager = std::make_shared<sprites::TResourceSpritesManager>();
    createSpellInfoStorage();
}

void TGameClientBase::initializeSpriteResolver()
{
    QObject::connect(&sprites::TSpriteSystem::instance().spritesheets(),
                     &sprites::TSpritesheetManager::spritesheetUncompressFailed, m_pResourceSpritesManager.get(),
                     [this](const sprites::TSpritesheetID& SpritesheetID) { onSpritesheetUncompressFailed(SpritesheetID); });
    m_pEngine->rootContext()->setContextProperty(QStringLiteral("ResourceSpritesManager"), m_pResourceSpritesManager.get());
}

void TGameClientBase::initializeAppearanceInstanceFactory()
{
    m_pAppearanceInstanceFactory = std::make_shared<appearances::TAppearanceInstanceFactory>(m_pAppearancesManager);
    const std::shared_ptr<appearances::TAppearanceInstanceTracker> tracker = m_pAppearanceInstanceFactory->instanceTracker();
    QObject::connect(tracker.get(), &appearances::TAppearanceInstanceTracker::loadSpritesheetWithID,
                     &sprites::TSpriteSystem::instance().spritesheets(), &sprites::TSpritesheetManager::getSpritesheet);
    m_pAppearanceInstanceGenericStoragesManager =
        std::make_shared<appearances::TAppearanceInstanceGenericStoragesManager>(appearanceInstanceFactory());
    m_pAppearanceInstanceGenericStoragesManager->registerInQmlContext(m_pEngine->rootContext());
}

void TGameClientBase::onSpritesheetUncompressFailed(const QString&) {}

void TGameClientBase::onAssetsComplete() {}

void TGameClientBase::onAssetsError() {}

void TGameClientBase::initializeQmlContext()
{
    m_pStoreAndResourceBalanceHelperController = std::make_unique<gamewindow::TStoreAndResourceBalanceHelperController>();
    m_pEngine->rootContext()->setContextProperty(QStringLiteral("storeAndResourceBalanceHelper"),
                                                 m_pStoreAndResourceBalanceHelperController.get());
}

void TGameClientBase::initializeAssets()
{
    cyclopedia::TCyclopediaMapStaticDataStorage* mapStaticData = cyclopediaMapStaticDataStorage().get();
    // CIP: the storage's asset requests go to the client's asset loading.
    QObject::connect(mapStaticData, &cyclopedia::TCyclopediaMapStaticDataStorage::newAssetDataToLoad, mapStaticData,
                     [](const tibia::assets::TAsset& asset, const bool prioritized) {
                         if (auto* loader = sprites::TSpriteSystem::instance().assetLoader())
                             loader->load(asset, prioritized);
                     });
    gamecore::setClientVersion(ClientVersion);
    QString error;
    if (!gamecore::loadAssets(ClientVersion, &error))
        m_assetLoadError = error;
}

void TGameClientBase::initializeSpriteTextureProviderManager()
{
    // Host: qtclient's providers live in a process-wide registry, so the
    // manager needs neither the sprite resolver nor a QML context property.
    m_pSpriteTextureProviderManager = std::make_shared<renderer::TSpriteTextureProviderManager>();
}

void TGameClientBase::connectToGameActionRouter()
{
    game::TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    sprites::TSpriteResolver* spriteResolver = &sprites::TSpriteSystem::instance().resolver();
    cyclopedia::TCyclopediaMapStaticDataStorage* mapStaticData = cyclopediaMapStaticDataStorage().get();
    QObject::connect(spriteResolver, &sprites::TSpriteResolver::publishGameAction, router,
                     &game::TInternalGameActionRouter::handleGameActionInternal);
    QObject::connect(mapStaticData, &cyclopedia::TCyclopediaMapStaticDataStorage::publishGameAction, router,
                     &game::TInternalGameActionRouter::handleGameActionInternal);
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal,
                     m_pSpriteTextureProviderManager.get(), &renderer::TSpriteTextureProviderManager::handleGameAction);
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, spriteResolver,
                     &sprites::TSpriteResolver::handleGameAction);
    QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, mapStaticData,
                     &cyclopedia::TCyclopediaMapStaticDataStorage::handleGameAction);
    if (m_pSoundController)
        QObject::connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, m_pSoundController.get(),
                         &sound::TSoundController::handleGameAction);
}

void TGameClientBase::createSpellInfoStorage()
{
    m_pSpellInfoStorage = std::make_unique<spells::TSpellInfoStorage>();
    spells::TSpellInfoStorage::setInstance(m_pSpellInfoStorage.get());
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
