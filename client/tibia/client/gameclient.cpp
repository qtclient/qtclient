#include "gameclient.h"
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
#include "prey/preyciptypes.h"
#include "news/newsstorage.h"

#include <algorithm>
#include "network/networkciptypes.h"
#include "imbuing/imbuingciptypes.h"
#include "highscores/highscoresciptypes.h"
#include "gameWidnow/gameWidnowciptypes.h"
#include "exivaoptions/exivaoptionsciptypes.h"
#include "exaltation/exaltationciptypes.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "common/commonciptypes.h"
#include "client/clientciptypes.h"
#include "crashreportdata.h"
#include "crashreportsender.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/createaccountdialogcontroller.h"
#include "gamewindow/createcharacterdialogcontroller.h"
#include "gamewindow/gamesessiondisconnectreactioncontroller.h"
#include "gamewindow/storeandresourcebalancehelpercontroller.h"
#include "shared/tibiafilesystemhelper.h"
#include "config/configciptypes.h"
#include "bossbestiary/bossbestiaryciptypes.h"
#include "authentication/authenticationciptypes.h"
#include "authentication/cloudflarechallengecontroller.h"
#include "authentication/emailcoderequestuploader.h"
#include "authentication/loginconfirmationcoderequestuploader.h"
#include "assets/assetsciptypes.h"
#include "shared/sharedciptypes.h"
#include "game/gameciptypes.h"
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
#include "game/currentgamesession.h"
#include "game/tgamenotify.h"
#include "client/gamecore.h"
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
#include "minimap/minimapmanager.h"
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
#include "network/gameserverconnection.h"
#include "protocol/clientmessages.h"
#include "shared/tibiafilesystemhelper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QQuickWindow>
#include <QThread>

#include <utility>
#include <tibia/game/gamesession.h>

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

/// The session setSessionInstances installed: from before its controllers
/// connect until it is released.
game::TGameSessionBase* g_sessionInstances = nullptr;

/// Host shim: the classes whose instance() host and pack code still read
/// resolve to `session`'s members (null: none).
void setSessionInstances(game::TGameSessionBase* session)
{
    g_sessionInstances = session;
    worldmap::TWorldMapStorage::setInstance(session ? session->worldMapStorage().get() : nullptr);
    creatures::TCreatureStorage::setInstance(session ? session->creatureStorage().get() : nullptr);
    container::TContainerStorage::setInstance(session ? session->containerStorage().get() : nullptr);
    effects::TEffectStorage::setInstance(session ? session->effectStorage().get() : nullptr);
    game::TPlayerData::setLiveInstance(session ? session->playerData().get() : nullptr);
}

/// Set while the client shuts down: a released session is deleted at once.
bool g_releaseGameSessionsNow = false;

