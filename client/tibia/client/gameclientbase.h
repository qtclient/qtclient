#pragma once

#include <QString>
#include <QtGlobal>

#include <memory>

class QQmlEngine;
class QQuickWindow;

namespace tibia {
namespace appearances {
class TAppearancesManager;
}
namespace config {
class TClientOptions;
}
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
}
namespace game {
class TGameActionRouter;
class TPassiveAbilityInfoStorage;
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
/// map's static data, the spell and passive ability tables, the game action
/// router, the store balance helper and the sound system.
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
    /// sound system, the QML context, the assets; then `initialized`.
    /// Host: CipSoft's sprite resolver, appearance instance factory and
    /// minimap steps have no counterpart here yet (qtclient's sprite system
    /// is the engine's, its minimap is the game session's).
    virtual void initialize();

    /// The client version the assets and the login are made for.
    static constexpr int ClientVersion = 1533;
    /// CIP: the window the client is shown in. With a window the sprite
    /// texture provider manager is made, the client's parts connect to the
    /// game action router and `mainWindowInitialized` follows.
    void setMainWindow(QQuickWindow* MainWindow);
    /// CIP: before each frame, at `Timestamp` (milliseconds): the sound
    /// controller and the sound manager catch up, and the highlight pulse of
    /// the frame is computed.
    void updateWithTimestamp(qint64 Timestamp);

    /// The QML engine the game window and its controllers are built in.
    QQmlEngine* qmlEngine() const { return m_pEngine; }
    /// The window the game window is shown in; null before it exists.
    QQuickWindow* mainWindow() const { return m_pMainWindow; }
    /// Where the client's data files live. Throws std::logic_error without one.
    std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper() const;
    /// The client options the client was made with.
    config::TClientOptions* clientOptions() const { return m_pClientOptions; }
    /// Throws std::logic_error before it exists.
    std::shared_ptr<appearances::TAppearancesManager> appearancesManager() const;
    /// Throws std::logic_error before it exists.
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> cyclopediaMapStaticDataStorage() const;
    /// The spell table; null before it exists.
    spells::TSpellInfoStorage* spellInfoStorage() const { return m_pSpellInfoStorage.get(); }
    /// The passive ability table.
    game::TPassiveAbilityInfoStorage* passiveAbilityInfoStorage() const { return m_pPassiveAbilityInfoStorage.get(); }
    /// The weapon proficiencies the server sent; emptied when the game session changes.
    proficiency::TProficiencyStorage* proficiencyStorage() const { return m_pProficiencyStorage.get(); }

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
    /// The client is made with its QML engine, the file system helper and the
    /// client options; it makes the appearances, the Cyclopedia map's static
    /// data, the passive ability table and the proficiency storage at once.
    TGameClientBase(QQmlEngine* Engine, std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                    config::TClientOptions* ClientOptions);
    /// The spell table. Host: qtclient's table reads spells.json when it is
    /// made, so the client makes it once the data directories are known
    /// (CipSoft makes an empty one with the client).
    void createSpellInfoStorage();

    /// CIP: the storages the client keeps for the whole process. Host: the
    /// spell table (spells.json is readable now); qtclient makes the
    /// appearances, the Cyclopedia map's static data and the game action
    /// router with the client, because the game window's controllers use
    /// them without a running engine (the character selection preview).
    virtual void initializeStorages();
    /// CIP: what the QML engine's root context offers the client's QML: the
    /// store balance helper (`storeAndResourceBalanceHelper`). Host: the mouse
    /// cursor and text copy controllers CipSoft makes here come with the
    /// engine (app::createEngine); the Appearance type helper reads the
    /// appearances through their instance().
    virtual void initializeQmlContext();
    /// CIP: the assets: the Cyclopedia map's static data asks the asset
    /// loading for its files, the client version is set and the assets, the
    /// static data, the Cyclopedia map and the sound bank are read. Host:
    /// qtclient reads them at once; a failure is kept in m_assetLoadError.
    virtual void initializeAssets();
    /// CIP: the sprite texture provider manager the renderers draw from.
    virtual void initializeSpriteTextureProviderManager();
    /// CIP: the minimap tile manager, the sprite resolver and the Cyclopedia
    /// map's static data publish into the game action router; the router's
    /// actions reach them, the sprite texture provider manager and the sound
    /// controller. Host: qtclient's minimap tile manager is the game
    /// session's; the game client connects it when the world is entered.
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
    std::shared_ptr<appearances::TAppearancesManager> m_pAppearancesManager;
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
    /// Host: what initializeAssets could not read; empty when all loaded.
    QString m_assetLoadError;
};

}
}
