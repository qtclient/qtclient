#pragma once

#include <QPointer>
#include <QString>
#include <QTimer>
#include <QtGlobal>

#include <memory>

class QQmlEngine;
class QQuickWindow;

namespace tibia {
namespace assets {
class TAssetAsyncLoader;
class TAssetHandler;
class TAssetManager;
}
namespace appearances {
class TAppearanceInstanceFactory;
class TAppearanceInstanceGenericStoragesManager;
class TAppearancesManager;
}
namespace config {
class TClientOptions;
class TTibiaConfig;
}
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
}
namespace minimap {
class TMinimapDiskIOAsync;
class TMinimapManager;
class TMinimapMarkerStorage;
class TMinimapTileManager;
class TMinimapTileStorage;
class TMinimapWorldFeed;
}
namespace worldmap {
class TWorldmapProtocolMessageHandler;
}
namespace game {
class TGameActionRouter;
class TPassiveAbilityInfoStorage;
class TTraderStorage;
}
namespace proficiency {
class TProficiencyStorage;
}
namespace renderer {
class TSpriteTextureProviderManager;
}
namespace shared {
class TTibiaFileSystemHelper;
}
namespace sprites {
class TResourceSpritesManager;
class TSpriteResolver;
}
namespace spells {
class TSpellInfoStorage;
}
namespace gamewindow {
class TStoreAndResourceBalanceHelperController;
}
namespace sound {
class TSoundConfiguration;
class TSoundController;
class TSoundManager;
class TSoundMetadataManager;
class TSoundPlayerManager;
}
}

namespace tibia {
namespace client {

/// CIP client::TGameClientBase: what the game client holds for the whole
/// process, apart from the game window: the QML engine, the main window, the
/// file system helper, the client options, the appearances, the Cyclopedia
/// map's static data, the minimap (tiles, markers and their disk), the spell
/// and passive ability tables, the game action router, the store balance
/// helper and the sound system.
///
/// The storages other code still reaches through their class's `instance()`
/// are installed there while the client exists (`setInstance`); without a
/// client (tests, tools) `instance()` falls back to a storage of its own.
///
/// CipSoft's TGameClientBase is the QObject between QObject and TGameClient.
/// In qtclient that moc surface (the initialized / mainWindowInitialized
/// signals) comes with the generated base of TGameClient, so this class holds
/// the members and the non-moc behaviour, and TGameClient derives from both
/// (it emits the two signals for this class: emitInitialized,
/// emitMainWindowInitialized).
///
/// The application makes the client, runs initialize() once and hands it the
/// client window with setMainWindow(); updateWithTimestamp() runs before each
/// frame.
class TGameClientBase
{
public:
    virtual ~TGameClientBase();

    TGameClientBase(const TGameClientBase&) = delete;
    TGameClientBase& operator=(const TGameClientBase&) = delete;

    /// CIP: the client's setup steps, in CipSoft's order: the storages, the
    /// sound system, the QML context, the assets, the sprite resolver, the
    /// appearance instance factory, the minimap; then `initialized`.
    virtual void initialize();

    /// The client version the assets and the login are made for.
    static constexpr int ClientVersion = 1533;
    /// CIP: the window the client is shown in. With a window the sprite
    /// texture provider manager is made, the client's parts connect to the
    /// game action router and `mainWindowInitialized` follows.
    void setMainWindow(QQuickWindow* MainWindow);
    /// CIP: before each frame, at `Timestamp` (milliseconds): the generic
    /// storages advance their appearance instances, the sound manager catches
    /// up and the highlight pulse of the frame is computed. Host: the sound
    /// controller catches up here too.
    void updateWithTimestamp(qint64 Timestamp);