/// A new game session. Released inside one of its own signals (it disconnected),
/// it is deleted once control is back in the event loop.
template<typename Session>
std::shared_ptr<Session> createGameSession()
{
    return std::shared_ptr<Session>(new Session(), [](Session* session) {
        if (g_releaseGameSessionsNow)
            delete session;
        else
            session->deleteLater();
    });
}

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
                         config::TClientOptions* ClientOptions, QObject* parent)
    : cip::tibia::client::TGameClient(parent)
    , ::tibia::client::TGameClientBase(Engine, std::move(TibiaFileSystemHelper), ClientOptions)
    , m_outfitDialogStorage(std::make_unique<game::TOutfitDialogStorage>())
    , m_frameDurations(std::make_unique<worldmap::TFrameDurationRing>())
    , m_predictor(std::make_unique<worldmap::TNextFrameTimestampPredictor>(shared::TClockService::process()))
    , m_sessiondumpCatalogLoader(std::make_shared<shared::TDownloadPool>(
          1, std::make_shared<shared::QNetworkAccessManagerWrapper>(std::make_shared<QNetworkAccessManager>())))
{
    game::TOutfitDialogStorage::setInstance(m_outfitDialogStorage.get());
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
    // The Cyclopedia map's static data and the sprite resolver trade the subarea
    // masks as RESOLVE_SPRITE / SPRITE_RESOLVED; the masks are asked for once the
    // main window exists.
    {
        game::TInternalGameActionRouter* router = m_pGameActionRouter->internalRouter();
        cyclopedia::TCyclopediaMapStaticDataStorage& mapStaticData = *cyclopediaMapStaticDataStorage();
        // CIP: the storage's asset requests go to the client's asset loading.
        connect(&mapStaticData, &cyclopedia::TCyclopediaMapStaticDataStorage::newAssetDataToLoad, &mapStaticData,
                [](const tibia::assets::TAsset& asset, const bool prioritized) {
                    if (auto* loader = sprites::TSpriteSystem::instance().assetLoader())
                        loader->load(asset, prioritized);
                });
        connect(&mapStaticData, &cyclopedia::TCyclopediaMapStaticDataStorage::publishGameAction, router,
                &game::TInternalGameActionRouter::handleGameActionInternal);
        connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, &mapStaticData,
                &cyclopedia::TCyclopediaMapStaticDataStorage::handleGameAction);
        connect(this, &TGameClient::mainWindowInitialized, &mapStaticData,
                &cyclopedia::TCyclopediaMapStaticDataStorage::onGameClientMainWindowInitialized);
        sprites::TSpriteResolver& spriteResolver = sprites::TSpriteSystem::instance().resolver();
        connect(&spriteResolver, &sprites::TSpriteResolver::publishGameAction, router,
                &game::TInternalGameActionRouter::handleGameActionInternal);
        connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, &spriteResolver,
                &sprites::TSpriteResolver::handleGameAction);
    }
    // The sound helper the dialogs and the QML singletons play through.
    auto* soundHelper = new qmlcomponents::TSoundHelperService(this);
    qmlcomponents::TSoundHelperService::setInstance(soundHelper);
    connect(soundHelper, &qmlcomponents::TSoundHelperService::publishGameAction, m_pGameActionRouter->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    m_crashReportDialog = std::make_unique<gamewindow::TCrashReportDialogController>();
    connect(m_crashReportDialog.get(), &gamewindow::TCrashReportDialogController::requestSendCrashReport, this,
            &TGameClient::sendCrashReport);
    // CIP: the current session's disconnect reaches the client once, after its signal returned.
    m_gameSessionDisconnected.setAsyncDelay(1);
    m_gameSessionDisconnected.setOnlyFirstSignal(true);
    connect(&m_gameSessionDisconnected, &shared::TQtConnectionWrapper::signal, this, &TGameClient::onGameSessionDisconnected);
    initializeSound();
    m_gameHost = new GameHostImpl(this);
    connect(this, &TGameClient::mainWindowInitialized, m_gameHost, &runtime::TGameHost::mainWindowInitialized);
    connect(this, &TGameClient::worldLeft, m_gameHost, &runtime::TGameHost::worldLeft);
    connect(this, &TGameClient::characterSwitchRequested, m_gameHost, &runtime::TGameHost::characterSwitchRequested);
    connect(this, &TGameClient::characterSelectionClosed, m_gameHost, &runtime::TGameHost::characterSelectionClosed);
    connect(this, &TGameClient::characterSelectionRequested, m_gameHost, &runtime::TGameHost::characterSelectionRequested);

    shared::ownProductSurface(this);
    assets::ownProductSurface(this);
    authentication::ownProductSurface(this);
    client::ownProductSurface(this);
    common::ownProductSurface(this);
    container::ownProductSurface(this);
    creatures::ownProductSurface(this);
    dailyreward::ownProductSurface(this);
    exaltation::ownProductSurface(this);
    exivaoptions::ownProductSurface(this);
    game::ownProductSurface(this);
    gameWidnow::ownProductSurface(this);
    imbuing::ownProductSurface(this);
    network::ownProductSurface(this);
    prey::ownProductSurface(this);
    proficiency::ownProductSurface(this);
    protocol::ownProductSurface(this);
    questlog::ownProductSurface(this);
    screenshot::ownProductSurface(this);
    spells::ownProductSurface(this);
    teamfinder::ownProductSurface(this);
    trade::ownProductSurface(this);
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



    // CIP: . The frame itself is afterAnimating → tick → TNextFrameUpdater.
    m_frameLimitTimer.setTimerType(Qt::PreciseTimer);
    m_frameLimitTimer.setSingleShot(true);
    connect(&m_frameLimitTimer, &QTimer::timeout, this, &TGameClient::tick);
    m_idleTick.setSingleShot(true);
    connect(&m_idleTick, &QTimer::timeout, this, &TGameClient::tick);
}

