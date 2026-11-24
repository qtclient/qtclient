#include "gameclient.h"
#include "assets/assetmanager.h"
#include "tutorial/sessiondumphintsprocessor.h"
#include "game/gameactionrouter.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/soundhelper.h"
#include <stdexcept>

#include "config/actionbaroptions.h"
#include "config/characterconfigurationmanager.h"
#include "colorhelper.h"
#include "creatures/movementtweens.h"
#include "teamfinder/teamfinderciptypes.h"
#include "friends/friendsciptypes.h"
#include "spells/spellsciptypes.h"
#include "screenshot/screenshotciptypes.h"
#include "reporting/reportingciptypes.h"
#include "questlog/questlogciptypes.h"
#include "protocol/protocolciptypes.h"
#include "proficiency/proficiencyciptypes.h"
#include "prey/preygameactionhandler.h"
#include "prey/preyprotocolmessagehandler.h"
#include "news/newsstorage.h"
#include "analysers/itempricestorage.h"
#include "proficiency/proficiencystorage.h"
#include "shared/tibiaclipboardhelper.h"
#include "shared/tibiaurltools.h"

#include <algorithm>
#include "imbuing/imbuingciptypes.h"
#include "highscores/highscoresciptypes.h"
#include "exivaoptions/exivaoptionsgameactionhandler.h"
#include "exivaoptions/exivaoptionsprotocolmessagehandler.h"
#include "exaltation/exaltationgameactionhandler.h"
#include "exaltation/exaltationprotocolmessagehandler.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "crashhandler.h"
#include "crashreportdata.h"
#include "crashreportsender.h"
#include "chat/chatchannel.h"
#include "config/clientoptionsuploader.h"
#include "minimap/minimaptilestorage.h"
#include "news/newsdiskio.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/targetselectioncontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/createaccountdialogcontroller.h"
#include "gamewindow/createcharacterdialogcontroller.h"
#include "gamewindow/gamesessiondisconnectreactioncontroller.h"
#include "gamewindow/storeandresourcebalancehelpercontroller.h"
#include "gamewindow/blessingsdialogcontroller.h"
#include "gamewindow/bossdifficultyselectiondialogcontroller.h"
#include "gamewindow/bugreportdialogcontroller.h"
#include "gamewindow/configurecreaturepodiumdialogcontroller.h"
#include "gamewindow/cyclopediadialogcontroller.h"
#include "gamewindow/deathdialogcontroller.h"
#include "gamewindow/edittextcontroller.h"
#include "gamewindow/highscoresdialogcontroller.h"
#include "gamewindow/infobannercontroller.h"
#include "gamewindow/linkcopywarningdialogcontroller.h"
#include "gamewindow/outfitdialogcontroller.h"
#include "gamewindow/preydialogcontroller.h"
#include "gamewindow/questlogcontroller.h"
#include "gamewindow/servermodaldialogcontroller.h"
#include "gamewindow/exaltationforgedialogcontroller.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/naggingmessagedialogcontroller.h"
#include "gamewindow/reportviolationcontroller.h"
#include "gamewindow/rewardwalldialogcontroller.h"
#include "gamewindow/selectobjectamountdialogcontroller.h"
#include "gamewindow/storedialogcontroller.h"
#include "game/fpsdropslogger.h"
#include "proficiency/weaponproficiencydialogcontroller.h"
#include "proficiency/weaponproficiencyshapedialogs.h"
#include "qmlcomponents/lenshelpmanager.h"
#include "shared/tibiafilesystemhelper.h"
#include "config/configciptypes.h"
#include "config/tibiaconfig.h"
#include "appearances/appearanceinstance.h"
#include "bossbestiary/bossbestiaryciptypes.h"
#include "authentication/cloudflarechallengecontroller.h"
#include "authentication/emailcoderequestuploader.h"
#include "authentication/loginconfirmationcoderequestuploader.h"
#include "shared/sharedciptypes.h"
#include "game/gameciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "creatures/vipstorage.h"
#include "trade/tradeciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"
#include "sprites/spriteresolver.h"
#include "sprites/spritesystem.h"
#include "assets/assetasyncloader.h"
#include "store/storestorage.h"
#include "tutorial/tutorialciptypes.h"

#include "app/bootstrap.h"
#include "app/clientoptions.h"
#include "chat/chatprotocolmessagehandler.h"
#include "minimap/minimapmarkerstorage.h"
#include "chat/textstorage.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "input/genericgameactionhandler.h"
#include "input/domainhandlers.h"
#include "input/eventfiltermanager.h"
#include "input/keyboardinputtogameactionconverter.h"
#include "input/keyboardshortcuthandler.h"
#include "input/keyboardstatusstorage.h"
#include "input/keyeventhandler.h"
#include "input/playermovementinputinterpreter.h"
#include "input/recordkeyboardshortcuthandler.h"
#include "chat/chatmodemanager.h"
#include "gamewindow/dialogmanager.h"
#include "qmlcomponents/appearanceinstancerendererbase.h"
#include "qmlcomponents/speechbubbleoverlay.h"
#include "game/currentgamesession.h"
#include "game/tgamenotify.h"
#include <tibia/game/gamenotify.h>
#include "assets/things.h"
#include "input/playermovementintenthandler.h"
#include "input/playermovementintent.h"
#include "protocol/clientmessages.h"
#include "client/magiceffectopacity.h"
#include "config/clientsettings.h"
#include "config/sidebaroptions.h"
#include "clockservice.h"
#include "downloadpool.h"
#include "jsondocumenthttppostsender.h"
#include "qnetworkaccessmanagerwrapper.h"
#include "systemsurvey/fpscounter.h"
#include "systemsurvey/fpshistory.h"
#include "systemsurvey/fpshistorycontroller.h"
#include "systemsurvey/fpshistorytojsonconverter.h"
#include "systemsurvey/fpshistoryuploader.h"
#include "systemsurvey/slidingfpscounter.h"
#include "controllers/characterselectioncontroller.h"
#include "game/gameservergamesession.h"
#include "game/sessiondumpdmpd.h"
#include "game/sessiondumpgamesession.h"
#include "game/tutorialsessiondumpgamesession.h"
#include "protocol/protocolmessagequeue.h"
#include "game/tutorialsessiondumpgamesessionmetainfo.h"
#include "game/sessiondumpgamesessionmetainfo.h"
#include "game/gameservergamesessionmetainfo.h"
#include "game/statusbarstorage.h"
#include "input/gameactionhandlers.h"
#include "minimap/minimapdiskio.h"
#include "gamewindow/stashdialogcontroller.h"
#include "gamewindow/taskboarddialogcontroller.h"
#include "input/gameactionmapper.h"
#include "minimap/waypointcosts.h"
#include "network/extendedopcoderouter.h"
#include "sound/soundprotocolmessagehandler.h"
#include "config/clientoptions.h"
#include "sound/gamesessionsoundprovider.h"
#include "sound/objectsoundcontroller.h"
#include "sound/soundconfiguration.h"
#include "sound/soundcontroller.h"
#include "sound/soundengine.h"
#include "sound/soundmanager.h"
#include "sound/soundmetadatamanager.h"
#include "sound/soundplayermanager.h"
#include "sound/soundstorage.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/nextframetimestamppredictor.h"
#include "effects/effectprotocolmessagehandler.h"
#include "effects/effectstorage.h"
#include "effects/numericaleffect.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "container/containerciptypes.h"
#include "creatures/creaturesciptypes.h"
#include "creatures/creaturestorage.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

#include <client/outfit.h>
#include "protocol/clientmessages.h"
#include "shared/tibiafilesystemhelper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QQmlContext>
#include <QQmlEngine>
#include <QJsonObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>

#include <utility>
#include "game/mapsession.h"
#include <client/position.h>
#include <QDir>

#include <csignal>

namespace tibia::client {

namespace {

/// CIP: a game session's router and the client's hand each other the actions
/// their controllers publish (the router ids keep an action from coming back).
void linkGameActionRouters(game::TGameActionRouter& client, game::TGameSessionBase& session)
{
    game::TInternalGameActionRouter* own = client.internalRouter();
    game::TInternalGameActionRouter* other = session.gameActionRouter()->internalRouter();
    QObject::connect(own, &game::TInternalGameActionRouter::publishGameActionBetweenRouters, other,
                     &game::TInternalGameActionRouter::handleGameActionFromOtherRouter);
    QObject::connect(other, &game::TInternalGameActionRouter::publishGameActionBetweenRouters, own,
                     &game::TInternalGameActionRouter::handleGameActionFromOtherRouter);
}

void unlinkGameActionRouters(game::TGameActionRouter& client, game::TGameSessionBase& session)
{
    game::TInternalGameActionRouter* own = client.internalRouter();
    game::TInternalGameActionRouter* other = session.gameActionRouter()->internalRouter();
    QObject::disconnect(own, &game::TInternalGameActionRouter::publishGameActionBetweenRouters, other,
                        &game::TInternalGameActionRouter::handleGameActionFromOtherRouter);
    QObject::disconnect(other, &game::TInternalGameActionRouter::publishGameActionBetweenRouters, own,
                        &game::TInternalGameActionRouter::handleGameActionFromOtherRouter);
}

/// The current session (null between sessions).
game::TGameSessionBase* liveSession()
{
    return game::currentGameSession();
}

/// The current session is in the game.
bool sessionOnline()
{
    const game::TGameSessionBase* session = liveSession();
    return session && session->isOnline();
}

/// Host: qtclient's engine (its data directories, the runtime game config and
/// the appearances) runs on the GUI thread; the client polls it each frame.
bool g_engineRunning = false;
volatile std::sig_atomic_t g_exitSignaled = 0;
std::function<void()> g_forceExitHandler;

void onExitSignal(int)
{
    g_exitSignaled = 1;
}

/// Runs on the GUI thread, after the event that noticed the signal has returned.
/// The handler shows Exit / Logout / Cancel. Quitting here would close the
/// window from the signal and skip that prompt. Doing this from afterAnimating
/// is inside QPlatformWindow::deliverUpdateRequest; closing the window there
/// frees it and the timer's hasPendingUpdateRequest then segfaults.
void finishExit()
{
    if (g_forceExitHandler)
        g_forceExitHandler();
    else
        QCoreApplication::quit();
}

/// Mounts `workDir` (monorepo: the directory holding data/assets/; package:
/// CipSoft's Resources next to the binary) and starts the engine.
bool startEngineHost(const QString& argv0, const QString& workDir)
{
    if (g_engineRunning)
        return true;
    const QByteArray argv0Bytes = argv0.toLocal8Bit();
    tibia::files::data().init(argv0Bytes.constData());
    // Work dir marker is CIP catalog-content.json. Prefer the monorepo extract
    // tree, then walk from the binary for CipSoft Resources (macOS .app cwd is often "/").
    const QString previousDir = QDir::currentPath();
    QDir::setCurrent(workDir);
    bool found = tibia::files::data().discoverWorkDir("data/assets/catalog-content.json");
    QDir::setCurrent(previousDir);
    if (!found) {
        static const char* const kCatalogRels[] = {
            "assets/catalog-content.json",
            "Resources/assets/catalog-content.json",
            "Contents/Resources/assets/catalog-content.json",
        };
        QDir directory(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 6 && !found; ++i) {
            for (const char* rel : kCatalogRels) {
                const QString catalog = directory.filePath(QString::fromUtf8(rel));
                if (!QFileInfo::exists(catalog))
                    continue;
                // Search root = parent of assets/ (Resources or package root).
                QDir resources(QFileInfo(catalog).absolutePath());
                resources.cdUp();
                found = tibia::files::data().addSearchPath(resources.absolutePath().toStdString(), true);
                break;
            }
            if (!directory.cdUp())
                break;
        }
    }
    if (!found)
        return false;
    // Virtual paths resolve under workDir. CIP assets use one Resources tree
    // (see TTibiaFileSystemHelper::assetsRoot). workDir "/" must become "/data",
    // not "//data" (wasm MEMFS will not open the latter).
    std::string dataRoot = tibia::files::data().getWorkDir();
    while (!dataRoot.empty() && (dataRoot.back() == '/' || dataRoot.back() == '\\'))
        dataRoot.pop_back();
    tibia::files::data().addSearchPath(dataRoot, true);
    tibia::files::data().addSearchPath(dataRoot + "/data", true);

    std::signal(SIGTERM, onExitSignal);
    std::signal(SIGINT, onExitSignal);
    tibia::appearances::TAppearancesManager::instance().init();

    tibia::files::data().setupUserWriteDir("tibia/");
    g_engineRunning = true;
    return true;
}

void stopEngineHost()
{
    if (!g_engineRunning)
        return;
    g_engineRunning = false;
    if (game::TGameSessionBase* session = liveSession(); session && session->isOnline())
        session->disconnectGame();
    tibia::appearances::TAppearancesManager::instance().terminate();
}

/// The engine's GUI thread work: a signalled exit.
void pollEngineHost()
{
    if (!g_engineRunning)
        return;
    if (g_exitSignaled) {
        g_exitSignaled = 0;
        // Queued: this poll often runs from afterAnimating, still inside the
        // window update timer (see finishExit). Cancel leaves the process up,
        // so a later signal can ask again.
        if (QCoreApplication* app = QCoreApplication::instance())
            QMetaObject::invokeMethod(app, finishExit, Qt::QueuedConnection);
        else
            finishExit();
    }
}

qint64 nowMicros()
{
    return shared::TClockService::process()->nowMicros();
}

/// The session setSessionInstances installed: from before its controllers
/// connect until it is released.
game::TGameSessionBase* g_sessionInstances = nullptr;

/// Host: the session the client's own lookups (activeSession) resolve to
/// (null: none).
void setSessionInstances(game::TGameSessionBase* session)
{
    g_sessionInstances = session;
}

/// Set while the client shuts down: a released session is deleted at once.
bool g_releaseGameSessionsNow = false;

/// Host: an object the client does not own, for a session that keeps it.
template<typename T>
std::shared_ptr<T> notOwned(T* object)
{
    return std::shared_ptr<T>(object, [](T*) {});
}

}

/// CIP: every session gets the client's file system helper, appearances,
/// instance factory, options, clipboard writer, URL tools, clock, item
/// prices, sound controller, keyboard shortcut handler and the spell,
/// passive ability and proficiency storages (the plain sessiondump also the
/// dialog stack). Released inside one of its own signals (it disconnected),
/// the session is deleted once control is back in the event loop.
template<typename Session, typename... Extra>
std::shared_ptr<Session> TGameClient::createGameSession(Extra&&... extra) const
{
    auto* session = new Session(tibiaFileSystemHelper(), appearancesManager(), appearanceInstanceFactory(),
                                sharedClientOptions(), m_pTibiaClipboardHelper, m_pTibiaUrlTools, m_pClockService,
                                m_pItemPriceStorage, soundController(), keyboardShortcutHandler(), spellInfoStorage(),
                                passiveAbilityInfoStorage(), proficiencyStorage(), std::forward<Extra>(extra)...);
    return std::shared_ptr<Session>(session, [](Session* gameSession) {
        if (g_releaseGameSessionsNow)
            delete gameSession;
        else
            gameSession->deleteLater();
    });
}

namespace {

/// The account's premium type (0 free, 1 premium, 2 free premium: premium
/// without an end date, the product's free-premium worlds) and days left.
std::pair<int, quint16> premiumTypeAndDays(const authentication::TPlaySessionData& session)
{
    const qint64 seconds = session.premiumUntil - QDateTime::currentSecsSinceEpoch();
    const int premiumType = !session.isPremium ? 0 : seconds <= 0 ? 2 : 1;
    const quint16 days = premiumType == 1 ? static_cast<quint16>((seconds + 86399) / 86400) : 0;
    return { premiumType, days };
}

/// Pack façade: composition under TGameClient (CIP base is already QObject).
class GameHostImpl final : public runtime::TGameHost
{
public:
    explicit GameHostImpl(TGameClient* client)
        : runtime::TGameHost(client)
        , m_client(client)
    {
    }

