#pragma once

#include "sessiondump/sessiondumpciptypes.h"

#include "app/launcherenv.h"
#include "authentication/loginrequestuploader.h"
#include "config/characterconfigurationmanager.h"
#include "game/gameservergamesession.h"
#include "runtime/gamehost.h"

#include <cip_gen.h>

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <vector>
#include <optional>

class QQmlEngine;
class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace renderer {
class TNextFrameUpdater;
}
}
namespace tibia {
namespace authentication {
class TCloudflareChallengeController;
struct TCloudflareChallengeClearanceInfo;
enum class TCloudflareChallengeError : int;
}
}
namespace tibia {
namespace gamewindow {
class TCharacterSelectionController;
class TCrashReportDialogController;
class TGameWindowController;
struct CharacterEntry;
}
namespace shared {
class TCrashReportSender;
}
}
namespace tibia {
namespace game {
class TOutfitDialogStorage;
class TGameserverGameSession;
class TSessiondumpGameSession;
class TStatusbarStorage;
}
}
namespace tibia {
namespace network {
class ExtendedOpcodeRouter;
class TGameserverDualConnection;
}
}
namespace tibia {
namespace sound {
class TSoundProtocolMessageHandler;
class TSoundStorage;
}
}
namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}
namespace tibia {
namespace config {
class TActionBarOptions;
class TSidebarOptions;
}
}
namespace tibia {
namespace minimap {
class TMinimapManager;
}
}
namespace tibia {
namespace worldmap {
class TAmbientLightStorage;
class TFrameDurationRing;
class TNextFrameTimestampPredictor;
class TNumericalEffectStorage;
class TWorldMapStorage;
class TWorldMapViewport;
class TWorldmapProtocolMessageHandler;
}
}
namespace tibia {
namespace chat {
class TGameMessageHandler;
class TWhiteBlacklist;
class TTextStorage;
}
}
namespace tibia {
namespace effects {
class TEffectStorage;
class TEffectGameActionHandler;
class TEffectProtocolMessageHandler;
}
}
namespace tibia {
namespace creatures {
class TCreatureStorage;
}
}
namespace tibia {
namespace container {
class TContainerStorage;
class TInventoryContainer;
}
}

namespace tibia {
namespace client {

/// The client's top level (CIP: TGameClient): engine pump, assets, account
/// login, character selection and the game server session.
class TGameClient final : public cip::tibia::client::TGameClient
{
    Q_OBJECT
public:
    struct Options
    {
        app::LauncherEnv env;
        /// Account login without SSO (dev).
        QString account;
        QString password;
        /// Picks this character as soon as the list arrives.
        QString autoCharacter;
        /// Logs out and quits this many seconds after entering the world (smoke tests); 0 keeps playing.
        int playForSeconds = 0;
        /// Said one per second after entering the world (smoke tests).
        QStringList say;
        /// Directions (n,e,se, ...) or relative goals (dx:dy) to walk after entering the world (smoke tests).
        QStringList walk;
        /// Records the game session to this file.
        QString recordTo;
        /// Plays this recorded session instead of logging in.
        QString replay;
    };

    /// What the game window is built in, and how the session starts.
    struct TGameWindowSetup
    {
        QQmlEngine* engine = nullptr;
        QQuickWindow* window = nullptr;
        /// clientwindow.qml's placeholder the game window goes into.
        QQuickItem* placeholder = nullptr;
        app::LauncherEnv env;
        /// Character selection with sample characters, no session.
        bool preview = false;
        /// The list CharacterSelection.qml shows.
        gamewindow::TCharacterSelectionController* characterSelection = nullptr;
        Options options;
        QString argv0;
        QString workDir;
    };

    explicit TGameClient(QObject* parent = nullptr);
    ~TGameClient() override;

    /// Creates the game window and its controllers, storages and input
    /// handlers, connects them and (unless previewing) starts the session.
    /// False when a QML file does not load or the engine does not start.
    bool createGameWindow(const TGameWindowSetup& setup);

    /// Starts the engine, loads assets and logs in. False when the engine
    /// cannot start (no work dir).
    bool start(const QString& argv0, const QString& workDir, const Options& options);
    void shutdown();
    /// Paces the game by this window's frames.
    void setWindow(QQuickWindow* window);
    /// CIP: a crash dump of an earlier run opens the crash report dialog
    /// with its annotations; its sender waits for the player's choice.
    /// CipSoft calls it at the end of the constructor; qtclient once the
    /// dialog stack exists.
    void checkForCrashDump();

