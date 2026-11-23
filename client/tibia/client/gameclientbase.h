#pragma once

#include <memory>

class QQmlEngine;
class QQuickWindow;

namespace tibia {
namespace game {
class TGameActionRouter;
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
/// game action router, the store balance helper and the sound system.
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

    /// The router the game window's controllers publish into and listen to.
    /// Throws std::logic_error before it exists.
    std::shared_ptr<game::TGameActionRouter> gameActionRouter() const;
    /// The `storeAndResourceBalanceHelper` context property. Throws
    /// std::logic_error before it exists.
    gamewindow::TStoreAndResourceBalanceHelperController* storeAndResourceBalanceHelperController() const;
    /// Throws std::logic_error before the sound system is up.
    std::shared_ptr<sound::TSoundMetadataManager> soundMetadataManager() const;

protected:
    TGameClientBase();

    /// The sound configuration from the fixed sound settings and the client
    /// options; the sound engine starts unless the settings turn sound off.
    /// Only a started engine gets the metadata manager, the sound manager,
    /// the sound players and the sound controller.
    virtual void initializeSound();

    QQmlEngine* m_pEngine = nullptr;
    QQuickWindow* m_pMainWindow = nullptr;
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
