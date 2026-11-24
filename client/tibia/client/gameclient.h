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
#include <QPointer>
#include <QQuickItem>
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
class TBlessingsDialogController;
class TBossDifficultySelectionDialogController;
class TBugReportDialogController;
class TCharacterSelectionController;
class TCreateAccountDialogController;
class TCreateCharacterDialogController;
class TCrashReportDialogController;
class TDeathDialogController;
class TExaltationForgeDialogController;
class TGameSessionDisconnectReactionController;
class TGameWindowController;
class TMarketController;
class TMessageDialogController;
class TMultiOfflineTrainingDialogController;
class TNaggingMessageDialogController;
class TOutfitMemorialDialogController;
class TReportViolationController;
class TRewardWallDialogController;
class TSelectObjectAmountDialogController;
class TShowImageDialogController;
class TShowSpellBookDialogController;
class TSoulSealsDialogController;
class TStoreDialogController;
}
namespace proficiency {
class TWeaponProficiencyDialogController;
class TWeaponProficiencyReshapeDialogController;
class TWeaponProficiencyShapeDialogController;
class TWeaponProficiencyShapeOptionsDialogController;
}
namespace shared {
class TCrashReportSender;
class TDownloadPool;
class TTibiaFileSystemHelper;
}
}
namespace tibia {
namespace game {
class FPSDropsLogger;
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
}
namespace input {
class TKeyboardStatusStorage;
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
class TLenshelpManager;
class TWorldMapRenderProvider;
}
}
namespace tibia {
namespace config {
class TActionBarOptions;
class TClientOptions;
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

    /// CIP: the client is made with the application's QML engine, the file
    /// system helper and the client options.
    TGameClient(QQmlEngine* Engine, std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                config::TClientOptions* ClientOptions, std::shared_ptr<shared::TDownloadPool> DownloadPool,
                QObject* parent = nullptr);
    ~TGameClient() override;

    /// Creates the game window and its controllers, storages and input
    /// handlers, connects them and (unless previewing) starts the session.
    /// False when a QML file does not load or the engine does not start.
    bool createGameWindow(const TGameWindowSetup& setup);