    gamewindow::TCharacterSelectionController* characterSelection() const { return m_characterSelection.get(); }
    game::TGameserverGameSession* session() const { return m_session.get(); }
    /// CIP-shaped replay session when `--replay` is set; null for live login.
    game::TSessiondumpGameSession* sessiondump() const { return m_sessiondump.get(); }
    network::ExtendedOpcodeRouter* opcodes() const { return m_opcodes.get(); }
    qmlcomponents::TWorldMapRenderProvider* worldMapRenderProvider() const { return m_worldMapRenderProvider.get(); }
    /// The view the world map shows: centred by the full map, read by the picker.
    worldmap::TWorldMapViewport* worldMapViewport() const;
    /// Pack-facing façade (signals + peels). Same instance for the process lifetime.
    runtime::TGameHost* gameHost() const { return m_gameHost; }
    /// Tick-to-swap durations for the FPS indicator's currentFrameCount.
    worldmap::TFrameDurationRing* frameDurations() const { return m_frameDurations.get(); }
    /// The active game session's storages (null while no session is played).
    worldmap::TAmbientLightStorage* ambientLight() const;
    worldmap::TWorldMapStorage* worldMapStorage() const;
    creatures::TCreatureStorage* creatureStorage() const;
    container::TContainerStorage* containerStorage() const;
    container::TInventoryContainer* inventoryContainer() const;
    worldmap::TWorldmapProtocolMessageHandler* worldmapProtocol() const;
    /// Speech bubbles and screen messages; null before start.
    chat::TTextStorage* textStorage() const;
    /// Wire the chat pane's ignore list into map bubbles (CIP handler +0x98).
    void setMessageIgnoreList(chat::TWhiteBlacklist* list);
    /// The active game session changed: its message handler, frame updater,
    /// view and minimap feed are rebuilt on it.
    void bindActiveSession();
    worldmap::TNumericalEffectStorage* numericalEffects() const;
    effects::TEffectStorage* effectStorage() const;
    /// The character's sidebars.json, loaded on entering the world.
    config::TSidebarOptions* sidebarOptions() const { return m_sidebarOptions.get(); }
    /// The character's statusBarData.json, loaded on entering the world.
    game::TStatusbarStorage* statusbarStorage() const { return m_statusbarStorage.get(); }
    /// The character's actionbars.json.
    config::TActionBarOptions* actionBarOptions() const { return m_actionBarOptions.get(); }
    /// The character's outfit presets.
    game::TOutfitDialogStorage* outfitDialogStorage() const { return m_outfitDialogStorage.get(); }
    /// Null until the game window exists.
    config::TCharacterConfigurationManager* characterConfigurationManager() const { return m_characterConfigurationManager.get(); }
    /// The session's minimap; null outside the world.
    minimap::TMinimapManager* minimap() const { return m_minimap.get(); }
    /// The chosen world's EWorldType (CIP TGameSessionBase +0x34, set by
    /// v147 with worldTypeSet): the login service's raw pvptype,
    /// 0 open, 1 optional, 2 hardcore PvP; -1 before a character is chosen.
    int worldType() const { return m_worldType; }

    static constexpr int ClientVersion = 1533;

    /// The login service's answer for this account; empty before login.
    const authentication::LoginResult& account() const { return m_account; }
    /// The password of a password login (empty after a launcher login): CIP's
    /// game session login data (+0x18) the chat refuses to send .
    const QString& accountPassword() const { return m_options.password; }
    /// The account changed (a character was created): `reply` is the login
    /// service's document with the new character list. Logs in again when
    /// it has none.
    void refreshAccount(const QJsonObject& reply);
    /// Password login from the product login dialog (`make client-start` without SSO).
    void submitCredentials(const QString& account, const QString& password);
    /// CIP Change Character: show CharacterSelection while online (optional lobby alt Online flags).
    void openCharacterSelectionForSwitch(const QJsonArray& alts = {});
    /// Hide CharacterSelection without logging out / quitting (in-game Switch toggle).
    void closeCharacterSelection();
    /// CIP Ctrl/Cmd+G / sidebar Switch: toggle in-game CharacterSelection while online.
    void toggleCharacterSelectionForSwitch();
    /// Start screen must show the character list or the login mask, never neither.
    void ensureLoginSurface();
    /// Abort on CIP's connecting dialog: drop the login HTTP request, or the
    /// game connection if the world is not entered yet.
    void abortPendingEnter();
    /// Send C 0x0D character switch; server runs Alts.switchAlt (same-socket quick switch).
    bool requestCharacterSwitch(const QString& characterName);
    void setSameSocketCharacterSwitch(bool enabled) { m_sameSocketCharacterSwitch = enabled; }
    /// End a tutorial sessiondump cinematic and connect to the pending live world.
    void finishTutorialCinematic();

Q_SIGNALS:
    /// The login service or the game server wants a newer client.
    void clientOutOfDate();
    /// No launcher SSO / --account: show the product login dialog.
    void loginCredentialsNeeded();
    /// A login HTTP request was just sent (password or launcher session).
    void loginStarted();
    /// The account's characters are in characterSelection; show CharacterSelection.qml.
    void characterSelectionRequested();
    void characterSelectionClosed();
    /// Cancel on the login-page character list: show the start-screen login mask again.
    void returnToLoginRequested();
    /// C 0x0D switch request accepted locally — overlay may show until S 0x0D completes.
    void characterSwitchRequested(const QString& characterName);
    /// The session ended; mainWindowInitialized follows the next world entry.
    void worldLeft();
    /// Account email after CipSoft password login / CLI (for remember-email).
    void sessionEmailAvailable(const QString& email);

private:
    struct TGameWindow;
    struct TGameWindowDeleter
    {
        void operator()(TGameWindow* gameWindow) const;
    };

