#pragma once

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
/// the members and the non-moc behaviour, and TGameClient derives from both.
class TGameClientBase
{
public:
    virtual ~TGameClientBase();

    TGameClientBase(const TGameClientBase&) = delete;
    TGameClientBase& operator=(const TGameClientBase&) = delete;

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
};

}
}