void TGameClient::createAuthenticationProcessController(const QUrl& LoginUrl)
{
    // CIP: the character selection, the login request uploader, the email and
    // login confirmation code uploaders (each with its own sender) and the
    // Cloudflare challenge for the login service, handed to the controller.
    auto pCharacterSelection = std::make_shared<gamewindow::TCharacterSelectionController>();
    auto pLoginRequestUploader = std::make_shared<authentication::TLoginRequestUploader>();
    auto pEMailCodeRequestUploader = std::make_shared<authentication::TEmailCodeRequestUploader>(
        LoginUrl, std::make_shared<shared::TJsonDocumentHttpPostSender>());
    auto pLoginConfirmationCodeRequestUploader = std::make_shared<authentication::TLoginConfirmationCodeRequestUploader>(
        LoginUrl, std::make_shared<shared::TJsonDocumentHttpPostSender>());
    auto pCloudflareChallenge = std::make_shared<authentication::TCloudflareChallengeController>();
    pCloudflareChallenge->setTargetUrl(LoginUrl);
    m_authentication = std::make_shared<authentication::TAuthenticationProcessController>(
        pCharacterSelection, pLoginRequestUploader, pEMailCodeRequestUploader, pLoginConfirmationCodeRequestUploader,
        pCloudflareChallenge);
    {
        // The play session data the login machine forgets lives in the client's
        // account; the clearance goes with the next login request.
        authentication::TAuthenticationProcessController::THost host;
        host.resetSessionData = [this] { m_account.session = {}; };
        host.applyClearance = [uploader = pLoginRequestUploader.get()](
                                  const authentication::TCloudflareChallengeClearanceInfo& info) {
            uploader->applyClearance(info);
        };
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
    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginFailed, this, &TGameClient::fail);
    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginFailedDueToMissingTwoFactorToken, this, [this] {
        fail(tr("This account needs an authenticator token."));
    });
    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginFailedDueToInvalidServerResponse, this, [this] {
        fail(tr("The login server sent an invalid response."));
    });
    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginFailedClientOutOfDate, this, [this] {
        // CIP enters ShowClientOutOfDate (info / client_too_old + Restart) —
        // do not also fail with a dismissible error toast.
        Q_EMIT clientOutOfDate();
    });
    connect(pLoginRequestUploader.get(), &authentication::TLoginRequestUploader::loginFailedDueToCloudflareMitigation, this,
            [this, uploader = pLoginRequestUploader.get(), cloudflare = pCloudflareChallenge.get()] {
                QUrl challengeUrl = uploader->lastMitigatedUrl();
                if (!challengeUrl.isValid())
                    challengeUrl = m_options.env.loginUrl();
                cloudflare->startChallenge(challengeUrl);
            });
    // The clearance retries the login; a failed challenge ends it with its reason.
    connect(pCloudflareChallenge.get(), &authentication::TCloudflareChallengeController::challengeSolveSuccess, this,
            [this] { login(); });
    connect(pCloudflareChallenge.get(), &authentication::TCloudflareChallengeController::challengeSolveFailed, this,
            [this](authentication::TCloudflareChallengeError error) {
                switch (error) {
                case authentication::TCloudflareChallengeError::Timeout:
                    fail(qtTrId("cloudflare_challenge_error_timeout"));
                    break;
                case authentication::TCloudflareChallengeError::Cancelled:
                    fail(qtTrId("cloudflare_challenge_error_cancelled"));
                    break;
                case authentication::TCloudflareChallengeError::Unknown:
                default:
                    fail(qtTrId("cloudflare_challenge_error_unknown"));
                    break;
                }
            });

    connect(pCharacterSelection.get(), &gamewindow::TCharacterSelectionController::publishGameAction,
            m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(pCharacterSelection.get(), &gamewindow::TCharacterSelectionController::requestCharacterLogin, this,
            [this](const gamewindow::TCharacterLoginData& data) { onCharacterChosen(data.characterName); });
    connect(pCharacterSelection.get(), &gamewindow::TCharacterSelectionController::abort, this, [this] {
        m_authentication->trigger(authentication::TAuthenticationProcessController::ETrigger::DialogAborted);
        Q_EMIT characterSelectionClosed();
        // CIP: Cancel while online (alt switch) only closes the dialog. From the
        // login / post-logout character list, return to the login mask — never quit.
        if (tibia::game::isOnline())
            return;
        Q_EMIT returnToLoginRequested();
    });
    connect(this, &TGameClient::characterSelectionRequested, this, [this] {
        if (!tibia::game::isOnline())
            return;
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
    // The session's manager saves the character's configuration through
    // storages the game window owns: save before the game window goes.
    if (config::TCharacterConfigurationManager* manager = characterConfigurationManager())
        manager->saveConfiguration();
    // CIP: the dialogs made after the game window controller go before it.
    m_pCreateCharacterDialogController.reset();
    m_pCreateAccountDialogController.reset();
    m_gameWindow.reset();
    // The store balance helper follows the session and the engine's hooks.
    m_pStoreAndResourceBalanceHelperController.reset();
    shutdown();
    qmlcomponents::TSoundHelperService::setInstance(nullptr);
}

bool TGameClient::start(const QString& argv0, const QString& workDir, const Options& options)
{
    m_options = options;
    if (!gamecore::init(argv0, workDir)) {
        fail(tr("Could not start the game engine (no assets/catalog-content.json under %1).").arg(workDir));
        return false;
    }
    m_started = true;
    // The spell table reads spells.json from the data directories init set up.
    createSpellInfoStorage();

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

    gamecore::setClientVersion(ClientVersion);
    QString error;
    if (!gamecore::loadAssets(ClientVersion, &error)) {
        fail(error);
        return true;
    }
    // Custom effects / xBRZ register via product modpack (mods::bootstrapAll).
    Q_EMIT initialized();

    if (!m_options.replay.isEmpty()) {
        const QFileInfo file(m_options.replay);
        if (!file.isFile()) {
            fail(tr("No recorded session at %1.").arg(m_options.replay));
            return true;
        }
        // Prefer canonical path (follows symlinks). Basename-only is kept for
        // files that live under cwd `records/` (where --record writes).
        const QFileInfo resolved(file.canonicalFilePath().isEmpty() ? file.absoluteFilePath()
                                                                    : file.canonicalFilePath());
        QString replayArg = resolved.absoluteFilePath();
        if (resolved.absolutePath().endsWith(QLatin1String("/records")))
            replayArg = resolved.fileName();
        auto metaInfo = std::make_shared<game::TSessiondumpGameSessionMetaInfo>();
        setGameSessionSettings(*metaInfo);
        metaInfo->sessiondump.url = QUrl::fromLocalFile(replayArg);
        metaInfo->startedByGameAction = true;
        enqueueGameSessions({ metaInfo });
        return true;
    }

    login();
    return true;
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

void TGameClient::setMessageIgnoreList(chat::TWhiteBlacklist* list)
{
    m_messageIgnoreList = list;
    // A game window made after the session: its lists reach the current one.
    if (game::TGameSessionBase* session = activeSession())
        session->setWhiteBlacklist(list);
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
    gamecore::terminate();
    tibia::game::cleanMap();
    m_opcodes.reset();
    // The session goes now, not on the next event loop pass: the engine is down.
    g_releaseGameSessionsNow = true;
    setCurrentGameSession(nullptr);
    g_releaseGameSessionsNow = false;
}

void TGameClient::login()
{
    const auto& env = m_options.env;
    if (m_options.account.isEmpty()) {
        // CIP start-screen login mask (TGameWindowController::loginPressed) supplies credentials.
        return;
    }
    Q_EMIT loginStarted();
    authentication::TLoginRequestData request;
    request.email = m_options.account;
    request.password = m_options.password;
    request.stayLoggedIn = true;
    using EState = authentication::TAuthenticationProcessController::EState;
    if (!m_authentication->isRunning() || m_authentication->state() != EState::ShowLoginWaitingDialog)
        m_authentication->start();
    m_authentication->setLoginCredentials(request.email, request.password);
    loginRequestUploader()->login(env.loginUrl(), request, env.revision);
}

void TGameClient::abortPendingEnter()
{
    loginRequestUploader()->abort();
    if (!tibia::game::isOnline()) {
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
    authentication::LoginResult result = authentication::TLoginRequestUploader::parse(reply);
    if (result.characters.empty()) {
        login();
        return;
    }
    if (result.session.sessionKey.isEmpty())
        result.session = m_account.session;
    const QString autoCharacter = std::exchange(m_options.autoCharacter, {});
    onLoginSuccessful(result);
    m_options.autoCharacter = autoCharacter;
}

void TGameClient::onLoginSuccessful(const authentication::LoginResult& result)
{
    m_account = result;
    onFpsTrackingActiveChanged(result.session.fpsTracking);
    // CipSoft login has no session.email; remember-email is fed from the typed mask / CLI.
    if (!m_options.account.isEmpty())
        Q_EMIT sessionEmailAvailable(m_options.account);
    // CIP (TAuthenticationProcessController): the news learn the account's
    // returner state and unlock against its highest character level.
    if (news::TNewsStorage* news = news::TNewsStorage::current()) {
        news->setReturnerInformation(result.session.isReturner, result.session.showRewardNews);
        int highestLevel = 0;
        for (const auto& c : result.characters)
            highestLevel = std::max(highestLevel, c.level);
        news->raiseAccountLevel(static_cast<ushort>(highestLevel));
    }
    showAccount(result.characters);
    if (!m_options.autoCharacter.isEmpty()) {
        for (const auto& c : result.characters) {
            if (c.name.compare(m_options.autoCharacter, Qt::CaseInsensitive) == 0) {
                onCharacterChosen(c.name);
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
    if (!tibia::game::isOnline())
        return;
    if (m_characterSelectionOpen) {
        closeCharacterSelection();
        return;
    }
    openCharacterSelectionForSwitch();
}

void TGameClient::ensureLoginSurface()
{
    if (tibia::game::isOnline() || tibia::game::isSwitchingCharacter() || !m_options.autoCharacter.isEmpty())
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
    if (characterName.isEmpty() || !tibia::game::isOnline() || m_pendingCharacter)
        return false;
    // CIP: this character logs out; the chosen one logs in, in a new game
    // session, once it left the world.
    m_pendingCharacter = characterName;
    tibia::game::safeLogout();
    Q_EMIT characterSwitchRequested(characterName);
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
    // A character switch from a running session starts at the character selection.
    if (!m_authentication->isRunning())
        m_authentication->start(true);
    m_authentication->trigger(authentication::TAuthenticationProcessController::ETrigger::CharacterLoginRequested);
    if (tibia::game::isOnline()) {
        Q_EMIT characterSelectionClosed();
        // CIP: log this character out; the chosen one logs in once it left the world.
        m_pendingCharacter = characterName;
        tibia::game::safeLogout();
        return;
    }

    const authentication::TCharacter* chosen = nullptr;
    for (const auto& c : m_account.characters) {
        if (c.name == characterName) {
            chosen = &c;
            break;
        }
    }
    const auto* world = chosen ? m_account.world(chosen->worldId) : nullptr;
    if (!world) {
        fail(tr("The world of %1 is not in the world list.").arg(characterName));
        return;
    }
    Q_EMIT characterSelectionClosed();
    m_worldType = world->pvpType;

    game::GameserverCredentials credentials;
    credentials.account = m_options.account;
    credentials.password = m_options.password;
    credentials.sessionKey = m_account.session.sessionKey;
    credentials.worldName = world->name;
    credentials.host = world->host();
    credentials.port = world->port();
    credentials.characterName = chosen->name;
    if (!m_options.recordTo.isEmpty())
        credentials.recordTo = QFileInfo(m_options.recordTo).absoluteFilePath();

    // CIP: a new login does not open the character selection on its own disconnect.
    if (gamewindow::TGameSessionDisconnectReactionController* reaction = gameSessionDisconnectReactionController())
        reaction->resetShowCharacterSelectionOnDisconnect();
    enqueueGameSessions(createGameSessionMetaInfos(credentials, chosen->tutorial ? game::resolveTutorialSessiondumpPath() : QString()));
}

void TGameClient::loginAgainWithLastCharacter()
{
    const QString characterName = characterSelection()->lastSelectedCharacterName();
    if (!characterName.isEmpty())
        onCharacterChosen(characterName);
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
    const game::GameserverCredentials& loginData, const QString& tutorialSessiondump) const
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
    gameserver->loginData = loginData;
    if (!tutorialSessiondump.isEmpty()) {
        auto tutorial = std::make_shared<game::TTutorialSessiondumpGameSessionMetaInfo>();
        tutorial->sessiondump.url = QUrl::fromLocalFile(tutorialSessiondump);
        setGameSessionSettings(*tutorial);
        tutorial->characterName = loginData.characterName;
        metaInfos.push_back(std::move(tutorial));
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
        // CIP: the tutorial's dump runs in its own session class.
        std::shared_ptr<game::TSessiondumpGameSession> dump;
        if (type == EType::TutorialSessiondump)
            dump = createGameSession<game::TTutorialSessiondumpGameSession>();
        else
            dump = createGameSession<game::TSessiondumpGameSession>();
        dump->setCharacterConfigurationStorages(m_characterConfigurationStorages);
        dump->initialize(metaInfo);
        setCurrentGameSession(dump);
        dump->startSession();
        return;
    }
    if (type != EType::Gameserver)
        return;
    setCurrentGameSession(nullptr);
    const auto live = createGameSession<game::TGameserverGameSession>();
    live->setCharacterConfigurationStorages(m_characterConfigurationStorages);
    live->initialize(metaInfo);
    setCurrentGameSession(live);
    live->connectToGameserver();
    live->startSession();
}

void TGameClient::connectToCurrentGameSession()
{
    game::IGameSession* current = m_currentGameSession.get();
    connect(current, &game::IGameSession::worldEntered, this, &TGameClient::onWorldEntered);
    connect(current, &game::IGameSession::showCharacterSelectionDialog, this, [this] { openCharacterSelectionForSwitch(); });
    if (auto* base = dynamic_cast<game::TGameSessionBase*>(current))
        connect(base->genericGameActionHandler().get(), &input::TGenericGameActionHandler::sendQuitGame, this,
                [this] { requestLogoutOrShowStartScreen(nullptr); });
    const auto outOfDate = [this] {
        Q_EMIT clientOutOfDate();
        fail(tr("The game server needs a newer client."));
    };
    if (auto* live = dynamic_cast<game::TGameserverGameSession*>(current))
        connect(live, &game::TGameserverGameSession::clientOutOfDate, this, outOfDate);
    else if (auto* dump = dynamic_cast<game::TSessiondumpGameSession*>(current))
        connect(dump, &game::TSessiondumpGameSession::clientOutOfDate, this, outOfDate);
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
        // A retired session no longer follows the game's notifications.
        disconnect(game::TGameNotify::instance(), nullptr, m_currentGameSession.get(), nullptr);
        // The session's minimap feed.
        if (m_minimap)
            m_minimap->setProtocolHandler(nullptr);
        if (previous)
            previous->setWhiteBlacklist(nullptr);
        // Host shim: the classes' instance() stop resolving to this session.
        setSessionInstances(nullptr);
        // Pack-facing accessors stop resolving to the retired session.
        game::setCurrentGameSession(nullptr);
        m_currentGameSession.reset();
    }
    // Host shim: host and pack code still read these classes' instance(); they
    // are the new session's members before its controllers connect.
    if (auto* next = dynamic_cast<game::TGameSessionBase*>(GameSession.get())) {
        setSessionInstances(next);
        next->initializeWorldMapSessionState();
    }
    connectGameWindowControllersToGameSession(GameSession);
    // The login uploader's character list follows the session's player.
    if (authentication::TLoginRequestUploader* uploader = loginRequestUploader())
        uploader->setGameSession(std::dynamic_pointer_cast<game::TGameSessionBase>(GameSession));
    m_currentGameSession = GameSession;
    // Host: code no session is handed to (pack ABI peels, packs) resolves it here.
    game::setCurrentGameSession(m_currentGameSession);
    if (!m_currentGameSession)
        return;
    if (auto* current = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get())) {
        linkGameActionRouters(*m_pGameActionRouter, *current);
        current->setWhiteBlacklist(m_messageIgnoreList);
    }
    if (m_minimap)
        m_minimap->setProtocolHandler(worldmapProtocol());
    connectToCurrentGameSession();
    m_gameSessionDisconnected.connectTo(m_currentGameSession.get(), &game::IGameSession::disconnected);
    m_gameSessionDisconnected.connectTo(m_currentGameSession.get(), &game::IGameSession::connectionError);
    m_gameSessionDisconnected.resetEmitCount();
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
    if (config::TCharacterConfigurationManager* manager = characterConfigurationManager();
        manager && tibia::game::localPlayerId() != 0)
        manager->onPlayerCreatureLoaded(tibia::game::localPlayerId());
    const std::string assets = gamecore::assetsRoot(ClientVersion);
    // CipSoft: Resources/minimap is a sibling of Resources/assets (assetsRoot adds
    // Resources as a search path so short name `minimap/` resolves). Prefer that
    // over nested assets/minimap. Prefer getRealPath over getRealDir+concat so a
    // "/" search root is never re-joined into "//…" (wasm MEMFS rejects that).
    std::string defaultMinimapPath = tibia::files::data().getRealPath("minimap");
    if (defaultMinimapPath.empty())
        defaultMinimapPath = tibia::files::data().getRealPath(assets + "minimap");
    m_minimap = std::make_unique<minimap::TMinimapManager>(
        defaultMinimapPath.empty() ? QString() : QString::fromStdString(defaultMinimapPath));
    // The map handler keeps the minimap's floor and fields current.
    m_minimap->setProtocolHandler(worldmapProtocol());
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal,
            m_minimap->markerStorage(), qOverload<const input::IGameActionPtr&>(&minimap::TMinimapMarkerStorage::handleGameAction));
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal,
            m_minimap->tileManager(), &minimap::TMinimapTileManager::handleGameAction);
    // CIP click-to-walk / minimap GoPath uses Indexed8 waypoint costs (qtclient
    // TWaypointCosts), not OTC WasSeen tile blocks — otherwise far clicks stop mid-path.
    tibia::game::setWaypointCostFactory([this]() -> std::shared_ptr<minimap::TWaypointCosts> {
        return minimap::snapshotWaypointCosts(m_minimap ? m_minimap->tileStorage() : nullptr);
    });
    Q_EMIT mainWindowInitialized();

    for (qsizetype i = 0; i < m_options.say.size(); ++i) {
        QTimer::singleShot(1000 * (i + 1), this, [text = m_options.say[i].toStdString()] {
            if (tibia::game::isOnline())
                tibia::game::talk(text);
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
            if (!tibia::game::isOnline() || tibia::game::localPlayerIsAutoWalking() || tibia::game::localPlayerIsPreWalking()
                || tibia::game::localPlayerIsWalking())
                return;
            const QString token = m_options.walk[*next].trimmed().toLower();
            ++*next;
            if (token.contains(QLatin1Char(':'))) {
                // dx:dy walks to a field relative to the character.
                const QStringList offset = token.split(QLatin1Char(':'));
                const Position at = tibia::game::localPlayerPosition();
                tibia::game::localPlayerAutoWalk(Position(at.x + offset.value(0).toInt(), at.y + offset.value(1).toInt(), at.z));
                return;
            }
            const auto direction = directions.value(token, tibia::client::InvalidDirection);
            if (direction != tibia::client::InvalidDirection)
                tibia::game::walk(direction);
        });
        QTimer::singleShot(2000, walker, [walker] { walker->start(5); });
    }

    if (m_options.playForSeconds > 0) {
        QTimer::singleShot(m_options.playForSeconds * 1000, this, [this] {
            const bool online = tibia::game::isOnline();
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

game::TStatusbarStorage* TGameClient::statusbarStorage() const
{
    config::TCharacterConfigurationManager* manager = characterConfigurationManager();
    return manager ? manager->statusbarStorage() : nullptr;
}

config::TActionBarOptions* TGameClient::actionBarOptions() const
{
    config::TCharacterConfigurationManager* manager = characterConfigurationManager();
    return manager ? manager->actionBarOptions() : nullptr;
}

void TGameClient::onWorldLeft()
{
    tibia::game::clearWaypointCostFactory();
    m_minimap.reset();
    if (config::TCharacterConfigurationManager* manager = characterConfigurationManager())
        manager->saveConfiguration();
    if (chat::TTextStorage* texts = textStorage())
        texts->clear();
    if (m_pSoundController)
        m_pSoundController->handleSoundTrigger(true);
    Q_EMIT worldLeft();
    // CIP: after logout return to character selection while the account session remains.
    ensureLoginSurface();
    if (m_pendingCharacter) {
        const QString next = *m_pendingCharacter;
        m_pendingCharacter.reset();
        QTimer::singleShot(0, this, [this, next] { onCharacterChosen(next); });
    }
}

void TGameClient::setWindow(QQuickWindow* window)
{

    // CIP: .
    connect(window, &QQuickWindow::afterAnimating, this, &TGameClient::tick);
    connect(window, &QQuickWindow::frameSwapped, this, [ring = m_frameDurations.get()] { ring->finish(); }, Qt::DirectConnection);
    // CIP onRenderThreadFrameSwapped: every swapped frame starts a frame of the FPS counter.
    connect(window, &QQuickWindow::frameSwapped, this, [this] {
        if (m_fpsHistory)
            m_fpsHistory->fpsCounter()->startNewFrame();
    }, Qt::DirectConnection);
}

void TGameClient::initializeSound()
{
    ::tibia::client::TGameClientBase::initializeSound();
    if (!m_pSoundController)
        return;
    // CipSoft connects the sound metadata with the sound manager while it
    // sets up the assets, and the sound controller with the router once the
    // main window exists; qtclient's assets and main window do not run those
    // steps, so the connections are made with the sound system.
    connect(m_pSoundMetadataManager.get(), &sound::TSoundMetadataManager::metadataChanged, m_pSoundManager.get(),
            &sound::TSoundManager::onSoundMetadataChanged);
    sound::TSoundController::setInstance(m_pSoundController.get());
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, m_pSoundController.get(),
            &sound::TSoundController::handleGameAction);
    // Out of the game the title music plays once the sound bank is known.
    connect(m_pSoundMetadataManager.get(), &sound::TSoundMetadataManager::metadataChanged, this, [this] {
        if (!tibia::game::isOnline())
            m_pSoundController->handleSoundTrigger(true);
    });
}

void TGameClient::onWindowSizeChanged()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCloseAllMultiActionButtons>(0, 0));
}

void TGameClient::tick()
{
    // notifyCameraMove / QML during tween advance must not re-enter NFU: a nested
    // tick would resumeRealTime while the outer advance still holds the predicted
    // clock, collapsing DurationTween measurements and desyncing feet.
    if (m_inTick)
        return;
    m_inTick = true;

    gamecore::poll();
    if (m_pSoundController) {
        // CIP TGameClientBase::v27 (before each frame): the controllers and the cache.
        const auto now = static_cast<int64_t>(shared::TClockService::frameMillis());
        if (game::TGameSessionBase* session = game::currentGameSession()) {
            if (session->objectSoundController())
                session->objectSoundController()->updateObjectCounts(
                    tibia::game::hasLocalPlayer() ? tibia::game::localPlayerPosition() : Position());
        }
        m_pSoundController->update(now);
        m_pSoundManager->updateWithTimestamp(now);
    }
    m_idleTick.start(tibia::game::isOnline() ? 500 : 100);
    if (m_frameRateLimitEnabled && !frameSlotReached(gamecore::nowMicros())) {
        m_inTick = false;
        return;
    }

    // Shown one tick-to-swap time from now, on average over the last frames.
    const qint64 offsetMicros = m_frameDurations->averageNs() / 1000;
    m_frameDurations->start();
    m_predictor->setOffset(offsetMicros);
    beforeRenderFrameGameWindow();
    if (m_started)
        if (auto* session = dynamic_cast<game::TGameSessionBase*>(m_currentGameSession.get()))
            session->onPrepareNextFrameToRender(m_predictor->predict());
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
    now = gamecore::nowMicros();
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
    if (!tibia::game::isOnline())
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
        m_fpsSampleTimer.stop();
        m_fpsUploadTimer.stop();
        return;
    }
    auto clockService = shared::TClockService::process();
    m_fpsHistory = std::make_shared<systemsurvey::TFPSHistory>(
        std::make_shared<systemsurvey::TFPSCounter>(clockService), clockService);
    // Host: CipSoft posts to [URLS] fpsHistoryRecipient; qtclient posts to the
    // qtc-aac hardware receiver.
    auto uploader = std::make_shared<systemsurvey::TFPSHistoryUploader>(
        app::LauncherEnv::load(QDir::currentPath()).fpsHistoryRecipientUrl(),
        std::make_shared<systemsurvey::TFPSHistoryToJsonConverter>(),
        std::make_shared<shared::TJsonDocumentHttpPostSender>());
    const int renderer = app::rendererIndexForGraphicsApi(QQuickWindow::graphicsApi());
    m_fpsHistoryController = std::make_shared<systemsurvey::TFPSHistoryController>(uploader, m_fpsHistory, renderer);
    const QString group = QStringLiteral("FPSHISTORY");
    startFpsHistoryTimers(config::clientSettingAtLeast(group, QStringLiteral("sampleRateMsec"), 1000, 100),
                          config::clientSettingAtLeast(group, QStringLiteral("uploadRateSec"), 60, 1));
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
        connect(this, &TGameClient::worldLeft, controller, &systemsurvey::TFPSHistoryController::uploadHistoryAndClearIt);
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

}