    qint64 averageFrameNs() const override
    {
        if (worldmap::TFrameDurationRing* ring = m_client->frameDurations())
            return ring->averageNs();
        return 0;
    }

    qint64 worldmapBuildNs() const override
    {
        return provider() ? provider()->buildNs() : 0;
    }
    qint64 worldmapTilesNs() const override
    {
        return provider() ? provider()->tilesNs() : 0;
    }
    qint64 worldmapLightNs() const override
    {
        return provider() ? provider()->lightNs() : 0;
    }
    qint64 worldmapRaycastNs() const override
    {
        return provider() ? provider()->raycastNs() : 0;
    }
    qint64 worldmapRaycastGatherNs() const override
    {
        return provider() ? provider()->raycastGatherNs() : 0;
    }
    qint64 worldmapRaycastRoofsNs() const override
    {
        return provider() ? provider()->raycastRoofsNs() : 0;
    }

    void toggleCharacterSelectionForSwitch() override
    {
        m_client->toggleCharacterSelectionForSwitch();
    }

    bool requestCharacterSwitch(const QString& characterName) override
    {
        return m_client->requestCharacterSwitch(characterName);
    }

    QStringList characterNames() const override
    {
        QStringList names;
        for (const authentication::TCharacter& c : m_client->account().characters) {
            if (!c.name.isEmpty())
                names.append(c.name);
        }
        return names;
    }

    void requestLogout() override
    {
        if (game::TGameserverGameSession* session = m_client->session())
            session->requestLogout();
        else if (game::TSessiondumpGameSession* dump = m_client->sessiondump())
            dump->requestLogout();
    }

    network::ExtendedOpcodeRouter* opcodeRouter() const override
    {
        return m_client->opcodes();
    }

    /// The current session's world map frame (packs read it each frame); null
    /// between sessions.
    qmlcomponents::TWorldMapRenderProvider* worldMapRenderProvider() const override
    {
        return m_client->worldMapRenderProvider();
    }

private:
    qmlcomponents::TWorldMapRenderProvider* provider() const
    {
        return m_client->worldMapRenderProvider();
    }

    TGameClient* m_client = nullptr;
};

}

TGameClient::TGameClient(QQmlEngine* Engine, std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper,
                         config::TClientOptions* ClientOptions, std::shared_ptr<config::TTibiaConfig> TibiaConfig,
                         std::shared_ptr<shared::TDownloadPool> DownloadPool, QObject* parent)
    : cip::tibia::client::TGameClient(parent)
    , ::tibia::client::TGameClientBase(Engine, std::move(TibiaFileSystemHelper), ClientOptions, std::move(TibiaConfig))
    , m_frameDurations(std::make_unique<worldmap::TFrameDurationRing>())
    , m_pSlidingFPSCounter(std::make_shared<systemsurvey::TSlidingFPSCounter>(1000, shared::TClockService::process()))
    , m_predictor(std::make_unique<worldmap::TNextFrameTimestampPredictor>(shared::TClockService::process()))
    , m_sessiondumpCatalogLoader(std::move(DownloadPool))
{
    m_pGameActionRouter = std::make_shared<game::TGameActionRouter>();
    // CIP: the client's own game actions go through the router.
    connect(this, &TGameClient::publishGameAction, m_pGameActionRouter->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    // The actions whose CIP handler classes are not ported yet.
    auto* unported = new input::TUnportedGameActionHandler(this);
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, unported,
            &input::TUnportedGameActionHandler::handleGameAction);
    connect(unported, &input::TUnportedGameActionHandler::publishGameAction, m_pGameActionRouter->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    // CIP: the Cyclopedia map's static data asks for the subarea masks once
    // the main window exists (they come back through the game action router).
    connect(this, &TGameClient::mainWindowInitialized, cyclopediaMapStaticDataStorage().get(),
            &cyclopedia::TCyclopediaMapStaticDataStorage::onGameClientMainWindowInitialized);
    // The sound helper the dialogs and the QML singletons play through.
    auto* soundHelper = new qmlcomponents::TSoundHelperService(this);
    qmlcomponents::TSoundHelperService::setInstance(soundHelper);
    connect(soundHelper, &qmlcomponents::TSoundHelperService::publishGameAction, m_pGameActionRouter->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    m_gameHost = new GameHostImpl(this);
    // Pack ABI: the packs' "main window initialized" is the world entry.
    connect(this, &TGameClient::worldEntered, m_gameHost, &runtime::TGameHost::mainWindowInitialized);
    connect(this, &TGameClient::worldLeft, m_gameHost, &runtime::TGameHost::worldLeft);
    connect(this, &TGameClient::characterSwitchRequested, m_gameHost, &runtime::TGameHost::characterSwitchRequested);
    connect(this, &TGameClient::characterSelectionClosed, m_gameHost, &runtime::TGameHost::characterSelectionClosed);
    connect(this, &TGameClient::characterSelectionRequested, m_gameHost, &runtime::TGameHost::characterSelectionRequested);

    // The process' storages and handlers above publish to the client's router.
    {
        game::TInternalGameActionRouter* router = m_pGameActionRouter->internalRouter();
        const auto publishToRouter = [router](auto* publisher) {
            if (publisher)
                connect(publisher, &std::remove_pointer_t<decltype(publisher)>::publishGameAction, router,
                        &game::TInternalGameActionRouter::handleGameActionInternal);
        };
        const auto publishChildrenToRouter = [this, &publishToRouter]<typename Publisher>(Publisher*) {
            for (Publisher* child : findChildren<Publisher*>(QString(), Qt::FindDirectChildrenOnly))
                publishToRouter(child);
        };
        publishChildrenToRouter(static_cast<game::TPlayerProtocolMessageHandler*>(nullptr));
        // Their game actions.
    }



    // CIP: the game window is built on the next event loop pass once the
    // timer starts. Host: the builder is qtclient's createGameWindow; a game
    // window that cannot be built ends the client.
    m_showGameWindowTimer.setInterval(0);
    m_showGameWindowTimer.setSingleShot(true);
    connect(&m_showGameWindowTimer, &QTimer::timeout, this, [this] {
        if (!createGameWindow(m_gameWindowSetup)) {
            qCritical().noquote() << "The game window could not be built.";
            QCoreApplication::exit(1);
        }
    });
}

void TGameClient::createAuthenticationProcessController(const QUrl& LoginUrl)
{
    // CIP: the character selection, the login request uploader, the email and
    // login confirmation code uploaders (each with its own sender) and the
    // Cloudflare challenge for the login service, handed to the controller.
    auto pCharacterSelection = std::make_shared<gamewindow::TCharacterSelectionController>();
    auto pLoginRequestUploader = std::make_shared<authentication::TLoginRequestUploader>(
        LoginUrl, std::make_shared<shared::TJsonDocumentHttpPostSender>(), clientOptions());
    pLoginRequestUploader->setClientRevision(m_options.env.revision);
    auto pEMailCodeRequestUploader = std::make_shared<authentication::TEmailCodeRequestUploader>(
        LoginUrl, std::make_shared<shared::TJsonDocumentHttpPostSender>());
    auto pLoginConfirmationCodeRequestUploader = std::make_shared<authentication::TLoginConfirmationCodeRequestUploader>(
        LoginUrl, std::make_shared<shared::TJsonDocumentHttpPostSender>());
    auto pCloudflareChallenge = std::make_shared<authentication::TCloudflareChallengeController>();
    pCloudflareChallenge->setTargetUrl(LoginUrl);
    m_authentication = std::make_shared<authentication::TAuthenticationProcessController>(
        pCharacterSelection, pLoginRequestUploader, pEMailCodeRequestUploader, pLoginConfirmationCodeRequestUploader,
        pCloudflareChallenge, assetManager());
    {
        // The play session data the login machine forgets lives in the client's
        // account; the clearance goes with the next login request.
        authentication::TAuthenticationProcessController::THost host;
        host.resetSessionData = [this] { m_account.session = {}; };
        m_authentication->setHost(std::move(host));
    }
    m_authentication->bindLoginUploader(pLoginRequestUploader.get());
    connect(pCloudflareChallenge.get(), &authentication::TCloudflareChallengeController::challengeSolveSuccess,
            m_authentication.get(), &authentication::TAuthenticationProcessController::onCloudflareSolverSuccess);
    connect(pCloudflareChallenge.get(), &authentication::TCloudflareChallengeController::challengeSolveFailed,
            m_authentication.get(), [controller = m_authentication.get()] {
                controller->trigger(authentication::TAuthenticationProcessController::ETrigger::CloudflareSolveFailed);
            });

    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginSuccessful, this, &TGameClient::onLoginSuccessful);
    // CIP: the login machine shows the login error (loginerror_caption); its
    // abort (OK) returns to the login mask. CLI runs end on a failed login.
    const auto exitCliRunOnLoginFailure = [this] {
        if (!m_options.autoCharacter.isEmpty() || !m_options.replay.isEmpty() || m_options.playForSeconds > 0)
            QCoreApplication::exit(2);
    };
    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginFailed, this,
            [this, exitCliRunOnLoginFailure](const QString& message) {
                qCritical().noquote() << message;
                exitCliRunOnLoginFailure();
            });
    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginFailedDueToInvalidServerResponse, this,
            exitCliRunOnLoginFailure);
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::abort, this, [this] {
        if (!sessionOnline())
            ensureLoginSurface();
    });

    connect(pCharacterSelection.get(), &gamewindow::TCharacterSelectionController::publishGameAction,
            m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::publishGameAction,
            m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(pCharacterSelection.get(), &gamewindow::TCharacterSelectionController::requestCharacterLogin, this,
            [this](const gamewindow::TCharacterLoginData& data) { onCharacterChosen(data.characterName); });
    connect(pCharacterSelection.get(), &gamewindow::TCharacterSelectionController::abort, this, [this] {
        m_authentication->trigger(authentication::TAuthenticationProcessController::ETrigger::DialogAborted);
        Q_EMIT characterSelectionClosed();
        // CIP: Cancel while online (alt switch) only closes the dialog. From the
        // login / post-logout character list, return to the login mask — never quit.
        if (sessionOnline())
            return;
        Q_EMIT returnToLoginRequested();
    });
    connect(this, &TGameClient::characterSelectionRequested, this, [this] {
        m_characterSelectionOpen = true;
        if (gamewindow::TCharacterSelectionController* selection = characterSelection())
            selection->open();
    });
    connect(this, &TGameClient::characterSelectionClosed, this, [this] {
        if (m_characterSelectionOpen) {
            if (gamewindow::TCharacterSelectionController* selection = characterSelection())
                selection->close();
        }
        m_characterSelectionOpen = false;
    });
    // CIP: the controller asks the client to connect the chosen character,
    // and its FPS tracking switch follows the login.
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::requestCharacterGameserverLogin, this,
            &TGameClient::connectClientToGameserverWithExistingCredentials);
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::fpsTrackingActiveChanged, this,
            &TGameClient::onFpsTrackingActiveChanged);
    connect(m_authentication.get(), &authentication::TAuthenticationProcessController::optionsTrackingActiveChanged, this,
            &TGameClient::onOptionsTrackingActiveChanged);
    // CIP CHARACER_SELECTION_AUTO_CLOSE: hide the list without treating it as Cancel.
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, this,
            [this](const input::IGameActionPtr& action) {
                if (action && action->gameActionType() == input::EGameAction::CHARACER_SELECTION_AUTO_CLOSE)
                    Q_EMIT characterSelectionClosed();
            });
}