    /// The QML engine the game window and its controllers are built in.
    QQmlEngine* qmlEngine() const { return m_pEngine; }
    /// The window the game window is shown in; null before it exists.
    QQuickWindow* mainWindow() const { return m_pMainWindow; }
    /// Where the client's data files live. Throws std::logic_error without one.
    std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper() const;
    /// The client options the client was made with.
    config::TClientOptions* clientOptions() const { return m_pClientOptions; }
    /// CIP: the client settings (config.ini) the client was made with.
    std::shared_ptr<config::TTibiaConfig> tibiaConfig() const { return m_pTibiaConfig; }
    /// Throws std::logic_error before it exists.
    std::shared_ptr<appearances::TAppearancesManager> appearancesManager() const;
    /// CIP: throws std::logic_error before initializeAssets.
    std::shared_ptr<assets::TAssetManager> assetManager() const;
    /// Throws std::logic_error before it exists.
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> cyclopediaMapStaticDataStorage() const;
    /// The spell table; null before it exists.
    spells::TSpellInfoStorage* spellInfoStorage() const { return m_pSpellInfoStorage.get(); }
    /// The passive ability table.
    game::TPassiveAbilityInfoStorage* passiveAbilityInfoStorage() const { return m_pPassiveAbilityInfoStorage.get(); }
    /// The weapon proficiencies the server sent; emptied when the game session changes.
    proficiency::TProficiencyStorage* proficiencyStorage() const { return m_pProficiencyStorage.get(); }

    /// Throws std::logic_error before it exists.
    std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory() const;
    /// Null before the appearance instance factory exists.
    std::shared_ptr<appearances::TAppearanceInstanceGenericStoragesManager> appearanceInstanceGenericStoragesManager() const
    {
        return m_pAppearanceInstanceGenericStoragesManager;
    }
    /// The object types each NPC buys. Throws std::logic_error before it exists.
    std::shared_ptr<game::TTraderStorage> traderStorage() const;
    /// The client's built-in resource images; null before the storages exist.
    std::shared_ptr<sprites::TResourceSpritesManager> resourceSpritesManager() const { return m_pResourceSpritesManager; }
    /// The client's sprite resolver. Throws std::logic_error before it exists.
    std::shared_ptr<sprites::TSpriteResolver> spriteResolver() const;

    /// Where the minimap's tiles and marker file are read and written.
    /// Throws std::logic_error before it exists.
    std::shared_ptr<minimap::TMinimapDiskIOAsync> minimapTileDiskIO() const;
    /// The minimap tiles the world fills in. Throws std::logic_error before it exists.
    std::shared_ptr<minimap::TMinimapTileStorage> minimapTileStorage() const;
    /// The minimap flags. Throws std::logic_error before it exists.
    std::shared_ptr<minimap::TMinimapMarkerStorage> minimapMarkerStorage() const;
    /// The minimap tiles the renderers draw. Throws std::logic_error before it exists.
    std::shared_ptr<minimap::TMinimapTileManager> minimapTileManager() const;
    /// Host: the map protocol handler of the current game session keeps the
    /// minimap's floor and fields current (null: none). A handler that
    /// already holds a map brings the minimap up to its area at once.
    void setMinimapWorldmapProtocolHandler(worldmap::TWorldmapProtocolMessageHandler* Handler);
    /// Host: the changed markers and every loaded tile are written, and the
    /// disk is waited for (on leaving the world, before the minimap export
    /// and when the client goes).
    void saveMinimapToDisk();

    /// The router the game window's controllers publish into and listen to.
    /// Throws std::logic_error before it exists.
    std::shared_ptr<game::TGameActionRouter> gameActionRouter() const;
    /// The `storeAndResourceBalanceHelper` context property. Throws
    /// std::logic_error before it exists.
    gamewindow::TStoreAndResourceBalanceHelperController* storeAndResourceBalanceHelperController() const;
    /// Throws std::logic_error before the sound system is up.
    std::shared_ptr<sound::TSoundMetadataManager> soundMetadataManager() const;
    /// Null until the sound system is up.
    std::shared_ptr<sound::TSoundConfiguration> soundConfiguration() const { return m_pSoundConfiguration; }
    /// Null unless the sound engine started.
    std::shared_ptr<sound::TSoundController> soundController() const { return m_pSoundController; }
    /// Null before the main window exists.
    std::shared_ptr<renderer::TSpriteTextureProviderManager> spriteTextureProviderManager() const
    {
        return m_pSpriteTextureProviderManager;
    }

protected:
    /// The client is made with its QML engine, the file system helper, the
    /// client options and the client settings; it makes the appearances, the
    /// Cyclopedia map's static data, the passive ability table and the
    /// proficiency storage at once. Host: without client settings the client
    /// takes the application's (config::processTibiaConfig).
    TGameClientBase(QQmlEngine* Engine, std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                    config::TClientOptions* ClientOptions, std::shared_ptr<config::TTibiaConfig> TibiaConfig = nullptr);
    /// The spell table. Host: qtclient's table reads spells.json when it is
    /// made, so the client makes it once the data directories are known
    /// (CipSoft makes an empty one with the client).
    void createSpellInfoStorage();