    /// Host: starts qtclient's engine (data directories, the frame options)
    /// the client's setup steps run on. False when it cannot start (no work dir).
    bool startEngine(const QString& argv0, const QString& workDir);
    /// Logs in with the options' account, or plays the recorded session;
    /// reports the assets initialize() could not read instead.
    void start();
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
    /// The current session's; null without one.
    config::TCharacterConfigurationManager* characterConfigurationManager() const;
    /// The session's minimap; null outside the world.
    minimap::TMinimapManager* minimap() const { return m_minimap.get(); }
    /// The chosen world's EWorldType (CIP TGameSessionBase +0x34, set by
    /// v147 with worldTypeSet): the login service's raw pvptype,
    /// 0 open, 1 optional, 2 hardcore PvP; -1 before a character is chosen.
    int worldType() const { return m_worldType; }

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
    /// Pack ABI: another character while online. The character selection
    /// logs it in (CipSoft's path: this character logs out, the chosen one
    /// logs in once the session disconnected). False when offline or the
    /// account has no character of that name.
    bool requestCharacterSwitch(const QString& characterName);
    /// End a tutorial sessiondump cinematic and connect to the pending live world.
    void finishTutorialCinematic();
    /// CIP: the create-account dialog, made with the game window; null before.
    gamewindow::TCreateAccountDialogController* createAccountDialogController() const
    {
        return m_pCreateAccountDialogController.get();
    }
    /// CIP: the create-character dialog, made with the game window; null before.
    gamewindow::TCreateCharacterDialogController* createCharacterDialogController() const
    {
        return m_pCreateCharacterDialogController.get();
    }
    /// Host (pack skin): `Controller` replaces the create-character dialog; the
    /// client takes it over and deletes the previous one later.
    void setCreateCharacterDialogController(gamewindow::TCreateCharacterDialogController* Controller);
    /// CIP: the player wants to leave the client. Without a game session it
    /// closes right away; in the tutorial it asks Exit / Cancel, in any other
    /// session Exit / Logout / Cancel.
    void exitClient();
    /// CIP: the sidebars check their bounds again (true) or not while the client window changes.
    void setSidebarTriggerCheckBoundsEnabled(bool Enabled);

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
    /// Host: the character entered the world (the game window shows the game
    /// screen; the packs' TGameHost::mainWindowInitialized).
    void worldEntered();
    /// The session ended; worldEntered follows the next world entry.
    void worldLeft();
    /// CIP: the sidebar manager's sidebar count (the application widens the client window).
    void numberOfSidebarsChanged(std::size_t NewNumber);
    /// Account email after CipSoft password login / CLI (for remember-email).
    void sessionEmailAvailable(const QString& email);
    /// CIP: the current game session disconnected (or failed to connect);
    /// emitted right after onGameSessionDisconnected handled it.
    void currentGamesessionDisconnected(std::shared_ptr<tibia::game::IGameSession> pCurrentGameSession);

public Q_SLOTS:
    /// CIP: takes the next meta info off the queue, builds its game session,
    /// makes it the current session and starts it.
    void autoconnectNextGameSessionInQueue() override;
    /// CIP: the current session disconnected (or failed to connect): the
    /// dialogs that only make sense in a session close; the next queued
    /// session connects, or with none left there is no current session (and
    /// the client logs in again when the player asked for it).
    void onGameSessionDisconnected();
    /// CIP: the authentication process controller wants the chosen
    /// character in the game. Without a session the connecting dialog shows
    /// and the connection starts on the next event loop pass; with one (a
    /// character change) it is quit, so that the client logs in again once
    /// it disconnected.
    void connectClientToGameserverWithExistingCredentials() override;
    /// CIP: the queue gets the game sessions of the authentication process
    /// controller's character login (exactly one character), then the first
    /// one connects.
    void onConnectClientToGameserver();
    /// CIP: Cancel on the connecting dialog: no reconnect, the current
    /// session goes and the character selection shows again.
    void onAbortConnectClientToGameserver();
    /// CIP: the character selection shows again (through the authentication
    /// process controller).
    void onShowCharacterSelectionDialogRequested();
    /// CIP: the game server accepted the connection: the login process ends
    /// and the FPS drops logger learns the character and its world.
    void onGameSessionConnected();
    /// CIP: the window title and the crash report name the character.
    void onGameSessionLoginSuccessful();
    /// CIP: the game server's login advice in a message box (OK).
    void onGameSessionLoginAdvice(const QString& LoginAdvice);
    /// CIP: a refused game login closes the connecting dialog (the disconnect
    /// reaction controller shows the error).
    void onGameSessionLoginError(const QString& LoginError, int Action);
    /// CIP: during a live session a changed walk key state becomes the
    /// player's movement intent (SET_PLAYER_MOVEMENT_INTENT).
    void onKeyboardStatusStorageChanged();
    /// CIP: with FPS tracking on, samples the frame rate every
    /// [FPSHISTORY] sampleRateMsec and uploads the history every uploadRateSec
    /// (and when the session ends or the client closes); off drops both.
    void onFpsTrackingActiveChanged(bool FpsTrackingActive);
    /// CIP: the client window changed its size: every open multi action list
    /// of the action bars closes.
    void onWindowSizeChanged() override;
    /// CIP: the game window's Quit.
    void onExitClientRequested();
    /// CIP: the login mask's credentials start the account login.
    void onRequestLoginWithCredentials(const QString& EMailAddress, const QString& Password);
    /// CIP: the start screen's Create Account opens the create-account dialog.
    void onRequestCreateAccount();
    /// CIP: opens the create-character dialog.
    void onRequestCreateCharacter();
    /// CIP: the exit confirmation's answer. The dialog closes; Logout logs the
    /// character out (QUIT_GAME), Exit closes the client, Cancel stays.
    void onConfirmOrCancelExitGame(
        cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier);

private:
    struct TGameWindow;
    /// CIP (before each frame): the game window's per-frame steps (the death dialog's countdown).
    void beforeRenderFrameGameWindow();
    struct TGameWindowDeleter
    {
        void operator()(TGameWindow* gameWindow) const;
    };