authentication::TLoginRequestUploader* TGameClient::loginRequestUploader() const
{
    return m_authentication ? m_authentication->loginRequestUploader() : nullptr;
}

TGameClient::~TGameClient()
{
    // CIP: first the process-wide context menu goes.
    gamewindow::TContextMenuController::resetInstance();
    // The session's manager saves the character's configuration through
    // storages the game window owns: save before the game window goes.
    if (config::TCharacterConfigurationManager* manager = characterConfigurationManager())
        manager->saveConfiguration();
    // CIP: the dialogs made after the game window controller go before it
    // (the client's members in reverse order, then the game window's).
    m_pLoginAdviceDialogController.reset();
    m_pExitGameDialogController.reset();
    m_pConnectingDialogController.reset();
    m_pHighscoresDialogController.reset();
    m_pPreyDialogController.reset();
    m_pConfigureCreaturePodiumDialogController.reset();
    m_pSelectOutfitDialogController.reset();
    m_pCyclopediaDialogController.reset();
    m_pLinkCopyWarningDialogController.reset();
    m_pInfoBannerController.reset();
    m_pEditTextController.reset();
    m_pServerModalDialogController.reset();
    m_pQuestLogController.reset();
    m_pCreateCharacterDialogController.reset();
    m_pCreateAccountDialogController.reset();
    m_pRewardWallDialogController.reset();
    m_pOutfitMemorialDialogController.reset();
    m_pLenshelpManager.reset();
    m_pConfirmMoveObjectToStashDialogController.reset();
    m_pStashDialogController.reset();
    m_pBossDifficultySelectionDialogController.reset();
    m_pTaskboardDialogController.reset();
    m_pSoulSealsDialogController.reset();
    m_pWeaponProficiencyShapeOptionsDialogController.reset();
    m_pWeaponProficiencyShapeDialogController.reset();
    m_pWeaponProficiencyReshapeDialogController.reset();
    m_pWeaponProficiencyDialogController.reset();
    m_pNaggingMessageDialogController.reset();
    m_pShowSpellBookDialogController.reset();
    m_pShowImageDialogController.reset();
    m_pExaltationForgeDialogController.reset();
    m_pSelectObjectAmountDialogController.reset();
    m_pBlessingsDialogController.reset();
    m_pMultiOfflineTrainingDialogController.reset();
    m_pStoreDialogController.reset();
    m_pDeathDialogController.reset();
    m_pGameSessionDisconnectReactionController.reset();
    m_pBugReportDialogController.reset();
    m_pReportViolationController.reset();
    m_pMarketController.reset();
    m_gameWindow.reset();
    // Host: the static dialog stack (and the event filters it holds) goes with
    // the client instead of at static destruction, after the application.
    gamewindow::TDialogStackController::setInstance(nullptr);
    // The store balance helper follows the session and the engine's hooks.
    m_pStoreAndResourceBalanceHelperController.reset();
    shutdown();
    qmlcomponents::TSoundHelperService::setInstance(nullptr);
}

void TGameClient::setForceExitHandler(std::function<void()> handler)
{
    g_forceExitHandler = std::move(handler);
}

bool TGameClient::startEngine(const QString& argv0, const QString& workDir)
{
    if (!startEngineHost(argv0, workDir)) {
        fail(tr("Could not start the game engine (no assets/catalog-content.json under %1).").arg(workDir));
        return false;
    }
    m_started = true;

    // Engine objects exist only after init.
    m_opcodes = std::make_unique<network::ExtendedOpcodeRouter>();
    const QJsonObject clientOptions = app::ClientOptions::load().options();
    m_frameRateLimitEnabled = clientOptions.value(QLatin1String("frameRateLimitEnabled")).toBool(false);
    m_frameRateLimit = std::clamp(clientOptions.value(QLatin1String("frameRateLimit")).toInt(60), 1, 255);
    const auto applyEffectOpacities = [](const QJsonObject& options) {
        const auto effectOpacity = [&](const char* key) {
            return static_cast<float>(std::clamp(options.value(QLatin1String(key)).toDouble(1.0), 0.0, 1.0));
        };
        setMagicEffectOpacities(effectOpacity("ownEffectsOpacity"),
                                effectOpacity("otherPlayersEffectsOpacity"),
                                effectOpacity("monsterEffectsOpacity"),
                                effectOpacity("monsterBossAreaEffectsOpacity"));
    };
    applyEffectOpacities(clientOptions);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this, applyEffectOpacities](const QString& name) {
        const QJsonObject options = app::ClientOptions::load().options();
        if (name == QLatin1String("frameRateLimitEnabled") || name == QLatin1String("frameRateLimit")) {
            m_frameRateLimitEnabled = options.value(QLatin1String("frameRateLimitEnabled")).toBool(false);
            m_frameRateLimit = std::clamp(options.value(QLatin1String("frameRateLimit")).toInt(60), 1, 255);
        }
        if (name == QLatin1String("ownEffectsOpacity") || name == QLatin1String("otherPlayersEffectsOpacity")
            || name == QLatin1String("monsterEffectsOpacity") || name == QLatin1String("monsterBossAreaEffectsOpacity"))
            applyEffectOpacities(options);
    });
    return true;
}

void TGameClient::emitInitialized()
{
    Q_EMIT initialized();
}

void TGameClient::emitMainWindowInitialized()
{
    Q_EMIT mainWindowInitialized();
}

void TGameClient::start()
{
    // Custom effects / xBRZ register via product modpack (mods::bootstrapAll).
    if (!m_options.replay.isEmpty()) {
        const QFileInfo file(m_options.replay);
        if (!file.isFile()) {
            fail(tr("No recorded session at %1.").arg(m_options.replay));
            return;
        }
        // Prefer canonical path (follows symlinks). Basename-only is kept for
        // files that live under cwd `records/` (where --record writes).
        const QFileInfo resolved(file.canonicalFilePath().isEmpty() ? file.absoluteFilePath()
                                                                    : file.canonicalFilePath());
        QString replayArg = resolved.absoluteFilePath();
        if (resolved.absolutePath().endsWith(QLatin1String("/records")))
            replayArg = resolved.fileName();
        // Host: a dump with a .hints file next to it is a tutorial dump; CipSoft
        // has hints only in the tutorial session, so it replays as the tutorial.
        std::shared_ptr<game::TSessiondumpGameSessionMetaInfo> metaInfo;
        if (const QString hints = tutorial::hintsPathForDump(resolved.absoluteFilePath()); !hints.isEmpty()) {
            auto tutorial = std::make_shared<game::TTutorialSessiondumpGameSessionMetaInfo>();
            tutorial->sessiondump.hintsUrl = QUrl::fromLocalFile(hints);
            tutorial->sessiondump.hasHints = true;
            metaInfo = std::move(tutorial);
        } else {
            metaInfo = std::make_shared<game::TSessiondumpGameSessionMetaInfo>();
        }
        setGameSessionSettings(*metaInfo);
        metaInfo->sessiondump.url = QUrl::fromLocalFile(replayArg);
        metaInfo->startedByGameAction = true;
        enqueueGameSessions({ metaInfo });
        return;
    }

    login();
}

namespace {

game::TGameSessionBase* activeSession()
{
    return g_sessionInstances;
}

}

qmlcomponents::TWorldMapRenderProvider* TGameClient::worldMapRenderProvider() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_currentGameSession.get());
    return session ? session->worldMapRenderProvider().get() : nullptr;
}

worldmap::TWorldMapViewport* TGameClient::worldMapViewport() const
{
    return activeSession() ? activeSession()->worldMapViewport().get() : nullptr;
}

worldmap::TAmbientLightStorage* TGameClient::ambientLight() const
{
    return activeSession() ? activeSession()->ambientLightStorage().get() : nullptr;
}

worldmap::TWorldMapStorage* TGameClient::worldMapStorage() const
{
    return activeSession() ? activeSession()->worldMapStorage().get() : nullptr;
}

creatures::TCreatureStorage* TGameClient::creatureStorage() const
{
    return activeSession() ? activeSession()->creatureStorage().get() : nullptr;
}

container::TContainerStorage* TGameClient::containerStorage() const
{
    return activeSession() ? activeSession()->containerStorage().get() : nullptr;
}

container::TInventoryContainer* TGameClient::inventoryContainer() const
{
    return activeSession() ? activeSession()->inventoryContainer().get() : nullptr;
}

