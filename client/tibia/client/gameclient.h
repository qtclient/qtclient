#pragma once

#include "sessiondump/sessiondumpciptypes.h"
#include "sessiondump/sessiondumpcatalogloader.h"

#include "app/launcherenv.h"
#include "authentication/authenticationprocesscontroller.h"
#include "client/gameclientbase.h"
#include "authentication/loginrequestuploader.h"
#include "config/characterconfigurationmanager.h"
#include "game/gameservergamesession.h"
#include "runtime/gamehost.h"
#include "qtconnectionwrapper.h"

#include <cip_gen.h>

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <deque>
#include <memory>
#include <vector>

namespace tibia::systemsurvey {
class TFPSHistory;
class TFPSHistoryController;
}
#include <optional>

class QQmlEngine;
class QQuickItem;
class QQuickWindow;

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
class TGameSessionDisconnectReactionController;
class TGameWindowController;
}
namespace shared {
class TCrashReportSender;
}
}
namespace tibia {
namespace game {
class IGameSession;
class TGameActionRouter;
class TGameSessionMetaInfo;
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
class TSoundConfiguration;
class TSoundController;
class TSoundManager;
class TSoundMetadataManager;
class TSoundPlayerManager;
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
class TWorldMapStorage;
class TWorldMapViewport;
class TWorldmapProtocolMessageHandler;
}
}
namespace tibia {
namespace chat {
class TChatProtocolMessageHandler;
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
/// login, character selection and the game server session. CipSoft's
/// TGameClientBase members live in TGameClientBase.
class TGameClient final : public cip::tibia::client::TGameClient, public ::tibia::client::TGameClientBase
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

    /// The login's character selection (the authentication process controller's);
    /// null before the game window is built.
    gamewindow::TCharacterSelectionController* characterSelection() const
    {
        return m_authentication ? m_authentication->characterSelectionController() : nullptr;
    }
    /// The current game session: one per login, replay or tutorial; null between them.
    const std::shared_ptr<game::IGameSession>& currentGameSession() const { return m_currentGameSession; }
    /// The current session when it is a live one; null otherwise.
    game::TGameserverGameSession* session() const;
    /// The current session when it replays a sessiondump (`--replay`, the tutorial); null otherwise.
    game::TSessiondumpGameSession* sessiondump() const;
    network::ExtendedOpcodeRouter* opcodes() const { return m_opcodes.get(); }
    /// The current game session's world map frame; null between sessions.
    qmlcomponents::TWorldMapRenderProvider* worldMapRenderProvider() const;
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
        effects::TEffectStorage* effectStorage() const;
    /// Host shim until the game window's widgets connect to the session: the
    /// current session's sidebars.json, statusBarData.json and actionbars.json
    /// (its character configuration manager holds them); null without one.
    config::TSidebarOptions* sidebarOptions() const;
    game::TStatusbarStorage* statusbarStorage() const;
    config::TActionBarOptions* actionBarOptions() const;
    /// The character's outfit presets.
    game::TOutfitDialogStorage* outfitDialogStorage() const { return m_outfitDialogStorage.get(); }
    /// The current session's; null without one.
    config::TCharacterConfigurationManager* characterConfigurationManager() const;
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
    /// CIP Change Character: show CharacterSelection while online.
    void openCharacterSelectionForSwitch();
    /// Hide CharacterSelection without logging out / quitting (in-game Switch toggle).
    void closeCharacterSelection();
    /// CIP Ctrl/Cmd+G / sidebar Switch: toggle in-game CharacterSelection while online.
    void toggleCharacterSelectionForSwitch();
    /// Start screen must show the character list or the login mask, never neither.
    void ensureLoginSurface();
    /// Abort on CIP's connecting dialog: drop the login HTTP request, or the
    /// game connection if the world is not entered yet.
    void abortPendingEnter();
    /// CIP: another character while online: this one logs out, the chosen one
    /// logs in with a new game session once the world is left. False when
    /// offline or a change is already pending.
    bool requestCharacterSwitch(const QString& characterName);
    /// End a tutorial sessiondump cinematic and connect to the pending live world.
    void finishTutorialCinematic();
    /// CIP: the player wants to leave the client. Without a game session it
    /// closes right away; in the tutorial it asks Exit / Cancel, in any other
    /// session Exit / Logout / Cancel.
    void exitClient();

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
    /// A character change was requested: the current character logs out first.
    void characterSwitchRequested(const QString& characterName);
    /// The session ended; mainWindowInitialized follows the next world entry.
    void worldLeft();
    /// Account email after CipSoft password login / CLI (for remember-email).
    void sessionEmailAvailable(const QString& email);

public Q_SLOTS:
    /// CIP: takes the next meta info off the queue, builds its game session,
    /// makes it the current session and starts it.
    void autoconnectNextGameSessionInQueue() override;
    /// CIP: the current session disconnected (or failed to connect): the
    /// dialogs that only make sense in a session close; the next queued
    /// session connects, or with none left there is no current session (and
    /// the client logs in again when the player asked for it).
    void onGameSessionDisconnected();
    /// CIP: with FPS tracking on, samples the frame rate every
    /// [FPSHISTORY] sampleRateMsec and uploads the history every uploadRateSec
    /// (and when the session ends or the client closes); off drops both.
    void onFpsTrackingActiveChanged(bool FpsTrackingActive);
    /// CIP: the client window changed its size: every open multi action list
    /// of the action bars closes.
    void onWindowSizeChanged() override;
    /// CIP: the game window's Quit.
    void onExitClientRequested();
    /// CIP: the exit confirmation's answer. The dialog closes; Logout logs the
    /// character out (QUIT_GAME), Exit closes the client, Cancel stays.
    void onConfirmOrCancelExitGame(
        cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier);

private:
    struct TGameWindow;
    struct TGameWindowDeleter
    {
        void operator()(TGameWindow* gameWindow) const;
    };