    /// CIP: the storages the client keeps for the whole process: the trader
    /// storage, the resource sprites manager, the minimap tile and marker
    /// storages and the minimap tile manager (with a disk of its own). Host:
    /// also the spell table (spells.json is readable now); qtclient makes the
    /// appearances, the Cyclopedia map's static data and the game action
    /// router with the client, because the game window's controllers use them
    /// without a running engine (the character selection preview); the
    /// spritesheet manager is the sprite system's.
    virtual void initializeStorages();
    /// CIP: the minimap's disk; CipSoft's minimap manager becomes the root
    /// context's `MinimapManager`; a marker change starts the single-shot
    /// timer whose timeout writes the markers; the storages' disk requests go
    /// to the disk and its results come back to them. Host: CipSoft's bundled
    /// minimap is copied in while the user's minimap has no tiles, and the
    /// tile storage reads the current session's fields through a world feed.
    virtual void initializeMinimap();
    /// CIP: a failed spritesheet decompression reaches the client (the
    /// base does nothing with it); the resource sprites manager becomes the
    /// root context's `ResourceSpritesManager`. Host: qtclient's sprite
    /// resolver is the sprite system's (TSpriteSystem), made with it; it is
    /// handed the minimap tile manager here.
    virtual void initializeSpriteResolver();
    /// CIP: the appearance instance factory of the appearances; the
    /// instance tracker's spritesheet requests go to the spritesheet manager;
    /// then the generic storages manager of the factory, the root context's
    /// `AppearanceInstanceGenericStorageManager`.
    virtual void initializeAppearanceInstanceFactory();
    /// CIP slot: a spritesheet could not be decompressed. The base ignores it.
    virtual void onSpritesheetUncompressFailed(const QString& SpritesheetID);
    /// CIP slot: the assets finished loading. The base ignores it.
    virtual void onAssetsComplete();
    /// CIP slot: the assets could not be loaded. The base ignores it.
    virtual void onAssetsError();
    /// CIP: what the QML engine's root context offers the client's QML: the
    /// store balance helper (`storeAndResourceBalanceHelper`). Host: the mouse
    /// cursor and text copy controllers CipSoft makes here come with the
    /// engine (app::createEngine); the Appearance type helper reads the
    /// appearances through their instance().
    virtual void initializeQmlContext();
    /// CIP: the asset loader and the asset manager, the asset handler with the
    /// spritesheet manager, appearances, sound metadata and sound manager,
    /// trader storage, proficiency storage, the static data callbacks and the
    /// Cyclopedia map's static data. The manager hands the spritesheet
    /// metadata and every loaded asset to the handler and reports completion
    /// and errors to this client; the handler's requests go back to the
    /// manager; a spritesheet missing from the cache is loaded first.
    /// TGameClient starts the loading.
    virtual void initializeAssets();
    /// CIP: the sprite texture provider manager the renderers draw from, the
    /// root context's `SpriteTextureProviderManager`. Host: qtclient's
    /// providers live in a process-wide registry, so the manager does not
    /// take the sprite resolver.
    virtual void initializeSpriteTextureProviderManager();
    /// CIP: the minimap tile manager, the sprite resolver and the Cyclopedia
    /// map's static data publish into the game action router; the router's
    /// actions reach the minimap tile manager, the sprite texture provider
    /// manager, the sprite resolver, the Cyclopedia map's static data and the
    /// sound controller. Host: also the minimap marker storage (qtclient's
    /// storage handles the marker dialog's actions itself).
    virtual void connectToGameActionRouter();
    /// Host: the moc signals of CipSoft's TGameClientBase live in TGameClient.
    virtual void emitInitialized() = 0;
    virtual void emitMainWindowInitialized() = 0;