worldmap::TWorldmapProtocolMessageHandler* TGameClient::worldmapProtocol() const
{
    return activeSession() ? activeSession()->worldmapProtocolMessageHandler().get() : nullptr;
}

chat::TTextStorage* TGameClient::textStorage() const
{
    return activeSession() ? activeSession()->textStorage().get() : nullptr;
}

effects::TEffectStorage* TGameClient::effectStorage() const
{
    return activeSession() ? activeSession()->effectStorage().get() : nullptr;
}

void TGameClient::shutdown()
{
    if (!m_started)
        return;
    m_started = false;
    m_idleTick.stop();
    m_frameLimitTimer.stop();
    onWorldLeft();
    // No session follows: the logout below must not connect the next one.
    m_gameSessionMetaInfoQueue.clear();
    if (m_currentGameSession)
        disconnect(m_currentGameSession.get(), nullptr, this, nullptr);
    // Force logout / Map::clean while TWorldMapStorage still owns the field
    // QObjects. Map tileBlocks hold raw pointers into that tree; destroying
    // storage first leaves terminate → removeUnawareThings dangling (SIGSEGV
    // on quit Exit).
    stopEngineHost();
    m_opcodes.reset();
    // The session goes now, not on the next event loop pass: the engine is down.
    g_releaseGameSessionsNow = true;
    setCurrentGameSession(nullptr);
    g_releaseGameSessionsNow = false;
}

void TGameClient::login()
{
    if (m_options.account.isEmpty()) {
        // CIP start-screen login mask (TGameWindowController::loginPressed) supplies credentials.
        return;
    }
    Q_EMIT loginStarted();
    onRequestLoginWithCredentials(m_options.account, m_options.password);
}

void TGameClient::onRequestLoginWithCredentials(const QString& EMailAddress, const QString& Password)
{
    // The credentials go to the login machine; entering its login waiting
    // state sends the request.
    m_authentication->loginWithCredentials(EMailAddress, Password);
}

void TGameClient::abortPendingEnter()
{
    if (!sessionOnline()) {
        if (game::TGameserverGameSession* live = session())
            live->disconnectNow();
        else if (game::TSessiondumpGameSession* dump = sessiondump())
            dump->disconnectNow();
    }
}

void TGameClient::submitCredentials(const QString& account, const QString& password)
{
    m_options.account = account.trimmed();
    m_options.password = password;
    if (m_options.account.isEmpty()) {
        Q_EMIT loginCredentialsNeeded();
        return;
    }
    login();
}

void TGameClient::refreshAccount(const QJsonObject& reply)
{
    // Host: the client's account model (the world ports and PvP types the
    // session meta info reads, the characters a character switch shows) takes
    // the reply's play data; the authentication shows it in the selection.
    authentication::LoginResult result = authentication::TLoginRequestUploader::parse(reply);
    if (result.characters.empty())
        return;
    if (result.session.sessionKey.isEmpty())
        result.session = m_account.session;
    m_account = std::move(result);
}

void TGameClient::onLoginSuccessful(const authentication::LoginResult& result)
{
    m_account = result;
    // CipSoft login has no session.email; remember-email is fed from the typed mask / CLI.
    if (!m_options.account.isEmpty())
        Q_EMIT sessionEmailAvailable(m_options.account);
    showAccount(result.characters);
    if (!m_options.autoCharacter.isEmpty()) {
        for (const auto& c : result.characters) {
            if (c.name.compare(m_options.autoCharacter, Qt::CaseInsensitive) == 0) {
                // The selection logs the character in as if it was confirmed.
                characterSelection()->selectCharacter(c.name);
                return;
            }
        }
        fail(tr("No character named %1 on this account.").arg(m_options.autoCharacter));
        return;
    }
    Q_EMIT characterSelectionRequested();
}

void TGameClient::openCharacterSelectionForSwitch()
{
    if (m_account.characters.empty())
        return;
    showAccount(m_account.characters);
    Q_EMIT characterSelectionRequested();
}

void TGameClient::closeCharacterSelection()
{
    Q_EMIT characterSelectionClosed();
}

void TGameClient::toggleCharacterSelectionForSwitch()
{
    if (!sessionOnline())
        return;
    if (m_characterSelectionOpen) {
        closeCharacterSelection();
        return;
    }
    openCharacterSelectionForSwitch();
}

void TGameClient::ensureLoginSurface()
{
    if (sessionOnline() || (liveSession() && liveSession()->isSwitchingCharacter()) || !m_options.autoCharacter.isEmpty())
        return;
    // A character change (or a login again) connects the next session itself.
    if (const gamewindow::TGameSessionDisconnectReactionController* reaction = gameSessionDisconnectReactionController();
        reaction && reaction->reconnectRequested())
        return;
    // A live account session reopens the character list (including an empty
    // account, which the bootstrap turns into Create Character). Otherwise the
    // login mask is the only way back in — leaving both hidden is a dead screen.
    if (!m_account.session.sessionKey.isEmpty()) {
        Q_EMIT characterSelectionRequested();
        return;
    }
    Q_EMIT returnToLoginRequested();
}

bool TGameClient::requestCharacterSwitch(const QString& characterName)
{
    if (characterName.isEmpty() || !sessionOnline())
        return false;
    const auto known = std::find_if(m_account.characters.begin(), m_account.characters.end(),
                                    [&](const authentication::TCharacter& c) { return c.name == characterName; });
    if (known == m_account.characters.end())
        return false;
    // The selection logs the character in as if it was confirmed; the client
    // then quits this one and connects the chosen one once it disconnected.
    showAccount(m_account.characters);
    Q_EMIT characterSwitchRequested(characterName);
    characterSelection()->selectCharacter(characterName);
    return true;
}

void TGameClient::showAccount(const authentication::TCharacterList& characters)
{
    const auto [premiumType, premiumDays] = premiumTypeAndDays(m_account.session);
    gamewindow::TCharacterSelectionController* selection = characterSelection();
    selection->setAccountState(static_cast<int>(m_account.session.status));
    selection->setPremium(premiumType, premiumDays);
    selection->setRecoverySetupComplete(m_account.session.recoverySetupComplete);
    selection->setCharacters(characters, m_account.worlds);
    // Until a character logged in, the table preselects the main character.
    if (selection->lastSelectedCharacterName().isEmpty()) {
        for (const authentication::TCharacter& c : characters) {
            if (c.isMainCharacter)
                selection->setLastSelectedCharacterName(c.name);
        }
    }
}

void TGameClient::onCharacterChosen(const QString& characterName)
{
    characterSelection()->setLastSelectedCharacterName(characterName);
    Q_EMIT characterSelectionClosed();
    // The character login is the machine's ShowCharacterSelection step. A
    // stopped machine (a character change from a running session) or one
    // still in its post-login dialogs starts again there.
    using EState = authentication::TAuthenticationProcessController::EState;
    if (!m_authentication->isRunning() || m_authentication->state() != EState::ShowCharacterSelection)
        m_authentication->start(true);
    m_authentication->trigger(authentication::TAuthenticationProcessController::ETrigger::CharacterLoginRequested);
}

void TGameClient::connectClientToGameserverWithExistingCredentials()
{
    if (!m_currentGameSession) {
        m_pConnectingDialogController->setButtons(gamewindow::cancelDialogButtons());
        m_pConnectingDialogController->showMessage(qtTrId("connecting_caption"), qtTrId("connecting_description"));
        QTimer::singleShot(0, this, &TGameClient::onConnectClientToGameserver);
        return;
    }
    // A character change: the current one quits without leaving the game, so
    // the client logs in again once it disconnected.
    if (m_pGameSessionDisconnectReactionController && !m_pGameSessionDisconnectReactionController->reconnectRequested())
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionQuitGame>(false));
}

void TGameClient::onConnectClientToGameserver()
{
    m_gameSessionMetaInfoQueue.clear();
    const authentication::TLoginRequestData loginData = m_authentication->loginData();
    const std::vector<gamewindow::TCharacterLoginData> characterLoginData = m_authentication->characterLoginData();
    if (characterLoginData.size() == 1) {
        for (std::shared_ptr<game::TGameSessionMetaInfo>& metaInfo : createGameSessionMetaInfos(loginData, characterLoginData.front()))
            m_gameSessionMetaInfoQueue.push_back(std::move(metaInfo));
    }
    autoconnectNextGameSessionInQueue();
}

void TGameClient::onAbortConnectClientToGameserver()
{
    if (m_pGameSessionDisconnectReactionController && m_pGameSessionDisconnectReactionController->reconnectRequested())
        m_pGameSessionDisconnectReactionController->resetReconnectRequested();
    onGameSessionDisconnected();
    onShowCharacterSelectionDialogRequested();
}

void TGameClient::onShowCharacterSelectionDialogRequested()
{
    // CIP: the authentication process controller's login uploader (when it
    // has one) first takes the characters' looks from the game session.
    if (authentication::TLoginRequestUploader* uploader = m_authentication->loginRequestUploader())
        uploader->updateCharactersFromGameSession();
    m_authentication->showCharacterSelection();
}

void TGameClient::onGameSessionConnected()
{
    m_authentication->stopProcess();
    if (m_pFPSDropsLogger) {
        const gamewindow::TCharacterLoginData characterLoginData = m_authentication->firstCharacterLoginData();
        m_pFPSDropsLogger->setCharacterName(characterLoginData.characterName);
        m_pFPSDropsLogger->setWorldName(characterLoginData.worldName);
    }
    // CIP: with options tracking on, the account's client options go up once.
    if (m_pClientOptionsUploader)
        m_pClientOptionsUploader->upload(m_authentication->loginData().email, clientOptions()->exportToBase64String());
}

void TGameClient::onGameSessionLoginSuccessful()
{
    updateWindowTitleExtension();
    updateCrashReportAnnotations();
}

void TGameClient::onGameSessionLoginAdvice(const QString& LoginAdvice)
{
    m_pLoginAdviceDialogController->setButtons(gamewindow::okDialogButtons());
    m_pLoginAdviceDialogController->showMessage(qtTrId("login_advice_dialog_caption"), LoginAdvice);
}

void TGameClient::onGameSessionLoginError(const QString& LoginError, const int Action)
{
    Q_UNUSED(LoginError);
    Q_UNUSED(Action);
    m_pConnectingDialogController->dialog()->close();
}

void TGameClient::updateWindowTitleExtension()
{
    QQuickItem* const placeholder = m_gameWindowPlaceholder;
    if (!placeholder)
        return;
    if (m_currentGameSession) {
        const std::vector<gamewindow::TCharacterLoginData> characterLoginData = m_authentication->characterLoginData();
        if (!characterLoginData.empty()) {
            placeholder->setProperty("windowTitleExtension", characterLoginData.front().characterName);
            return;
        }
    }
    placeholder->setProperty("windowTitleExtension", QString());
}

void TGameClient::updateCrashReportAnnotations()
{
    if (m_currentGameSession) {
        const std::vector<gamewindow::TCharacterLoginData> characterLoginData = m_authentication->characterLoginData();
        if (!characterLoginData.empty()) {
            setCharacterCrashReportAnnotations(characterLoginData.front().characterName, characterLoginData.front().worldName);
            return;
        }
    }
    removeCharacterCrashReportAnnotations();
    removeWorldAddressCrashReportAnnotations();
}

void TGameClient::setCharacterCrashReportAnnotations(const QString& CharacterName, const QString& WorldName)
{
    shared::TCrashHandler::instance().setAnnotation(QStringLiteral("Character"), CharacterName);
    shared::TCrashHandler::instance().setAnnotation(QStringLiteral("Server"), WorldName);
}

void TGameClient::removeCharacterCrashReportAnnotations()
{
    shared::TCrashHandler::instance().removeAnnotation(QStringLiteral("Character"));
    shared::TCrashHandler::instance().removeAnnotation(QStringLiteral("Server"));
    shared::TCrashHandler::instance().removeAnnotation(QStringLiteral("Position"));
}