    /// Logs the session out; without one, shows the start screen when given.
    void requestLogoutOrShowStartScreen(gamewindow::TGameWindowController* startScreen);
    /// CIP TGameClientBase's sound system, plus the connections qtclient
    /// makes with it here (the router, the sound metadata, the title music).
    void initializeSound() override;
    /// CIP: (re)arms the FPS sample and upload timers for the current history.
    void startFpsHistoryTimers(qint64 sampleRateMsec, qint64 uploadRateSec);
    /// CIP: posts the pending crash report to the configured crashReportUrl
    /// (the dumps only for a full report), then deletes the crash dumps.
    void sendCrashReport(bool SendFullReport);
    void login();
    void onLoginSuccessful(const authentication::LoginResult& result);
    /// The character list with the account's premium state.
    void showAccount(const authentication::TCharacterList& characters);
    /// CIP: makes the authentication process controller and its parts for the
    /// login service at `LoginUrl`, then connects the game client to them.
    void createAuthenticationProcessController(const QUrl& LoginUrl);
    authentication::TLoginRequestUploader* loginRequestUploader() const;
    void onCharacterChosen(const QString& characterName);
    void onWorldEntered();
    /// Writes the session's minimap and character options.
    void onWorldLeft();
    /// CIP: the meta infos of a character login: a tutorial character first
    /// watches the tutorial dump, then the live session logs in.
    std::vector<std::shared_ptr<game::TGameSessionMetaInfo>> createGameSessionMetaInfos(
        const game::GameserverCredentials& loginData, const QString& tutorialSessiondump) const;
    /// The settings every session's meta info carries (queue delays, effect
    /// and chat limits, movement).
    void setGameSessionSettings(game::TGameSessionMetaInfo& metaInfo) const;
    /// Queues the meta infos and connects the first one.
    void enqueueGameSessions(std::vector<std::shared_ptr<game::TGameSessionMetaInfo>> metaInfos);
    /// CIP: makes `GameSession` the current game session (null: none). The
    /// previous one is unhooked and released; every game window controller
    /// connects to the new one, in CipSoft's order.
    void setCurrentGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// qtclient's own reactions to the current session (world entry, an
    /// outdated client), until the game client's CipSoft slots replace them.
    void connectToCurrentGameSession();
    /// The game window controllers' connectors, in CipSoft's order (the
    /// connector calls of setCurrentGameSession); in gameclientgamewindow.cpp.
    void connectGameWindowControllersToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Logs the last chosen character in again (after a login wait, or when the
    /// player logged out to log in again).
    void loginAgainWithLastCharacter();
    /// The game window's disconnect reaction; null before the game window exists.
    gamewindow::TGameSessionDisconnectReactionController* gameSessionDisconnectReactionController() const;
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
    std::shared_ptr<systemsurvey::TFPSHistory> m_fpsHistory;
    std::shared_ptr<systemsurvey::TFPSHistoryController> m_fpsHistoryController;
    QTimer m_fpsSampleTimer;
    QTimer m_fpsUploadTimer;
    std::unique_ptr<worldmap::TNextFrameTimestampPredictor> m_predictor;
    bool m_frameRateLimitEnabled = false;
    /// Guards against nested afterAnimating/tick while NFU holds the predicted clock.
    bool m_inTick = false;
    int m_frameRateLimit = 60;
    int m_activeFrameRateLimit = 0;
    qint64 m_frameGridBase = 0;
    qint64 m_frameSlot = 0;
    qint64 m_frameInterval = 0;
    /// The account login's state machine with its character selection,
    /// uploaders and Cloudflare challenge (CIP: made with the game window); the
    /// game client still performs the login steps it enters.
    std::shared_ptr<authentication::TAuthenticationProcessController> m_authentication;
    /// True while CharacterSelection is open for an in-game Change Character switch.
    bool m_characterSelectionOpen = false;
    /// CIP: the sessions to play next, in order.
    std::deque<std::shared_ptr<const game::TGameSessionMetaInfo>> m_gameSessionMetaInfoQueue;
    std::shared_ptr<game::IGameSession> m_currentGameSession;
    /// CIP: the current session's disconnected and connectionError, once and
    /// on the next event loop pass (the session is not released inside its own
    /// signal).
    shared::TQtConnectionWrapper m_gameSessionDisconnected{ 1, true };
    std::unique_ptr<network::ExtendedOpcodeRouter> m_opcodes;
    std::unique_ptr<network::TGameserverDualConnection> m_dualConnection;
    std::unique_ptr<game::TOutfitDialogStorage> m_outfitDialogStorage;
    /// Host shim: the character configuration storages the game window still
    /// owns; each new session's manager takes them.
    config::TCharacterConfigurationManager::TStorages m_characterConfigurationStorages;
    /// The chat pane's ignore list, applied to each session's message handler.
    chat::TWhiteBlacklist* m_messageIgnoreList = nullptr;
    std::unique_ptr<minimap::TMinimapManager> m_minimap;
    // CIP sessiondump surfaces (live replay is TSessiondumpGameSession).
    sessiondump::TSessiondumpCatalogLoader m_sessiondumpCatalogLoader;
    sessiondump::TSessiondumpProgressClockService m_sessiondumpProgressClockService;
    authentication::LoginResult m_account;
    int m_worldType = -1;
    bool m_started = false;
    /// Logged in once the current character has left the world (CIP switch).
    std::optional<QString> m_pendingCharacter;
    /// Destroyed first: its objects use the client's storages and session.
    std::unique_ptr<TGameWindow, TGameWindowDeleter> m_gameWindow;
};

}
}