    /// Logs the session out; without one, shows the start screen when given.
    void requestLogoutOrShowStartScreen(gamewindow::TGameWindowController* startScreen);
    /// CIP TGameClientBase's sound system, plus the connections qtclient
    /// makes with it here (the router, the sound metadata, the title music).
    void initializeSound() override;
    void emitInitialized() override;
    void emitMainWindowInitialized() override;
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
    /// Host glue: the character selection's choice is the authentication
    /// process controller's character login (it then asks the client to
    /// connect).
    void onCharacterChosen(const QString& characterName);
    void onWorldEntered();
    /// Writes the session's minimap and character options.
    void onWorldLeft();
    /// CIP: the meta infos of a character login: the live session with the
    /// account's login data and the character; a tutorial character first
    /// watches the tutorial dump.
    std::vector<std::shared_ptr<game::TGameSessionMetaInfo>> createGameSessionMetaInfos(
        const authentication::TLoginRequestData& LoginData, const gamewindow::TCharacterLoginData& CharacterLoginData);
    /// The settings every session's meta info carries (queue delays, effect
    /// and chat limits).
    void setGameSessionSettings(game::TGameSessionMetaInfo& metaInfo) const;
    /// Queues the meta infos and connects the first one.
    void enqueueGameSessions(std::vector<std::shared_ptr<game::TGameSessionMetaInfo>> metaInfos);
    /// CIP: during a session the window title names the character of the
    /// login; without one it has no extension.
    void updateWindowTitleExtension();
    /// CIP: during a session a crash report names the character and its
    /// world; without one the character, world, position and world address
    /// annotations go.
    void updateCrashReportAnnotations();
    /// CIP: a crash report's Character and Server annotations.
    static void setCharacterCrashReportAnnotations(const QString& CharacterName, const QString& WorldName);
    /// CIP: no Character, Server and Position annotations.
    static void removeCharacterCrashReportAnnotations();
    /// CIP: no UnprotectedWorldAddress and ProtectedWorldAddress annotations.
    static void removeWorldAddressCrashReportAnnotations();
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
    /// The game window's keyboard status; null before the game window exists.
    input::TKeyboardStatusStorage* keyboardStatusStorage() const;
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
    /// CIP +0xaa8: reports frame rate drops while FPS tracking is on.
    std::shared_ptr<game::FPSDropsLogger> m_pFPSDropsLogger;
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
    /// CIP: the client window's placeholder the game window is in (its
    /// windowTitleExtension names the character).
    QPointer<QQuickItem> m_gameWindowPlaceholder;
    int m_worldType = -1;
    bool m_started = false;
    // CIP: the game window's controllers the client owns, in CipSoft's member
    // order (showGameWindow makes them; the rest still live in TGameWindow).
    std::unique_ptr<gamewindow::TMarketController> m_pMarketController;
    std::unique_ptr<gamewindow::TReportViolationController> m_pReportViolationController;
    std::unique_ptr<gamewindow::TBugReportDialogController> m_pBugReportDialogController;
    /// The dialogs and returns when the current session cannot connect or ends.
    std::shared_ptr<gamewindow::TGameSessionDisconnectReactionController> m_pGameSessionDisconnectReactionController;
    std::unique_ptr<gamewindow::TDeathDialogController> m_pDeathDialogController;
    std::unique_ptr<gamewindow::TStoreDialogController> m_pStoreDialogController;
    std::unique_ptr<gamewindow::TMultiOfflineTrainingDialogController> m_pMultiOfflineTrainingDialogController;
    std::unique_ptr<gamewindow::TBlessingsDialogController> m_pBlessingsDialogController;
    std::unique_ptr<gamewindow::TSelectObjectAmountDialogController> m_pSelectObjectAmountDialogController;
    std::unique_ptr<gamewindow::TExaltationForgeDialogController> m_pExaltationForgeDialogController;
    std::unique_ptr<gamewindow::TShowImageDialogController> m_pShowImageDialogController;
    std::unique_ptr<gamewindow::TShowSpellBookDialogController> m_pShowSpellBookDialogController;
    std::unique_ptr<gamewindow::TNaggingMessageDialogController> m_pNaggingMessageDialogController;
    std::unique_ptr<proficiency::TWeaponProficiencyDialogController> m_pWeaponProficiencyDialogController;
    std::unique_ptr<proficiency::TWeaponProficiencyReshapeDialogController> m_pWeaponProficiencyReshapeDialogController;
    std::unique_ptr<proficiency::TWeaponProficiencyShapeDialogController> m_pWeaponProficiencyShapeDialogController;
    std::unique_ptr<proficiency::TWeaponProficiencyShapeOptionsDialogController> m_pWeaponProficiencyShapeOptionsDialogController;
    std::unique_ptr<gamewindow::TSoulSealsDialogController> m_pSoulSealsDialogController;
    std::unique_ptr<gamewindow::TBossDifficultySelectionDialogController> m_pBossDifficultySelectionDialogController;
    std::unique_ptr<qmlcomponents::TLenshelpManager> m_pLenshelpManager;
    std::unique_ptr<gamewindow::TOutfitMemorialDialogController> m_pOutfitMemorialDialogController;
    std::unique_ptr<gamewindow::TRewardWallDialogController> m_pRewardWallDialogController;
    std::unique_ptr<gamewindow::TCreateAccountDialogController> m_pCreateAccountDialogController;
    std::unique_ptr<gamewindow::TCreateCharacterDialogController> m_pCreateCharacterDialogController;
    /// "Connecting to the game world" (Cancel aborts).
    std::unique_ptr<gamewindow::TMessageDialogController> m_pConnectingDialogController;
    /// The exit confirmation (Exit / Logout / Cancel, or Exit / Cancel in the tutorial).
    std::unique_ptr<gamewindow::TMessageDialogController> m_pExitGameDialogController;
    /// The game server's login advice.
    std::unique_ptr<gamewindow::TMessageDialogController> m_pLoginAdviceDialogController;
    /// Destroyed first: its objects use the client's storages and session.
    std::unique_ptr<TGameWindow, TGameWindowDeleter> m_gameWindow;
};

}
}