void TGameClient::removeWorldAddressCrashReportAnnotations()
{
    shared::TCrashHandler::instance().removeAnnotation(QStringLiteral("UnprotectedWorldAddress"));
    shared::TCrashHandler::instance().removeAnnotation(QStringLiteral("ProtectedWorldAddress"));
}

void TGameClient::setGameSessionSettings(game::TGameSessionMetaInfo& metaInfo) const
{
    const QString debug = QStringLiteral("DEBUG");
    auto queueConfiguration = std::make_shared<protocol::TProtocolMessageQueueConfiguration>();
    queueConfiguration->gameclientMessageDelay =
        static_cast<int>(config::clientSetting(debug, QStringLiteral("simulatedGameclientLag"), 0));
    queueConfiguration->gameserverMessageDelay =
        static_cast<int>(config::clientSetting(debug, QStringLiteral("simulatedGameserverLag"), 0));
    metaInfo.protocolMessageQueueConfiguration = std::move(queueConfiguration);
    const QString graphics = QStringLiteral("GRAPHICS");
    metaInfo.maximumNumberOfEffects =
        static_cast<quint32>(config::clientSettingAtMost(graphics, QStringLiteral("maximumNumberOfEffects"), 400, 800));
    metaInfo.millisecondsForAmbientLightChange =
        config::clientSetting(graphics, QStringLiteral("millisecondsForAmbientLightChange"), 10200);
    metaInfo.maxChatBacklogLength =
        static_cast<quint32>(config::clientSettingAtLeast(QStringLiteral("CHAT"), QStringLiteral("maxChatBacklogLength"), 1000, 5));
}

std::vector<std::shared_ptr<game::TGameSessionMetaInfo>> TGameClient::createGameSessionMetaInfos(
    const authentication::TLoginRequestData& LoginData, const gamewindow::TCharacterLoginData& CharacterLoginData)
{
    std::vector<std::shared_ptr<game::TGameSessionMetaInfo>> metaInfos;
    auto gameserver = std::make_shared<game::TGameserverGameSessionMetaInfo>();
    setGameSessionSettings(*gameserver);
    {
        const QString movement = QStringLiteral("MOVEMENT");
        gameserver->playerMovementIntentHandlerConfiguration->minPercentageOfCurrentMovementBeforeSendNext =
            config::clientSettingReal(movement, QStringLiteral("minPercentageOfCurrentMovementBeforeSendNext"), 1.0);
        gameserver->playerMovementIntentHandlerConfiguration->maxMillisecondsToFinishCurrentMovementBeforeSendNext =
            config::clientSetting(movement, QStringLiteral("maxMillisecondsToFinishCurrentMovementBeforeSendNext"), 0);
    }
    // CIP: a new login does not open the character selection on its own disconnect.
    if (m_pGameSessionDisconnectReactionController)
        m_pGameSessionDisconnectReactionController->resetShowCharacterSelectionOnDisconnect();
    // CIP: the session gets the account's login data and the character's.
    // Host: the world's game ports (the WebSocket ports in the browser), its
    // PvP type and the session key (when the login request has none) are the
    // login service's answer.
    gameserver->loginRequestData = LoginData;
    gameserver->characterLoginData = CharacterLoginData;
    authentication::TLoginRequestData& loginRequestData = gameserver->loginRequestData;
    gamewindow::TCharacterLoginData& characterLoginData = gameserver->characterLoginData;
    if (loginRequestData.sessionKey.isEmpty())
        loginRequestData.sessionKey = m_account.session.sessionKey;
    m_worldType = -1;
    for (const auto& [worldId, world] : m_account.worlds) {
        if (world.name == characterLoginData.worldName) {
            characterLoginData.worldPortProtected = world.port();
            characterLoginData.worldPortUnprotected = world.portUnprotected();
            m_worldType = world.pvpType;
            break;
        }
    }
    if (!m_options.recordTo.isEmpty())
        gameserver->recordTo = QFileInfo(m_options.recordTo).absoluteFilePath();
    // CIP: a tutorial character first watches the tutorial dump. Host: the
    // dump is qtclient's local tutorial file (CipSoft's comes from its assets).
    if (CharacterLoginData.tutorial) {
        const QString tutorialSessiondump = game::resolveTutorialSessiondumpPath();
        if (!tutorialSessiondump.isEmpty()) {
            auto tutorial = std::make_shared<game::TTutorialSessiondumpGameSessionMetaInfo>();
            tutorial->sessiondump.url = QUrl::fromLocalFile(tutorialSessiondump);
            // Host: qtclient's tutorial hints are the .hints file next to the dump.
            if (const QString hints = tutorial::hintsPathForDump(tutorialSessiondump); !hints.isEmpty()) {
                tutorial->sessiondump.hintsUrl = QUrl::fromLocalFile(hints);
                tutorial->sessiondump.hasHints = true;
            }
            setGameSessionSettings(*tutorial);
            tutorial->characterName = characterLoginData.characterName;
            tutorial->isMale = characterLoginData.isMale;
            // CIP: [URLS] tutorialProgressWebService. Host: the launcher
            // environment's receiver, as for the other client services.
            tutorial->tutorialProgressWebService = app::LauncherEnv::load(QDir::currentPath()).tutorialProgressWebServiceUrl();
            tutorial->clientSessionId = loginRequestData.sessionKey;
            metaInfos.push_back(std::move(tutorial));
        }
    }
    metaInfos.push_back(std::move(gameserver));
    return metaInfos;
}

void TGameClient::enqueueGameSessions(std::vector<std::shared_ptr<game::TGameSessionMetaInfo>> metaInfos)
{
    for (std::shared_ptr<game::TGameSessionMetaInfo>& metaInfo : metaInfos)
        m_gameSessionMetaInfoQueue.push_back(std::move(metaInfo));
    autoconnectNextGameSessionInQueue();
}

void TGameClient::autoconnectNextGameSessionInQueue()
{
    if (m_gameSessionMetaInfoQueue.empty())
        return;
    const std::shared_ptr<const game::TGameSessionMetaInfo> metaInfo = m_gameSessionMetaInfoQueue.front();
    m_gameSessionMetaInfoQueue.pop_front();
    using EType = game::TGameSessionMetaInfo::EType;
    const EType type = metaInfo->getType();
    if (type == EType::Sessiondump || type == EType::TutorialSessiondump) {
        // CIP: a replay or the tutorial never reconnects.
        if (gamewindow::TGameSessionDisconnectReactionController* reaction = gameSessionDisconnectReactionController())
            reaction->disableReconnectionAttempts();
        // CIP: the tutorial's dump runs in its own session class; a plain dump
        // also gets the client's dialog stack.
        std::shared_ptr<game::TSessiondumpGameSession> dump;
        if (type == EType::TutorialSessiondump)
            dump = createGameSession<game::TTutorialSessiondumpGameSession>();
        else
            dump = createGameSession<game::TSessiondumpGameSession>(m_pDialogStackController);
        dump->initialize(metaInfo);
        setCurrentGameSession(dump);
        dump->startSession();
        return;
    }
    if (type != EType::Gameserver)
        return;
    setCurrentGameSession(nullptr);
    // CIP: a crash report from now on names the character and its world, and
    // the item prices are the world's.
    if (const auto* gameserver = dynamic_cast<const game::TGameserverGameSessionMetaInfo*>(metaInfo.get())) {
        setCharacterCrashReportAnnotations(gameserver->characterLoginData.characterName, gameserver->characterLoginData.worldName);
        m_pItemPriceStorage->setWorldName(gameserver->characterLoginData.worldName);
    }
    const auto live = createGameSession<game::TGameserverGameSession>();
    live->initialize(metaInfo);
    setCurrentGameSession(live);
    // CIP: the session's target selection, character selection, login advice
    // and login error requests, and the walk keys during the session.
    connect(live.get(), &game::IGameSession::startTargetSelection, this, &TGameClient::onStartTargetSelection);
    connect(live.get(), &game::IGameSession::showCharacterSelectionDialog, this,
            &TGameClient::onShowCharacterSelectionDialogRequested);
    connect(live.get(), &game::IGameSession::gameLoginAdviceMessage, this, &TGameClient::onGameSessionLoginAdvice);
    connect(live.get(), &game::IGameSession::gameLoginErrorMessage, this, &TGameClient::onGameSessionLoginError);
    connect(m_pKeyboardStatusStorage.get(), &input::TKeyboardStatusStorage::changed, this,
            &TGameClient::onKeyboardStatusStorageChanged);
    live->connectToGameserver();
    live->startSession();
}

void TGameClient::connectToCurrentGameSession()
{
    game::IGameSession* current = m_currentGameSession.get();
    connect(current, &game::IGameSession::worldEntered, this, &TGameClient::onWorldEntered);
    // Command line runs (smoke tests, replays) end with an error when the session fails.
    if (!m_options.autoCharacter.isEmpty() || !m_options.replay.isEmpty() || m_options.playForSeconds > 0) {
        connect(current, &game::IGameSession::connectionError, this, [] { QCoreApplication::exit(2); });
        connect(current, &game::IGameSession::gameLoginErrorMessage, this, [] { QCoreApplication::exit(2); });
    }
}

void TGameClient::setCurrentGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_currentGameSession) {
        auto* previous = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get());
        if (GameSession && previous)
            unlinkGameActionRouters(*m_pGameActionRouter, *previous);
        disconnect(m_currentGameSession.get(), nullptr, this, nullptr);
        m_gameSessionDisconnected.disconnectAll();
        m_gameSessionLoginSuccessful.disconnectAll();
        m_gameSessionConnected.disconnectAll();
        // CIP: the previous session ends now and lets go of the client's
        // minimap storages and white/blacklist.
        m_currentGameSession->disconnectNow();
        if (previous) {
            previous->setMinimapStorages(nullptr, nullptr, nullptr);
            previous->setWhiteBlacklist(nullptr);
            previous->setPremiumTimeStorage(nullptr);
        }
        // A retired session no longer follows the game's notifications.
        disconnect(game::TGameNotify::instance(), nullptr, m_currentGameSession.get(), nullptr);
        // The session's minimap feed.
        setMinimapWorldmapProtocolHandler(nullptr);
        // Host: the client's lookups stop resolving to this session.
        setSessionInstances(nullptr);
        // Pack-facing accessors stop resolving to the retired session.
        game::setCurrentGameSession(nullptr);
        m_currentGameSession.reset();
    }
    // Host: code no session is handed to (game::current*) resolves to the new
    // session before its controllers connect.
    if (auto* next = dynamic_cast<game::TGameSessionBase*>(GameSession.get())) {
        setSessionInstances(next);
        game::setCurrentGameSession(GameSession);
        next->initializeWorldMapSessionState();
    }
    connectGameWindowControllersToGameSession(GameSession);
    m_currentGameSession = GameSession;
    // Host: code no session is handed to (pack ABI peels, packs) resolves it here.
    game::setCurrentGameSession(m_currentGameSession);
    if (m_currentGameSession) {
        // CIP: the routers link, the session gets the client's
        // white/blacklist and minimap storages, the minimap reads its world
        // map, and the client follows its player, connection, game login and
        // disconnect.
        if (auto* current = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get())) {
            linkGameActionRouters(*m_pGameActionRouter, *current);
            current->setWhiteBlacklist(m_pWhiteBlacklist.get());
            current->setPremiumTimeStorage(m_pPremiumTimeStorage);
            current->setMinimapStorages(minimapTileStorage(), minimapMarkerStorage(), cyclopediaMapStaticDataStorage());
        }
        setMinimapWorldmapProtocolHandler(worldmapProtocol());
        connectToCurrentGameSession();
        connect(m_currentGameSession.get(), &game::IGameSession::playerCreatureAddedToCreatureStorage, this,
                &TGameClient::onPlayerCreatureAddedToCreatureStorage);
        m_gameSessionConnected.connectTo(m_currentGameSession.get(), &game::IGameSession::connected);
        m_gameSessionLoginSuccessful.connectTo(m_currentGameSession.get(), &game::IGameSession::gameLoginSuccessful);
        m_gameSessionDisconnected.connectTo(m_currentGameSession.get(), &game::IGameSession::disconnected);
        m_gameSessionDisconnected.connectTo(m_currentGameSession.get(), &game::IGameSession::connectionError);
        m_gameSessionLoginSuccessful.resetEmitCount();
        m_gameSessionConnected.resetEmitCount();
        m_gameSessionDisconnected.resetEmitCount();
    }
    // CIP: the proficiency storage starts over with every session.
    proficiencyStorage()->clear();
}