    /// The sound configuration from the fixed sound settings and the client
    /// options; the sound engine starts unless the settings turn sound off.
    /// Only a started engine gets the metadata manager, the sound manager,
    /// the sound players and the sound controller.
    virtual void initializeSound();

    QQmlEngine* m_pEngine = nullptr;
    QQuickWindow* m_pMainWindow = nullptr;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_pTibiaFileSystemHelper;
    /// Host: qtclient's client options are the process-wide config object
    /// (CipSoft shares them between the application and the client).
    config::TClientOptions* m_pClientOptions = nullptr;
    std::shared_ptr<config::TTibiaConfig> m_pTibiaConfig;
    std::shared_ptr<appearances::TAppearancesManager> m_pAppearancesManager;
    std::shared_ptr<appearances::TAppearanceInstanceFactory> m_pAppearanceInstanceFactory;
    std::shared_ptr<assets::TAssetAsyncLoader> m_pAssetAsyncLoader;
    std::shared_ptr<assets::TAssetManager> m_pAssetManager;
    std::shared_ptr<assets::TAssetHandler> m_pAssetHandler;
    std::shared_ptr<game::TTraderStorage> m_pTraderStorage;
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> m_pCyclopediaMapStaticDataStorage;
    std::unique_ptr<spells::TSpellInfoStorage> m_pSpellInfoStorage;
    std::unique_ptr<game::TPassiveAbilityInfoStorage> m_pPassiveAbilityInfoStorage;
    std::unique_ptr<proficiency::TProficiencyStorage> m_pProficiencyStorage;
    std::shared_ptr<game::TGameActionRouter> m_pGameActionRouter;
    std::unique_ptr<gamewindow::TStoreAndResourceBalanceHelperController> m_pStoreAndResourceBalanceHelperController;
    std::shared_ptr<sound::TSoundConfiguration> m_pSoundConfiguration;
    std::shared_ptr<sound::TSoundController> m_pSoundController;
    std::shared_ptr<sound::TSoundManager> m_pSoundManager;
    std::shared_ptr<sound::TSoundPlayerManager> m_pSoundPlayerManager;
    std::shared_ptr<sound::TSoundMetadataManager> m_pSoundMetadataManager;
    std::shared_ptr<renderer::TSpriteTextureProviderManager> m_pSpriteTextureProviderManager;
    std::shared_ptr<appearances::TAppearanceInstanceGenericStoragesManager> m_pAppearanceInstanceGenericStoragesManager;
    std::shared_ptr<sprites::TResourceSpritesManager> m_pResourceSpritesManager;
    std::shared_ptr<minimap::TMinimapDiskIOAsync> m_pMinimapTileDiskIO;
    std::shared_ptr<minimap::TMinimapTileStorage> m_pMinimapTileStorage;
    std::shared_ptr<minimap::TMinimapMarkerStorage> m_pMinimapMarkerStorage;
    std::shared_ptr<minimap::TMinimapManager> m_pMinimapManager;
    std::shared_ptr<minimap::TMinimapTileManager> m_pMinimapTileManager;
    std::shared_ptr<sprites::TSpriteResolver> m_pSpriteResolver;
    /// Host: the tile manager's own disk (CipSoft does not keep it); it is
    /// told where CipSoft's bundled minimap is once the assets are known.
    std::shared_ptr<minimap::TMinimapDiskIOAsync> m_pMinimapTileManagerDiskIO;
    /// Host: the current session's fields as the tile storage reads them.
    std::unique_ptr<minimap::TMinimapWorldFeed> m_pMinimapWorldFeed;
    QPointer<worldmap::TWorldmapProtocolMessageHandler> m_pMinimapWorldmapProtocolHandler;
    /// CIP: started by a marker change; its timeout writes the markers.
    QTimer m_minimapMarkerStorageTimer;
    /// Host: why the appearances could not be loaded; empty when they were.
    QString m_assetLoadError;
};

}
}