    /// Builds the manager of every character configuration storage: the game client's own
    /// (sidebars, action bars, statusbar, outfit presets) and the given ones.
    void createCharacterConfigurationManager(config::TCharacterConfigurationManager::TStorages storages);
    /// Logs the session out; without one, shows the start screen when given.
    void requestLogoutOrShowStartScreen(gamewindow::TGameWindowController* startScreen);
    /// CIP: posts the pending crash report to the configured crashReportUrl
    /// (the dumps only for a full report), then deletes the crash dumps.
    void sendCrashReport(bool SendFullReport);
    void login();
    void onLoginSuccessful(const authentication::LoginResult& result);
    void onCharacterChosen(const gamewindow::CharacterEntry& character);
    void onWorldEntered();
    /// Writes the session's minimap and character options.
    void onWorldLeft();
    void startTutorialThenLive(const game::GameserverCredentials& live, const QString& dumpPath);
    void connectLive(const game::GameserverCredentials& credentials);
    void fail(const QString& message);
    /// Once per frame, before the scene graph syncs (CIP:).
    void tick();
    /// The frame-rate limiter; false when this frame comes too early.
    bool frameSlotReached(qint64 nowMicros);

    Options m_options;
    std::unique_ptr<gamewindow::TCrashReportDialogController> m_crashReportDialog;
    std::shared_ptr<shared::TCrashReportSender> m_crashReportSender;
    runtime::TGameHost* m_gameHost = nullptr;
    /// Ticks while no frames come (a hidden window).
    QTimer m_idleTick;
    QTimer m_frameLimitTimer;
    std::unique_ptr<worldmap::TFrameDurationRing> m_frameDurations;
    std::unique_ptr<worldmap::TNextFrameTimestampPredictor> m_predictor;
    std::unique_ptr<renderer::TNextFrameUpdater> m_updater;
    bool m_frameRateLimitEnabled = false;
    /// Guards against nested afterAnimating/tick while NFU holds the predicted clock.
    bool m_inTick = false;
    int m_frameRateLimit = 60;
    int m_activeFrameRateLimit = 0;
    qint64 m_frameGridBase = 0;
    qint64 m_frameSlot = 0;
    qint64 m_frameInterval = 0;
    std::unique_ptr<authentication::TLoginRequestUploader> m_login;
    std::unique_ptr<authentication::TCloudflareChallengeController> m_cloudflareChallenge;
    bool m_suppressConnectError = false;
    std::unique_ptr<gamewindow::TCharacterSelectionController> m_characterSelection;
    /// True while CharacterSelection is open for an in-game Change Character switch.
    bool m_characterSelectionOpen = false;
    std::unique_ptr<game::TGameserverGameSession> m_session;
    std::unique_ptr<game::TSessiondumpGameSession> m_sessiondump;
    std::unique_ptr<network::ExtendedOpcodeRouter> m_opcodes;
    std::unique_ptr<network::TGameserverDualConnection> m_dualConnection;
    std::unique_ptr<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    std::unique_ptr<config::TSidebarOptions> m_sidebarOptions;
    std::unique_ptr<game::TStatusbarStorage> m_statusbarStorage;
    std::unique_ptr<config::TActionBarOptions> m_actionBarOptions;
    std::unique_ptr<game::TOutfitDialogStorage> m_outfitDialogStorage;
    std::unique_ptr<config::TCharacterConfigurationManager> m_characterConfigurationManager;
    /// Queue connections of the static protocol ingresses (reconnected per session).
    std::vector<QMetaObject::Connection> m_staticIngressConnections;
    /// The chat pane's ignore list, applied to each session's message handler.
    chat::TWhiteBlacklist* m_messageIgnoreList = nullptr;
    std::unique_ptr<chat::TGameMessageHandler> m_messageHandler;
    std::unique_ptr<minimap::TMinimapManager> m_minimap;
    // CIP sessiondump surfaces (live replay is TSessiondumpGameSession).
    sessiondump::TClientSessiondumpRecorder m_sessiondumpRecorder;
    sessiondump::TSessiondumpAsyncLoader m_sessiondumpAsyncLoader;
    sessiondump::TSessiondumpCatalogLoader m_sessiondumpCatalogLoader;
    sessiondump::TSessiondumpPlayer m_sessiondumpPlayer;
    sessiondump::TSessiondumpProgressClockService m_sessiondumpProgressClockService;
    authentication::LoginResult m_account;
    std::optional<game::GameserverCredentials> m_pendingLiveCredentials;
    int m_worldType = -1;
    bool m_started = false;
    bool m_tutorialCinematic = false;
    bool m_sameSocketCharacterSwitch = false;
    /// Logged in once the current character has left the world (CIP switch).
    std::optional<QString> m_pendingCharacter;
    /// Destroyed first: its objects use the client's storages and session.
    std::unique_ptr<TGameWindow, TGameWindowDeleter> m_gameWindow;
};

}
}