game::TGameserverGameSession* TGameClient::session() const
{
    return dynamic_cast<game::TGameserverGameSession*>(m_currentGameSession.get());
}

game::TSessiondumpGameSession* TGameClient::sessiondump() const
{
    return dynamic_cast<game::TSessiondumpGameSession*>(m_currentGameSession.get());
}

void TGameClient::finishTutorialCinematic()
{
    game::TSessiondumpGameSession* dump = sessiondump();
    if (!dump || !dump->isTutorial())
        return;
    dump->disconnectNow();
}

void TGameClient::onWorldEntered()
{
    // Entering a world dismisses the character list (login and alt switch).
    Q_EMIT characterSelectionClosed();
    // The map handler keeps the minimap's floor and fields current.
    setMinimapWorldmapProtocolHandler(worldmapProtocol());
    // Host: the pack's travel path probe (game/mapsession) reads the minimap's
    // waypoint costs (TWaypointCosts) of the session's world.
    tibia::game::setWaypointCostFactory([this]() -> std::shared_ptr<minimap::TWaypointCosts> {
        return minimap::snapshotWaypointCosts(minimapTileStorage().get());
    });
    Q_EMIT worldEntered();

    for (qsizetype i = 0; i < m_options.say.size(); ++i) {
        QTimer::singleShot(1000 * (i + 1), this, [text = m_options.say[i].toStdString()] {
            // A say (CIP chat message mode Say).
            if (const game::TGameSessionBase* session = liveSession(); session && session->canPerformGameAction() && !text.empty())
                protocol::gameclient::sendTalk(1, 0, "", text);
        });
    }

    if (!m_options.walk.isEmpty()) {
        static const QHash<QString, tibia::client::Direction> directions = {
            { QStringLiteral("n"), tibia::client::North },      { QStringLiteral("ne"), tibia::client::NorthEast },
            { QStringLiteral("e"), tibia::client::East },       { QStringLiteral("se"), tibia::client::SouthEast },
            { QStringLiteral("s"), tibia::client::South },      { QStringLiteral("sw"), tibia::client::SouthWest },
            { QStringLiteral("w"), tibia::client::West },       { QStringLiteral("nw"), tibia::client::NorthWest },
        };
        auto* walker = new QTimer(this);
        auto next = std::make_shared<qsizetype>(0);
        connect(walker, &QTimer::timeout, this, [this, walker, next] {
            if (*next >= m_options.walk.size()) {
                walker->stop();
                return;
            }
            game::TGameSessionBase* session = liveSession();
            input::TPlayerMovementIntentHandler* intent = session ? session->movementIntent() : nullptr;
            const std::shared_ptr<creatures::TCreature> player =
                session && session->creatureStorage() ? session->creatureStorage()->player() : nullptr;
            if (!session || !session->isOnline() || !intent || intent->hasGoal() || intent->isPredicting()
                || (player && player->isMoving()))
                return;
            const QString token = m_options.walk[*next].trimmed().toLower();
            ++*next;
            if (token.contains(QLatin1Char(':'))) {
                // dx:dy walks to a field relative to the character.
                const QStringList offset = token.split(QLatin1Char(':'));
                const Position at = session->localPlayerPosition();
                const Position goal(at.x + offset.value(0).toInt(), at.y + offset.value(1).toInt(), at.z);
                if (goal.isValid())
                    intent->handleMovementIntent(
                        std::make_shared<input::TMovementIntentTargetCoordinate>(goal.x, goal.y, goal.z, true));
                return;
            }
            const auto direction = directions.value(token, tibia::client::InvalidDirection);
            // A step (pack notify onWalk, as a key step).
            if (direction != tibia::client::InvalidDirection && session->canPerformGameAction()) {
                tibia::game::notify("onWalk", direction);
                intent->step(input::toMovementDirection(direction));
            }
        });
        QTimer::singleShot(2000, walker, [walker] { walker->start(5); });
    }

    if (m_options.playForSeconds > 0) {
        QTimer::singleShot(m_options.playForSeconds * 1000, this, [this] {
            const bool online = sessionOnline();
            if (game::TGameserverGameSession* live = session())
                live->disconnectNow();
            else if (game::TSessiondumpGameSession* dump = sessiondump())
                dump->disconnectNow();
            QCoreApplication::exit(online ? 0 : 1);
        });
    }
}

config::TCharacterConfigurationManager* TGameClient::characterConfigurationManager() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_currentGameSession.get());
    return session ? session->characterConfigurationManager().get() : nullptr;
}

config::TSidebarOptions* TGameClient::sidebarOptions() const
{
    config::TCharacterConfigurationManager* manager = characterConfigurationManager();
    return manager ? manager->sidebarOptions() : nullptr;
}

void TGameClient::onWorldLeft()
{
    tibia::game::clearWaypointCostFactory();
    saveMinimapToDisk();
    if (config::TCharacterConfigurationManager* manager = characterConfigurationManager())
        manager->saveConfiguration();
    if (chat::TTextStorage* texts = textStorage())
        texts->clear();
    if (m_pSoundController)
        m_pSoundController->handleSoundTrigger(true);
    Q_EMIT worldLeft();
    // CIP: after logout return to character selection while the account session remains.
    ensureLoginSurface();
}

void TGameClient::setWindow(QQuickWindow* window)
{

    // CIP: the window's frames drive the client.
    connect(window, &QQuickWindow::afterAnimating, this, &TGameClient::onBeforeRenderFrameMainThread);
    connect(window, &QQuickWindow::frameSwapped, this, &TGameClient::onRenderThreadFrameSwapped, Qt::DirectConnection);
}

void TGameClient::onRenderThreadFrameSwapped()
{
    // The render time ring has a lock of its own.
    m_frameDurations->finish();
    QMetaObject::invokeMethod(this, [this] {
        if (auto* session = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get()))
            session->updatePlayerMovementIntentHandler();
        screenshot::TScreenshotManager* screenshotManager = screenshot::TScreenshotManager::instance();
        gamewindow::TGameWindowController* gameWindow = gameWindowController();
        if (screenshotManager && gameWindow && gameWindow->gameWindowState() == QLatin1String("INGAME")
            && screenshotManager->takeScreenshotIfEventIsWainting() && m_pFPSDropsLogger)
            m_pFPSDropsLogger->resetFrameDurations();
    }, Qt::QueuedConnection);
}

void TGameClient::wakeUpPlayerMovementIntentHandler()
{
    if (auto* session = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get()))
        session->updatePlayerMovementIntentHandler();
}

void TGameClient::onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<creatures::TCreature> pPlayer)
{
    if (gamewindow::TSidebarManager* sidebar = sidebarManager())
        sidebar->emitCheckBounds();
    const std::shared_ptr<creatures::TCreature> player = pPlayer.lock();
    if (!player)
        return;
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionTryToSwitchHotkeySet>(player->name(), false));
    if (m_pFPSDropsLogger)
        connect(player.get(), &creatures::TCreature::positionWasUpdated, m_pFPSDropsLogger.get(),
                &game::FPSDropsLogger::setPlayerPosition, Qt::QueuedConnection);
    QTimer::singleShot(0, this, [this] {
        if (m_pConnectingDialogController)
            m_pConnectingDialogController->dialog()->close();
        if (gamewindow::TGameWindowController* gameWindow = gameWindowController())
            gameWindow->showGameScreen();
    });
}

void TGameClient::updateFrameStatistics()
{
    m_frameDurations->start();
    m_pSlidingFPSCounter->startNewFrame();
    if (m_fpsHistory)
        m_fpsHistory->fpsCounter()->startNewFrame();
    if (m_pFPSDropsLogger)
        m_pFPSDropsLogger->addFrameDuration(m_pSlidingFPSCounter->lastFrameDuration());
}

void TGameClient::TDurationStatistics::add(const qint64 Duration)
{
    if (count == 0) {
        average = Duration;
        minimum = Duration;
        maximum = Duration;
        count = 1;
        return;
    }
    average = static_cast<qint64>((static_cast<quint64>(average) * count + static_cast<quint64>(Duration)) / (count + 1));
    if (Duration < minimum)
        minimum = Duration;
    if (maximum < Duration)
        maximum = Duration;
    ++count;
}

void TGameClient::initialize()
{
    ::tibia::client::TGameClientBase::initialize();
    // CIP: the marker game actions reach the client's marker storage. Host:
    // qtclient's handler takes the client router's actions here.
    m_pMinimapMarkerGameActionHandler = std::make_shared<minimap::TMinimapMarkerGameActionHandler>(minimapMarkerStorage());
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal,
            m_pMinimapMarkerGameActionHandler.get(), &minimap::TMinimapMarkerGameActionHandler::handleGameAction);
    // CIP: the marker file is read once the minimap is up.
    minimapTileDiskIO()->loadMinimapMarkerFileContentFromDiskIfPossible();
    // CIP: the news storage over the client's clock and its disk cache: the
    // storage asks the cache to save, the cache hands back what it loaded,
    // and the cached news load now.
    m_pNewsStorage = std::make_shared<news::TNewsStorage>(shared::TClockService::process());
    m_pNewsDiskIO = std::make_shared<news::TNewsDiskIOAsync>(tibiaFileSystemHelper());
    connect(m_pNewsStorage.get(), &news::TNewsStorage::startSavingNewsDataToDisk, m_pNewsDiskIO.get(),
            &news::TNewsDiskIO::saveNewsDataToDisk);
    connect(m_pNewsDiskIO.get(), &news::TNewsDiskIO::loadingNewsDataSucceeded, m_pNewsStorage.get(),
            &news::TNewsStorage::loadFromDisk);
    m_pNewsDiskIO->loadNewsDataFromDiskIfPossible();
    // CIP: the frame rate limit's timer and the timer of a window that shows
    // no frames both prepare the next frame.
    m_frameLimitTimer.setTimerType(Qt::PreciseTimer);
    m_frameLimitTimer.setSingleShot(true);
    connect(&m_frameLimitTimer, &QTimer::timeout, this, &TGameClient::onBeforeRenderFrameMainThread);
    m_idleTick.setSingleShot(true);
    connect(&m_idleTick, &QTimer::timeout, this, &TGameClient::onBeforeRenderFrameMainThread);
    // CIP: the current session's disconnect, connection and game login reach
    // the client once, after their signal returned; the disconnect is
    // announced right after the client handled it.
    m_gameSessionDisconnected.setAsyncDelay(1);
    m_gameSessionDisconnected.setOnlyFirstSignal(true);
    connect(&m_gameSessionDisconnected, &shared::TQtConnectionWrapper::signal, this, &TGameClient::onGameSessionDisconnected);
    connect(
        &m_gameSessionDisconnected, &shared::TQtConnectionWrapper::signal, &m_gameSessionDisconnected,
        [this] { Q_EMIT currentGamesessionDisconnected(m_currentGameSession); }, Qt::DirectConnection);
    connect(&m_gameSessionConnected, &shared::TQtConnectionWrapper::signal, this, &TGameClient::onGameSessionConnected);
    connect(&m_gameSessionLoginSuccessful, &shared::TQtConnectionWrapper::signal, this,
            &TGameClient::onGameSessionLoginSuccessful);
}

void TGameClient::initializeQmlContext()
{
    ::tibia::client::TGameClientBase::initializeQmlContext();
    initializeClientObjects();
}

void TGameClient::initializeClientObjects()
{
    // CIP: the clipboard, the URL tools and the game's clipboard writer over both.
    m_pClipboardHelper = std::make_shared<shared::TClipboardHelper>();
    m_pTibiaUrlTools = std::make_shared<shared::TTibiaUrlTools>();
    m_pTibiaClipboardHelper = std::make_shared<shared::TTibiaClipboardHelper>(m_pClipboardHelper, m_pTibiaUrlTools);
    // CIP: the client's white/blacklist.
    m_pWhiteBlacklist = std::make_shared<chat::TWhiteBlacklist>(clientOptions());
    m_pWhiteBlacklist->loadFromOptions();
    // CIP: the client's mouse mapping with the three control schemes' tables,
    // following the options' control and loot scheme from now on.
    m_pGameActionMapper = std::make_shared<input::TGameActionMapper>();
    connect(clientOptions(), &config::TClientOptions::controlOrLootSchemeOptionsChanged, this,
            &TGameClient::onControlOrLootSchemeOptionsChanged);
    onControlOrLootSchemeOptionsChanged();
    initializeCreatureMarkOptions();
    // CIP: the chat mode manager; a disconnected session ends a temporary chat on.
    m_pChatModeManager = std::make_shared<chat::TChatModeManager>();
    connect(this, &TGameClient::currentGamesessionDisconnected, m_pChatModeManager.get(),
            &chat::TChatModeManager::onCurrentGamesessionDisconnected);
    // CIP: the client's clock (the frame counters, the premium time storage,
    // the keyboard shortcuts and every session read it). Host: the process
    // clock, which the frame counters made in the constructor already read.
    m_pClockService = shared::TClockService::process();
    // CIP: the keyboard state follows the application's state; the key event
    // handler, the shortcuts (over the client options, the chat mode and the
    // clock) and the shortcut recorder go into the application event filters.
    m_pKeyboardStatusStorage = std::make_shared<input::TKeyboardStatusStorage>();
    m_pKeyEventHandler = std::make_shared<input::TKeyEventHandler>(m_pKeyboardStatusStorage);
    if (auto* guiApplication = qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        connect(guiApplication, &QGuiApplication::applicationStateChanged, m_pKeyboardStatusStorage.get(),
                &input::TKeyboardStatusStorage::onApplicationStateChanged);
    const std::weak_ptr<chat::TChatModeManager> chatModeManager = m_pChatModeManager;
    m_pKeyboardShortcutHandler = std::make_shared<input::TKeyboardShortcutHandler>(
        [chatModeManager] {
            const std::shared_ptr<chat::TChatModeManager> manager = chatModeManager.lock();
            return manager ? manager->chatMode() : 0;
        },
        m_pClockService);
    // Host: qtclient's handler takes the chat mode as a function, so the
    // client tells it when the mode changed.
    connect(m_pChatModeManager.get(), &chat::TChatModeManager::chatModeChanged, m_pKeyboardShortcutHandler.get(),
            &input::TKeyboardShortcutHandler::onHotkeyOptionsChanged);
    m_pRecordKeyboardShortcutHandler = std::make_shared<input::TRecordKeyboardShortcutHandler>();
    m_pEventFilterManager = std::make_shared<input::TEventFilterManager>(m_pKeyboardShortcutHandler, m_pKeyEventHandler,
                                                                         m_pRecordKeyboardShortcutHandler);
    // CIP: the walk keys over the keyboard state, the options and the chat
    // mode, and their movement intent.
    m_pPlayerMovementInputInterpreter = std::make_shared<input::TPlayerMovementInputInterpreter>(
        m_pKeyboardStatusStorage, clientOptions(), m_pChatModeManager.get());
    m_pKeyboardInputToGameActionConverter =
        std::make_shared<input::TKeyboardInputToGameActionConverter>(m_pPlayerMovementInputInterpreter);
    connect(m_pKeyboardStatusStorage.get(), &input::TKeyboardStatusStorage::checkForChangeAfterDelay, this,
            &TGameClient::checkKeyboardStatusStorageAfterDelay);
    // CIP: the client's premium time storage; each current session gets it.
    m_pPremiumTimeStorage = std::make_shared<game::TPremiumTimeStorage>();
    // CIP: the website and clipboard game actions. Host: the websites are the
    // launcher environment's URLs.
    m_pOpenWebsiteGameActionHandler = std::make_shared<input::TOpenWebsiteGameActionHandler>(m_gameWindowSetup.env);
    m_pClipboardGameActionHandler = std::make_shared<input::TClipboardGameActionHandler>(
        m_pTibiaClipboardHelper, sharedClientOptions(), m_pTibiaUrlTools);
    // CIP: the item prices. Host: the process-wide storage.
    m_pItemPriceStorage = notOwned(analysers::TItemPriceStorage::instance());
    // CIP: the [GRAPHICS] timings are taken from the client settings once: a
    // missile's flight per field, how long numerical effects show and merge,
    // and how long a creature's temporary mark shows; [DEBUG] whether speech
    // bubbles show their bounding rectangle.
    const std::shared_ptr<config::TTibiaConfig> settings = tibiaConfig();
    const QString graphics = QStringLiteral("GRAPHICS");
    appearances::TMissileEffectInstance::setMillisecondsPerField(
        settings->clientSetting(graphics, QStringLiteral("missileFlyDelayMsec"), 150));
    effects::TNumericalEffect::setDuration(settings->clientSetting(graphics, QStringLiteral("numericalEffectDisplayDurationMsec"), 1000));
    effects::TNumericalEffect::setMergeDuration(settings->clientSetting(graphics, QStringLiteral("numericalEffectMergeDurationMsec"), 100));
    creatures::TCreature::setTimedSquareDuration(
        settings->clientSetting(graphics, QStringLiteral("temporaryMarkDisplayDurationMsec"), 1000));
    qmlcomponents::TSpeechBubbleController::setShowBoundingRect(
        settings->clientSettingBool(QStringLiteral("DEBUG"), QStringLiteral("showSpeechBubbleBoundingRect"), false));
    // CIP: the dialog stack over the event filters is the dialogs' stack from
    // now on, and the client's clock the appearance renderers'.
    m_pDialogStackController = std::make_shared<gamewindow::TDialogStackController>(m_pEventFilterManager);
    gamewindow::TDialogStackController::setInstance(m_pDialogStackController);
    qmlcomponents::setAppearanceInstanceRendererClockService(m_pClockService);
}

void TGameClient::initializeCreatureMarkOptions()
{
    using config::EClientOption;
    config::TClientOptions* const options = clientOptions();
    const auto updateFrames = [this] {
        const config::TClientOptions* const current = clientOptions();
        creatures::TCreature::setShowStaticSquare(current->boolOption(EClientOption::combatShowPvpFrames));
        creatures::TCreature::setShowTimedSquare(current->boolOption(EClientOption::combatShowFrames));
        creatures::TCreature::setShowTargetFrame(current->boolOption(EClientOption::gameWindowShowTragetFrame));
    };
    updateFrames();
    connect(options, &config::TClientOptions::creatureMarksOptionsChanged, options, updateFrames);
    creatures::TCreature::setShowSelectionFrame(!options->boolOption(EClientOption::mouseHighlightTarget));
    m_pEngine->rootContext()->setContextProperty(QStringLiteral("UICachingEnabled"), false);
    connect(options, &config::TClientOptions::optionChanged, options, [this](const config::EClientOption OptionID) {
        if (OptionID == EClientOption::mouseHighlightTarget)
            creatures::TCreature::setShowSelectionFrame(!clientOptions()->boolOption(EClientOption::mouseHighlightTarget));
    });
}

void TGameClient::setClientWindowTitle(QObject* ClientWindow)
{
    // Host: the release title, the product's application name (CipSoft's
    // pinned test-server build shows its test-server client title here).
    ClientWindow->setProperty("windowTitle", QCoreApplication::applicationName());
}

void TGameClient::onControlOrLootSchemeOptionsChanged()
{
    m_pGameActionMapper->setControlScheme(clientOptions()->controlScheme());
    m_pGameActionMapper->setLootScheme(clientOptions()->lootScheme());
}

bool TGameClient::initializeWithStartMode(const int StartMode, const TGameWindowSetup& Setup)
{
    m_startMode = StartMode;
    m_gameWindowSetup = Setup;
    // CIP (constructor): the client window's title.
    if (Setup.placeholder)
        setClientWindowTitle(Setup.placeholder);
    // Host: the preview runs no engine and has no assets to wait for.
    if (Setup.preview)
        return createGameWindow(Setup);
    // Host: the QML engine, the options of this run and qtclient's engine
    // come before the client's setup steps.
    m_pEngine = Setup.engine;
    m_options = Setup.options;
    if (!startEngine(Setup.argv0, Setup.workDir))
        return false;
    initialize();
    setMainWindow(Setup.window);
    if (!shared::TTibiaFileSystemHelper::hasForcePatchMarker()) {
        assetManager()->startLoading();
        connect(assetManager().get(), &assets::TAssetManager::assetDataAccessible, this, &TGameClient::onAssetFinishedLoading);
    } else if (startLauncherToPatchClientOrReportError()) {
        Q_EMIT clientReadyToClose();
    }
    return true;
}

void TGameClient::onAssetFinishedLoading(const assets::TAsset Asset, std::shared_ptr<shared::TReadOnlyMemoryBlob> pData)
{
    Q_UNUSED(pData);
    if (Asset.type() != assets::TAsset::Appearances)
        return;
    disconnect(assetManager().get(), &assets::TAssetManager::assetDataAccessible, this, &TGameClient::onAssetFinishedLoading);
    QTimer::singleShot(0, this, &TGameClient::startNetworkTimerAndShowGameWindow);
}

void TGameClient::startNetworkTimerAndShowGameWindow()
{
    m_networkTimer.setTimerType(Qt::PreciseTimer);
    m_networkTimer.start();
    m_networkTimer.setInterval(50);
    connect(
        &m_networkTimer, &QTimer::timeout, &m_networkTimer,
        [this] {
            if (m_currentGameSession)
                m_currentGameSession->processNetwork();
        },
        Qt::DirectConnection);
    if (m_startMode != 0)
        throw std::logic_error("can't happen");
    m_showGameWindowTimer.start();
}

void TGameClient::initializeSound()
{
    ::tibia::client::TGameClientBase::initializeSound();
    if (!m_pSoundController)
        return;
    // Host: qtclient's assets read the sound bank through the sound
    // controller's instance(), so it is in place before them.
    sound::TSoundController::setInstance(m_pSoundController.get());
    // Out of the game the title music plays once the sound bank is known.
    connect(m_pSoundMetadataManager.get(), &sound::TSoundMetadataManager::metadataChanged, this, [this] {
        if (!sessionOnline())
            m_pSoundController->handleSoundTrigger(true);
    });
}

void TGameClient::onWindowSizeChanged()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCloseAllMultiActionButtons>(0, 0));
}

void TGameClient::onBeforeRenderFrameMainThread()
{
    // Host: QML that runs during the next frame updater's advance must not
    // re-enter it (a nested frame would resume the real-time movement clock
    // while the outer advance still holds the predicted one).
    if (m_inTick)
        return;
    m_inTick = true;

    // Host: the engine's network and dispatcher work.
    pollEngineHost();
    m_idleTick.start(sessionOnline() ? 500 : 100);
    if (m_frameRateLimitEnabled && !frameSlotReached(nowMicros())) {
        m_inTick = false;
        return;
    }
    qint64 start = nowMicros();

    // The next frame shows one render time from now (the mean of the last
    // frames' render times).
    const qint64 renderTimeMicros = m_frameDurations->averageNs() / 1000;
    updateFrameStatistics();
    // CIP: the game window's render driver asks for the frame after this one.
    if (gamewindow::TGameWindowController* controller = gameWindowController())
        controller->requestRenderDriverUpdate();
    m_predictor->setOffset(renderTimeMicros);
    const qint64 nextFrameTimestamp = m_predictor->predict();
    // Host: the appearance animations and sounds run on the frame clock.
    updateWithTimestamp(static_cast<qint64>(shared::TClockService::frameMillis()));
    auto* session = m_started ? dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get()) : nullptr;
    if (session) {
        session->onPrepareNextFrameToRender(nextFrameTimestamp);
        m_sessionFrameUpdateDurations.add(nowMicros() - start);
        session->updatePlayerMovementIntentHandler();
        start = nowMicros();
        beforeRenderFrameGameWindow();
        m_gameWindowFrameUpdateDurations.add(nowMicros() - start);
        session->updatePlayerMovementIntentHandler();
    }
    m_inTick = false;
}

bool TGameClient::frameSlotReached(qint64 now)
{
    // Frames go on a fixed grid of 1/limit seconds from when the limit was set.
    qint64 remaining = 0;
    if (m_activeFrameRateLimit != m_frameRateLimit) {
        m_activeFrameRateLimit = m_frameRateLimit;
        m_frameGridBase = now;
        m_frameSlot = now;
        m_frameInterval = 1000000 / m_frameRateLimit;
        remaining = m_frameInterval;
    } else {
        const qint64 elapsed = now - m_frameSlot;
        if (elapsed <= m_frameInterval)
            remaining = m_frameInterval - elapsed;
    }
    if (remaining > 0) {
        if (remaining >= 2000) {
            m_frameLimitTimer.start(static_cast<int>(remaining / 1000 - 1));
            return false;
        }
        QThread::usleep(static_cast<unsigned long>(remaining));
    }
    now = nowMicros();
    m_frameSlot = now - (now - m_frameGridBase) % m_frameInterval;
    return true;
}

void TGameClient::fail(const QString& message)
{
    Q_EMIT showErrorAsPopupAndCriticalLog(message);
    if (!m_options.autoCharacter.isEmpty() || !m_options.replay.isEmpty() || m_options.playForSeconds > 0)
        QCoreApplication::exit(2);
    // A refused reconnect closes the character list before the socket fails, and
    // the login mask stays hidden while the account is still authenticated.
    if (!sessionOnline())
        ensureLoginSurface();
}

void TGameClient::checkForCrashDump()
{
    const shared::TTibiaFileSystemHelper fileSystem;
    const QString crashDumpFilePath = fileSystem.getLatestCrashDumpFilePath();
    if (crashDumpFilePath.isEmpty())
        return;
    auto crashReportData = std::make_shared<shared::TCrashReportData>(fileSystem.readCrashDumpMetaData(crashDumpFilePath));
    const QString sessiondumpFileName = shared::TTibiaFileSystemHelper::getCrashDumpSessionFileName(crashDumpFilePath);
    const QString sessiondumpFilePath =
        fileSystem.getFilePath(shared::TTibiaFileSystemHelper::DIRECTORY_CRASHDUMP, sessiondumpFileName);
    m_crashReportSender = std::make_shared<shared::TCrashReportSender>(crashDumpFilePath, sessiondumpFilePath, crashReportData);
    m_crashReportDialog->showDialog(crashReportData);
}

void TGameClient::onFpsTrackingActiveChanged(const bool FpsTrackingActive)
{
    if (!FpsTrackingActive) {
        m_fpsHistory.reset();
        m_fpsHistoryController.reset();
        m_pFPSDropsLogger.reset();
        return;
    }
    auto clockService = shared::TClockService::process();
    m_fpsHistory = std::make_shared<systemsurvey::TFPSHistory>(
        std::make_shared<systemsurvey::TFPSCounter>(clockService), clockService);
    // Host: CipSoft posts to [URLS] fpsHistoryRecipient; qtclient posts to the
    // qtc-aac hardware receiver.
    const QUrl fpsHistoryRecipientUrl = app::LauncherEnv::load(QDir::currentPath()).fpsHistoryRecipientUrl();
    auto uploader = std::make_shared<systemsurvey::TFPSHistoryUploader>(
        fpsHistoryRecipientUrl,
        std::make_shared<systemsurvey::TFPSHistoryToJsonConverter>(),
        std::make_shared<shared::TJsonDocumentHttpPostSender>());
    const int renderer = app::rendererIndexForGraphicsApi(QQuickWindow::graphicsApi());
    m_fpsHistoryController = std::make_shared<systemsurvey::TFPSHistoryController>(uploader, m_fpsHistory, renderer);
    const QString group = QStringLiteral("FPSHISTORY");
    startFpsHistoryTimers(config::clientSettingAtLeast(group, QStringLiteral("sampleRateMsec"), 1000, 100),
                          config::clientSettingAtLeast(group, QStringLiteral("uploadRateSec"), 60, 1));
    // CIP: the frame rate drops go to the same recipient. Host: qtclient's
    // logger keeps no local log file (CipSoft passes one in the file system
    // helper's directory).
    m_pFPSDropsLogger = std::make_shared<game::FPSDropsLogger>(fpsHistoryRecipientUrl, clientOptions());
}

void TGameClient::startFpsHistoryTimers(const qint64 sampleRateMsec, const qint64 uploadRateSec)
{
    if (m_fpsHistory) {
        m_fpsSampleTimer.setInterval(int(sampleRateMsec));
        connect(&m_fpsSampleTimer, &QTimer::timeout, m_fpsHistory.get(),
                &systemsurvey::TFPSHistory::sampleCurrentFPSInformationAndClearFPSCounter);
        m_fpsSampleTimer.start();
    }
    if (m_fpsHistoryController) {
        auto* controller = m_fpsHistoryController.get();
        m_fpsUploadTimer.setInterval(int(uploadRateSec * 1000));
        connect(&m_fpsUploadTimer, &QTimer::timeout, controller,
                &systemsurvey::TFPSHistoryController::uploadHistoryAndClearIt);
        connect(this, &TGameClient::currentGamesessionDisconnected, controller,
                &systemsurvey::TFPSHistoryController::uploadHistoryAndClearIt);
        connect(this, &TGameClient::clientReadyToClose, controller,
                &systemsurvey::TFPSHistoryController::uploadHistoryAndClearIt);
        m_fpsUploadTimer.start();
    }
}

void TGameClient::sendCrashReport(const bool SendFullReport)
{
    const shared::TTibiaFileSystemHelper fileSystem;
    QByteArray minidump;
    QByteArray sessiondump;
    if (SendFullReport) {
        minidump = fileSystem.readFileIfExists(m_crashReportSender->minidumpPath());
        sessiondump = fileSystem.readFileIfExists(m_crashReportSender->sessiondumpPath());
    }
    // Host: CipSoft reads [URLS] crashReportUrl; qtclient's comes from the mod
    // configuration (urls.crashReportPath). Without one nothing is posted.
    const QUrl crashReportUrl = app::LauncherEnv::load(QDir::currentPath()).crashReportUrl();
    if (crashReportUrl.isValid())
        m_crashReportSender->send(minidump, sessiondump, crashReportUrl, SendFullReport);
    fileSystem.removeCrashDumps();
}

void TGameClient::onOptionsTrackingActiveChanged(const bool OptionsTrackingActive)
{
    if (!OptionsTrackingActive) {
        m_pClientOptionsUploader.reset();
        return;
    }
    if (m_pClientOptionsUploader)
        return;
    // Host: CipSoft posts to [URLS] fpsHistoryRecipient; qtclient's comes from
    // the launcher environment, as for the FPS history.
    m_pClientOptionsUploader = std::make_shared<config::TClientOptionsUploader>(
        app::LauncherEnv::load(QDir::currentPath()).fpsHistoryRecipientUrl());
}

void TGameClient::onSpritesheetUncompressFailed(const QString& SpritesheetID)
{
    Q_UNUSED(SpritesheetID);
    const QString message = qtTrId("asset_file_corrupted_please_restart");
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionSetStatusLine>(message, chat::systemRed()));
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionAddTextMessage>(
        input::EGameAction::ADD_SYSTEM_CHAT_MESSAGE, message, chat::systemRed()));
    // The launcher patches the client on its next start.
    shared::TTibiaFileSystemHelper::createForcePatchMarker();
}

void TGameClient::onAssetsComplete()
{
    if (assetManager()->state() == assets::TAssetManager::Complete) {
        Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionSoundTrigger>(true));
        return;
    }
    assetManager()->stopLoading();
    saveClientState();
    setCurrentGameSession(nullptr);
    Q_EMIT clientReadyToClose();
}

void TGameClient::onAssetsError()
{
    if (startLauncherToPatchClientOrReportError())
        Q_EMIT clientReadyToClose();
}

void TGameClient::saveClientState()
{
    minimapTileStorage()->saveAllTilesToDisk();
    if (m_pNewsStorage)
        m_pNewsStorage->saveToDisk();
    minimapTileDiskIO()->waitForDone();
    if (m_pNewsDiskIO)
        m_pNewsDiskIO->waitForFinished();
}

bool TGameClient::startLauncherOrReportError()
{
    const bool started =
        shared::TTibiaFileSystemHelper::startLauncher(shared::TTibiaFileSystemHelper::LAUNCHER_START_SHOW_UI, QString());
    if (!started)
        Q_EMIT showErrorAsPopupAndCriticalLog(qtTrId("launcher_start_failed"));
    return started;
}

bool TGameClient::startLauncherToPatchClientOrReportError()
{
    if (!m_packageIsUpToDate) {
        if (shared::TTibiaFileSystemHelper::startLauncherToPatchClient())
            return true;
        Q_EMIT showErrorAsPopupAndCriticalLog(qtTrId("launcher_start_failed"));
        return false;
    }
    // The launcher just found the files up to date: patching now cannot help,
    // the next start patches.
    assetManager()->stopLoading();
    shared::TTibiaFileSystemHelper::createForcePatchMarker();
    if (gameWindowController()) {
        Q_EMIT showErrorAsPopupAndCriticalLog(qtTrId("launcher_start_pointless_due_to_files_uptodate"));
        return false;
    }
    // CIP: the game window shows the error once it is made, and it is made now.
    m_pendingErrorMessage = qtTrId("launcher_start_pointless_due_to_files_uptodate");
    startNetworkTimerAndShowGameWindow();
    return false;
}

void TGameClient::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    const input::EGameAction type = pGameAction->gameActionType();
    if (type == input::EGameAction::SESSIONDUMP_START) {
        // CIP: the chosen replay replaces whatever was queued; a tutorial
        // entry plays in the tutorial's session.
        const auto start = std::dynamic_pointer_cast<const input::TGameActionSessiondumpStart>(pGameAction);
        m_gameSessionMetaInfoQueue.clear();
        std::shared_ptr<game::TSessiondumpGameSessionMetaInfo> metaInfo =
            std::make_shared<game::TSessiondumpGameSessionMetaInfo>();
        if (start->sessiondumpEntry().hasHints)
            metaInfo = std::make_shared<game::TTutorialSessiondumpGameSessionMetaInfo>();
        metaInfo->sessiondump = start->sessiondumpEntry();
        setGameSessionSettings(*metaInfo);
        metaInfo->startedByGameAction = true;
        m_gameSessionMetaInfoQueue.push_back(std::move(metaInfo));
        autoconnectNextGameSessionInQueue();
        return;
    }
    if (type != input::EGameAction::OPEN_LAUNCHER)
        return;
    saveClientState();
    if (startLauncherOrReportError())
        Q_EMIT clientReadyToClose();
}

void TGameClient::onOpenLauncherRequested()
{
    saveClientState();
    if (startLauncherOrReportError())
        Q_EMIT clientReadyToClose();
}

void TGameClient::onTryToPatchClient()
{
    saveClientState();
    if (startLauncherToPatchClientOrReportError())
        Q_EMIT clientReadyToClose();
}

void TGameClient::onPatchClient()
{
    if (startLauncherToPatchClientOrReportError())
        Q_EMIT clientReadyToClose();
}

void TGameClient::onStartTargetSelection()
{
    // Host: CipSoft reads the controller from the root context's property;
    // qtclient's game window holds it.
    if (gamewindow::TTargetSelectionController* controller = targetSelectionController())
        controller->setSelecting(true);
}

}

