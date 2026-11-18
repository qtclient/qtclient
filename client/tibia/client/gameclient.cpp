#include "gameclient.h"
#include "game/gameactionrouter.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/soundhelper.h"
#include <stdexcept>

#include "config/actionbaroptions.h"
#include "config/characterconfigurationmanager.h"
#include "colorhelper.h"
#include "creatures/movementtweens.h"
#include "teamfinder/teamfinderciptypes.h"
#include "friends/friendsciptypes.h"
#include "taskboard/taskboardciptypes.h"
#include "spells/spellsciptypes.h"
#include "skillwheel/skillwheelciptypes.h"
#include "screenshot/screenshotciptypes.h"
#include "reporting/reportingciptypes.h"
#include "quickloot/quicklootciptypes.h"
#include "questlog/questlogciptypes.h"
#include "protocol/protocolciptypes.h"
#include "proficiency/proficiencyciptypes.h"
#include "prey/preyciptypes.h"
#include "news/newsstorage.h"

#include <algorithm>
#include "network/networkciptypes.h"
#include "market/marketciptypes.h"
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
#include "shared/tibiafilesystemhelper.h"
#include "config/configciptypes.h"
#include "bossbestiary/bossbestiaryciptypes.h"
#include "authentication/authenticationciptypes.h"
#include "authentication/cloudflarechallengecontroller.h"
#include "assets/assetsciptypes.h"
#include "shared/sharedciptypes.h"
#include "game/gameciptypes.h"
#include "creatures/vipstorage.h"
#include "trade/tradeciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "store/storestorage.h"
#include "tutorial/tutorialciptypes.h"

#include "app/bootstrap.h"
#include "app/clientoptions.h"
#include "chat/chatprotocolmessagehandler.h"
#include "chat/gamemessagehandler.h"
#include "minimap/minimapmarkerstorage.h"
#include "chat/textstorage.h"
#include "game/gamesessionbase.h"
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
#include "game/statusbarstorage.h"
#include "input/gameactionhandlers.h"
#include "minimap/minimapmanager.h"
#include "minimap/waypointcosts.h"
#include "network/extendedopcoderouter.h"
#include "network/gameserverdualconnection.h"
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
#include "renderer/nextframeupdater.h"
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
#include "creatures/walkdebug.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

#include <client/outfit.h>
#include "network/gameserverpacketprocessor.h"
#include "protocol/clientmessages.h"
#include "shared/tibiafilesystemhelper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointer>
#include <QQuickWindow>
#include <QThread>

#include <utility>
#include <tibia/game/gamesession.h>

Q_LOGGING_CATEGORY(lcGameClient, "tibia.client")
Q_LOGGING_CATEGORY(lcFrameCost, "tibia.frame", QtWarningMsg)

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

    void setSameSocketCharacterSwitch(const bool enabled) override
    {
        m_client->setSameSocketCharacterSwitch(enabled);
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

TGameClient::TGameClient(QObject* parent)
    : cip::tibia::client::TGameClient(parent)
    , m_login(std::make_unique<authentication::TLoginRequestUploader>())
    , m_characterSelection(std::make_unique<gamewindow::TCharacterSelectionController>())
    , m_worldMapRenderProvider(std::make_unique<qmlcomponents::TWorldMapRenderProvider>())
    , m_sidebarOptions(std::make_unique<config::TSidebarOptions>())
    , m_statusbarStorage(std::make_unique<game::TStatusbarStorage>())
    , m_actionBarOptions(std::make_unique<config::TActionBarOptions>())
    , m_outfitDialogStorage(std::make_unique<game::TOutfitDialogStorage>())
    , m_frameDurations(std::make_unique<worldmap::TFrameDurationRing>())
    , m_predictor(std::make_unique<worldmap::TNextFrameTimestampPredictor>())
    , m_sessiondumpCatalogLoader(std::make_shared<shared::TDownloadPool>(
          1, std::make_shared<shared::QNetworkAccessManagerWrapper>(std::make_shared<QNetworkAccessManager>())))
{
    game::TOutfitDialogStorage::setInstance(m_outfitDialogStorage.get());
    m_pGameActionRouter = std::make_shared<game::TGameActionRouter>();
    // The actions whose CIP handler classes are not ported yet.
    auto* unported = new input::TUnportedGameActionHandler(this);
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, unported,
            &input::TUnportedGameActionHandler::handleGameAction);
    connect(unported, &input::TUnportedGameActionHandler::publishGameAction, m_pGameActionRouter->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    // The sound helper the dialogs and the QML singletons play through.
    auto* soundHelper = new qmlcomponents::TSoundHelperService(this);
    qmlcomponents::TSoundHelperService::setInstance(soundHelper);
    connect(soundHelper, &qmlcomponents::TSoundHelperService::publishGameAction, m_pGameActionRouter->internalRouter(),
            &game::TInternalGameActionRouter::handleGameActionInternal);
    // The active session's game message handler (status line, map messages).
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, this,
            [this](const input::IGameActionPtr& action) {
                if (m_messageHandler)
                    m_messageHandler->handleGameAction(action);
            });
    m_crashReportDialog = std::make_unique<gamewindow::TCrashReportDialogController>();
    connect(m_crashReportDialog.get(), &gamewindow::TCrashReportDialogController::requestSendCrashReport, this,
            &TGameClient::sendCrashReport);
    // Each game session brings its own storages, view and handlers.
    connect(game::TGameNotify::instance(), &game::TGameNotify::gameSessionChanged, this, &TGameClient::bindActiveSession);
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
    bossbestiary::ownProductSurface(this);
    client::ownProductSurface(this);
    common::ownProductSurface(this);
    container::ownProductSurface(this);
    creatures::ownProductSurface(this);
    dailyreward::ownProductSurface(this);
    exaltation::ownProductSurface(this);
    exivaoptions::ownProductSurface(this);
    game::ownProductSurface(this);
    gameWidnow::ownProductSurface(this);
    highscores::ownProductSurface(this);
    imbuing::ownProductSurface(this);
    market::ownProductSurface(this);
    network::ownProductSurface(this);
    prey::ownProductSurface(this);
    proficiency::ownProductSurface(this);
    protocol::ownProductSurface(this);
    tutorial::ownProductSurface(this);
    questlog::ownProductSurface(this);
    quickloot::ownProductSurface(this);
    reporting::ownProductSurface(this);
    screenshot::ownProductSurface(this);
    skillwheel::ownProductSurface(this);
    spells::ownProductSurface(this);
    taskboard::ownProductSurface(this);
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
        publishToRouter(creatures::TVipStorage::instance());
        publishChildrenToRouter(static_cast<game::TPlayerProtocolMessageHandler*>(nullptr));
        publishChildrenToRouter(static_cast<quickloot::TQuickLootGameActionHandler*>(nullptr));
        // Their game actions.
        for (auto* handler : findChildren<quickloot::TQuickLootGameActionHandler*>(QString(), Qt::FindDirectChildrenOnly))
            connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, handler,
                    qOverload<const input::IGameActionPtr&>(&quickloot::TQuickLootGameActionHandler::handleGameAction));
        for (auto* handler : creatures::TVipStorage::instance()->findChildren<creatures::TVipGameActionHandler*>())
            connect(router, &game::TInternalGameActionRouter::publishGameActionInternal, handler,
                    qOverload<const input::IGameActionPtr&>(&creatures::TVipGameActionHandler::handleGameAction));
        publishChildrenToRouter(static_cast<skillwheel::TSkillWheelProtocolMessageHandler*>(nullptr));
    }



    // CIP: . The frame itself is afterAnimating → tick → TNextFrameUpdater.
    m_frameLimitTimer.setTimerType(Qt::PreciseTimer);
    m_frameLimitTimer.setSingleShot(true);
    connect(&m_frameLimitTimer, &QTimer::timeout, this, &TGameClient::tick);
    m_idleTick.setSingleShot(true);
    connect(&m_idleTick, &QTimer::timeout, this, &TGameClient::tick);

    m_authentication = std::make_unique<authentication::TAuthenticationProcessController>();
    m_authentication->bindLoginUploader(m_login.get());
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginSuccessful, this, &TGameClient::onLoginSuccessful);
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailed, this, &TGameClient::fail);
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailedDueToMissingTwoFactorToken, this, [this] {
        fail(tr("This account needs an authenticator token."));
    });
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailedDueToInvalidServerResponse, this, [this] {
        fail(tr("The login server sent an invalid response."));
    });
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailedClientOutOfDate, this, [this] {
        // CIP enters ShowClientOutOfDate (info / client_too_old + Restart) —
        // do not also fail with a dismissible error toast.
        Q_EMIT clientOutOfDate();
    });
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailedDueToCloudflareMitigation, this, [this] {
        if (!m_cloudflareChallenge) {
            m_cloudflareChallenge = std::make_unique<authentication::TCloudflareChallengeController>(this);
            connect(m_cloudflareChallenge.get(), &authentication::TCloudflareChallengeController::challengeSolveSuccess, this,
                    [this](const authentication::TCloudflareChallengeClearanceInfo& info) {
                        m_login->applyClearance(info);
                        m_authentication->onCloudflareSolverSuccess(info);
                        login();
                    });
            connect(m_cloudflareChallenge.get(), &authentication::TCloudflareChallengeController::challengeSolveFailed, this,
                    [this](authentication::TCloudflareChallengeError error) {
                        m_authentication->trigger(authentication::TAuthenticationProcessController::ETrigger::CloudflareSolveFailed);
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
        }
        QUrl challengeUrl = m_login->lastMitigatedUrl();
        if (!challengeUrl.isValid())
            challengeUrl = m_options.env.loginUrl();
        m_cloudflareChallenge->startChallenge(challengeUrl);
    });

    connect(m_characterSelection.get(), &gamewindow::TCharacterSelectionController::publishGameAction,
            m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::handleGameActionInternal);
    connect(m_characterSelection.get(), &gamewindow::TCharacterSelectionController::requestCharacterLogin, this,
            [this](const gamewindow::TCharacterLoginData& data) { onCharacterChosen(data.characterName); });
    connect(m_characterSelection.get(), &gamewindow::TCharacterSelectionController::abort, this, [this] {
        m_authentication->trigger(authentication::TAuthenticationProcessController::ETrigger::DialogAborted);
        Q_EMIT characterSelectionClosed();
        // CIP: Cancel while online (alt switch) only closes the dialog. From the
        // login / post-logout character list, return to the login mask — never quit.
        if (tibia::game::isOnline())
            return;
        Q_EMIT returnToLoginRequested();
    });
    connect(this, &TGameClient::characterSelectionRequested, this, [this] {
        if (tibia::game::isOnline())
            m_characterSelectionOpen = true;
    });
    connect(this, &TGameClient::characterSelectionClosed, this, [this] {
        m_characterSelectionOpen = false;
    });
    // CIP CHARACER_SELECTION_AUTO_CLOSE: hide the list without treating it as Cancel.
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, this,
            [this](const input::IGameActionPtr& action) {
                if (action && action->gameActionType() == input::EGameAction::CHARACER_SELECTION_AUTO_CLOSE)
                    Q_EMIT characterSelectionClosed();
            });
}

TGameClient::~TGameClient()
{
    // The manager saves the character's configuration on destruction, through
    // storages the game window owns: it goes first.
    m_characterConfigurationManager.reset();
    m_gameWindow.reset();
    shutdown();
    qmlcomponents::TSoundHelperService::setInstance(nullptr);
}

std::shared_ptr<game::TGameActionRouter> TGameClient::gameActionRouter() const
{
    if (!m_pGameActionRouter)
        throw std::logic_error("m_pGameActionRouter was not initialized yet");
    return m_pGameActionRouter;
}

bool TGameClient::start(const QString& argv0, const QString& workDir, const Options& options)
{
    m_options = options;
    if (!gamecore::init(argv0, workDir)) {
        fail(tr("Could not start the game engine (no assets/catalog-content.json under %1).").arg(workDir));
        return false;
    }
    m_started = true;

    // Engine objects exist only after init.
    m_opcodes = std::make_unique<network::ExtendedOpcodeRouter>();
    m_dualConnection = std::make_unique<network::TGameserverDualConnection>();
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
        m_sessiondump = std::make_unique<game::TSessiondumpGameSession>();
        linkGameActionRouters(*m_pGameActionRouter, *m_sessiondump);
        m_sessiondump->activate();
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::connected, this, [] { qCInfo(lcGameClient) << "session dump connected"; });
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::gameLoginSuccessful, this, [] { qCInfo(lcGameClient) << "session dump login accepted"; });
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::worldEntered, this, &TGameClient::onWorldEntered);
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::gameLoginErrorMessage, this, &TGameClient::fail);
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::connectionError, this, &TGameClient::fail);
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::clientOutOfDate, this, [this] {
            Q_EMIT clientOutOfDate();
            fail(tr("The game server needs a newer client."));
        });
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::gameLoginAdviceMessage, this, [](const QString& message) {
            qCInfo(lcGameClient).noquote() << "login advice:" << message;
        });
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::gameLoginWaitMessage, this, [](const QString& message, int seconds) {
            qCInfo(lcGameClient).noquote() << "login queue:" << message << seconds << "s";
        });
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::disconnected, this, [this] {
            qCInfo(lcGameClient) << "left the game (session dump)";
            onWorldLeft();
        });
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::sessionEndInformation, this, [](int reason) {
            qCInfo(lcGameClient) << "session ended, reason" << reason;
        });
        connect(m_sessiondump.get(), &game::TSessiondumpGameSession::sessionDumpMarker, this, [](const QString& marker) {
            qCInfo(lcGameClient).noquote() << "session dump marker:" << marker;
        });
        // Prefer canonical path (follows symlinks). Basename-only is kept for
        // files that live under cwd `records/` (where --record writes).
        const QFileInfo resolved(file.canonicalFilePath().isEmpty() ? file.absoluteFilePath()
                                                                    : file.canonicalFilePath());
        QString replayArg = resolved.absoluteFilePath();
        if (resolved.absolutePath().endsWith(QLatin1String("/records")))
            replayArg = resolved.fileName();
        qCInfo(lcGameClient).noquote() << "replaying" << resolved.absoluteFilePath()
                                       << "as" << replayArg;
        m_sessiondump->replay(replayArg);
        return true;
    }

    m_session = std::make_unique<game::TGameserverGameSession>();

    linkGameActionRouters(*m_pGameActionRouter, *m_session);
    game::TGameserverGameSession::setLiveInstance(m_session.get());
    m_session->activate();
    connect(m_session.get(), &game::TGameserverGameSession::connected, this, [] { qCInfo(lcGameClient) << "game server connected"; });
    connect(m_session.get(), &game::TGameserverGameSession::gameLoginSuccessful, this, [] { qCInfo(lcGameClient) << "game login accepted"; });
    connect(m_session.get(), &game::TGameserverGameSession::worldEntered, this, &TGameClient::onWorldEntered);
    connect(m_session.get(), &game::TGameserverGameSession::gameLoginErrorMessage, this, &TGameClient::fail);
    connect(m_session.get(), &game::TGameserverGameSession::connectionError, this, &TGameClient::fail);
    connect(m_session.get(), &game::TGameserverGameSession::clientOutOfDate, this, [this] {
        Q_EMIT clientOutOfDate();
        fail(tr("The game server needs a newer client."));
    });
    connect(m_session.get(), &game::TGameserverGameSession::gameLoginAdviceMessage, this, [](const QString& message) {
        qCInfo(lcGameClient).noquote() << "login advice:" << message;
    });
    connect(m_session.get(), &game::TGameserverGameSession::gameLoginWaitMessage, this, [](const QString& message, int seconds) {
        qCInfo(lcGameClient).noquote() << "login queue:" << message << seconds << "s";
    });
    connect(m_session.get(), &game::TGameserverGameSession::disconnected, this, [this] {
        qCInfo(lcGameClient) << "left the game";
        onWorldLeft();
    });
    connect(m_session.get(), &game::TGameserverGameSession::sessionEndInformation, this, [](int reason) {
        qCInfo(lcGameClient) << "session ended, reason" << reason;
    });
    login();
    return true;
}

namespace {

game::TGameSessionBase* activeSession()
{
    return game::TGameSessionBase::active();
}

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

void TGameClient::bindActiveSession()
{
    // The updater and the message handler work on one session's texts and effects.
    m_updater.reset();
    m_messageHandler.reset();
    game::TGameSessionBase* session = activeSession();
    m_worldMapRenderProvider->setViewport(session ? session->worldMapViewport().get() : nullptr);
    if (m_soundController) {
        disconnect(m_objectSoundConnection);
        m_soundController->setGameSessionSoundProvider(session ? session->gameSessionSoundProvider() : nullptr);
        if (session && session->objectSoundController())
            m_objectSoundConnection = connect(session->objectSoundController().get(),
                                              &sound::TObjectSoundController::ambienceObjectStreamCountChanged,
                                              m_soundController.get(),
                                              &sound::TSoundController::onAmbienceObjectStreamCountChanged);
    }
    if (m_chatProtocol) {
        m_chatProtocol->setTextStorage(session ? session->textStorage().get() : nullptr);
        m_chatProtocol->setEffectStorage(session ? session->effectStorage().get() : nullptr);
    }
    if (m_minimap)
        m_minimap->setProtocolHandler(worldmapProtocol());
    if (!session)
        return;
    m_messageHandler = std::make_unique<chat::TGameMessageHandler>(*session->textStorage());
    m_messageHandler->setOptions(app::ClientOptions::load().options());
    m_updater = std::make_unique<renderer::TNextFrameUpdater>(*m_worldMapRenderProvider, *session->textStorage());
}

void TGameClient::setChatProtocolMessageHandler(chat::TChatProtocolMessageHandler* handler)
{
    m_chatProtocol = handler;
    if (m_chatProtocol) {
        m_chatProtocol->setTextStorage(textStorage());
        game::TGameSessionBase* session = activeSession();
        m_chatProtocol->setEffectStorage(session ? session->effectStorage().get() : nullptr);
    }
}

void TGameClient::setMessageIgnoreList(chat::TWhiteBlacklist* list)
{
    m_messageIgnoreList = list;
    if (auto* teamFinder = teamfinder::TTeamFinderProtocolMessageHandler::instance())
        teamFinder->setIgnoreList(list);
}

void TGameClient::shutdown()
{
    if (!m_started)
        return;
    m_started = false;
    m_idleTick.stop();
    m_frameLimitTimer.stop();
    onWorldLeft();
    // Force logout / Map::clean while TWorldMapStorage still owns the field
    // QObjects. Map tileBlocks hold raw pointers into that tree; destroying
    // storage first leaves terminate → removeUnawareThings dangling (SIGSEGV
    // on quit Exit).
    gamecore::terminate();
    tibia::game::cleanMap();
    m_updater.reset();
    m_messageHandler.reset();
    m_opcodes.reset();
    m_dualConnection.reset();
    m_sessiondump.reset();
    game::TGameserverGameSession::setLiveInstance(nullptr);
    m_session.reset();
}

void TGameClient::login()
{
    const auto& env = m_options.env;
    if (m_options.account.isEmpty()) {
        // CIP start-screen login mask (TGameWindowController::loginPressed) supplies credentials.
        qCInfo(lcGameClient) << "waiting for account login (CIP login mask)";
        return;
    }
    qCInfo(lcGameClient).noquote() << "logging in" << m_options.account << "at" << env.loginUrl().toString();
    Q_EMIT loginStarted();
    authentication::TLoginRequestData request;
    request.email = m_options.account;
    request.password = m_options.password;
    request.stayLoggedIn = true;
    using EState = authentication::TAuthenticationProcessController::EState;
    if (!m_authentication->isRunning() || m_authentication->state() != EState::ShowLoginWaitingDialog)
        m_authentication->start();
    m_login->login(env.loginUrl(), request, env.revision);
}

void TGameClient::abortPendingEnter()
{
    m_suppressConnectError = true;
    m_login->abort();
    if (!tibia::game::isOnline()) {
        if (m_session)
            m_session->disconnectNow();
        else if (m_sessiondump)
            m_sessiondump->disconnectNow();
    }
    QTimer::singleShot(0, this, [this] { m_suppressConnectError = false; });
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
    qCInfo(lcGameClient) << "account has" << result.characters.size() << "characters on" << result.worlds.size() << "worlds";
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
    if (characterName.isEmpty() || !tibia::game::isOnline())
        return false;
    // Same character is allowed: CIP would logout+login; IdleOT re-enters on the same socket.
    if (tibia::game::isSwitchingCharacter())
        return false;
    if (!tibia::network::TGameserverNetworkPacketProcessor::live())
        return false;
    tibia::protocol::gameclient::sendCharacterSwitch(characterName.toStdString());
    Q_EMIT characterSwitchRequested(characterName);
    return true;
}

void TGameClient::showAccount(const authentication::TCharacterList& characters)
{
    const auto [premiumType, premiumDays] = premiumTypeAndDays(m_account.session);
    m_characterSelection->setAccountState(static_cast<int>(m_account.session.status));
    m_characterSelection->setPremium(premiumType, premiumDays);
    m_characterSelection->setRecoverySetupComplete(m_account.session.recoverySetupComplete);
    m_characterSelection->setCharacters(characters, m_account.worlds);
    // Until a character logged in, the table preselects the main character.
    if (m_characterSelection->lastSelectedCharacterName().isEmpty()) {
        for (const authentication::TCharacter& c : characters) {
            if (c.isMainCharacter)
                m_characterSelection->setLastSelectedCharacterName(c.name);
        }
    }
}

void TGameClient::onCharacterChosen(const QString& characterName)
{
    m_characterSelection->setLastSelectedCharacterName(characterName);
    // A character switch from a running session starts at the character selection.
    if (!m_authentication->isRunning())
        m_authentication->start(true);
    m_authentication->trigger(authentication::TAuthenticationProcessController::ETrigger::CharacterLoginRequested);
    if (tibia::game::isOnline()) {
        Q_EMIT characterSelectionClosed();
        if (m_sameSocketCharacterSwitch) {
            if (!requestCharacterSwitch(characterName))
                fail(tr("Could not switch to %1.").arg(characterName));
            return;
        }
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

    if (chosen->tutorial) {
        const QString dump = game::resolveTutorialSessiondumpPath();
        if (!dump.isEmpty()) {
            qCInfo(lcGameClient).noquote() << "tutorial character: cinematic" << dump
                                           << "then live" << world->name << "as" << chosen->name;
            startTutorialThenLive(credentials, dump);
            return;
        }
        qCInfo(lcGameClient) << "tutorial character but no sessiondump fixture; entering live Newhaven";
    }

    connectLive(credentials);
}

void TGameClient::connectLive(const game::GameserverCredentials& credentials)
{
    qCInfo(lcGameClient).noquote() << "entering" << credentials.worldName << "as" << credentials.characterName
                                   << "at" << credentials.host << credentials.port;
    if (m_dualConnection)
        m_dualConnection->setEndpoint(credentials.host, credentials.port);
    if (!m_session) {
        m_session = std::make_unique<game::TGameserverGameSession>();
        linkGameActionRouters(*m_pGameActionRouter, *m_session);
        game::TGameserverGameSession::setLiveInstance(m_session.get());
    }
    m_session->activate();
    m_session->connectToGameserver(credentials);
}

void TGameClient::startTutorialThenLive(const game::GameserverCredentials& live, const QString& dumpPath)
{
    m_pendingLiveCredentials = live;
    m_tutorialCinematic = true;
    m_sessiondump = std::make_unique<game::TSessiondumpGameSession>();
    linkGameActionRouters(*m_pGameActionRouter, *m_sessiondump);
    m_sessiondump->activate();
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::worldEntered, this, &TGameClient::onWorldEntered);
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::gameLoginErrorMessage, this, &TGameClient::fail);
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::connectionError, this, &TGameClient::fail);
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::disconnected, this, [this] {
        // The replay ends inside its own signal: delete it once that returns.
        if (game::TSessiondumpGameSession* dump = m_sessiondump.release())
            dump->deleteLater();
        if (!m_tutorialCinematic) {
            onWorldLeft();
            return;
        }
        m_tutorialCinematic = false;
        if (!m_pendingLiveCredentials) {
            onWorldLeft();
            return;
        }
        const game::GameserverCredentials live = *m_pendingLiveCredentials;
        m_pendingLiveCredentials.reset();
        // Tear down cinematic world UI before the live session attaches.
        onWorldLeft();
        connectLive(live);
    });
    m_sessiondump->replay(dumpPath);
}

void TGameClient::finishTutorialCinematic()
{
    if (!m_tutorialCinematic || !m_sessiondump)
        return;
    qCInfo(lcGameClient) << "finishing tutorial cinematic for live handoff";
    m_sessiondump->disconnectNow();
}

void TGameClient::onWorldEntered()
{
    const QString world = m_session ? m_session->worldName()
                                    : (m_sessiondump ? m_sessiondump->worldName() : QString());
    const QString character = m_session ? m_session->characterName()
                                        : (m_sessiondump ? m_sessiondump->characterName() : QString());
    qCInfo(lcGameClient).noquote() << "in world" << world << "as" << character;
    // Entering a world dismisses the character list (login and alt switch).
    Q_EMIT characterSelectionClosed();
    // A new game session: no own team or listed teams, the team finder pages start over.
    teamfinder::TTeamFinderProtocolMessageHandler::instance()->startSession();
    if (const uint32_t playerId = tibia::game::localPlayerId(); playerId != 0 && m_characterConfigurationManager)
        m_characterConfigurationManager->onPlayerCreatureLoaded(playerId);
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
            const Position pos = tibia::game::localPlayerPosition();
            const tibia::creatures::TCreatureStorage* creatures = tibia::creatures::TCreatureStorage::instance();
            qCInfo(lcGameClient).noquote() << "after" << m_options.playForSeconds << "s:"
                                           << QStringLiteral("at %1,%2,%3").arg(pos.x).arg(pos.y).arg(pos.z)
                                           << "level" << tibia::game::localPlayerLevel()
                                           << "hp" << tibia::game::localPlayerHealth() << "/" << tibia::game::localPlayerMaxHealth()
                                           << "known creatures" << (creatures ? creatures->creatureIds().size() : 0)
                                           << "online" << tibia::game::isOnline();
            const bool online = tibia::game::isOnline();
            if (m_session)
                m_session->disconnectNow();
            else if (m_sessiondump)
                m_sessiondump->disconnectNow();
            QCoreApplication::exit(online ? 0 : 1);
        });
    }
}

void TGameClient::createCharacterConfigurationManager(config::TCharacterConfigurationManager::TStorages storages)
{
    storages.sidebar = m_sidebarOptions.get();
    storages.actionBar = m_actionBarOptions.get();
    storages.statusbar = m_statusbarStorage.get();
    storages.outfitDialog = m_outfitDialogStorage.get();
    m_characterConfigurationManager = std::make_unique<config::TCharacterConfigurationManager>(storages);
}

void TGameClient::onWorldLeft()
{
    if (m_dualConnection)
        m_dualConnection->reset();
    tibia::game::clearWaypointCostFactory();
    m_minimap.reset();
    if (m_characterConfigurationManager)
        m_characterConfigurationManager->saveConfiguration();
    if (chat::TTextStorage* texts = textStorage())
        texts->clear();
    if (m_soundController)
        m_soundController->handleSoundTrigger(true);
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
    // QTCLIENT_PERF / QTCLIENT_GPU_PERF must enable categories before isDebugEnabled gates:
    // bootstrap is constructed after setWindow, so rules set there would miss this hook.
    if (!qEnvironmentVariableIsEmpty("QTCLIENT_PERF") || !qEnvironmentVariableIsEmpty("QTCLIENT_GPU_PERF")) {
        // lcRaycast is registered as tibia.rendering (not mod.raycast).
        QLoggingCategory::setFilterRules(QStringLiteral(
            "tibia.frame.debug=true\n"
            "tibia.worldmap.debug=true\n"
            "tibia.worldmap.build.debug=true\n"
            "tibia.weather.perf.debug=true\n"
            "tibia.rendering.debug=true"));
    }
    tibia::creatures::enableWalkDebugFromEnv();

    // CIP: .
    connect(window, &QQuickWindow::afterAnimating, this, &TGameClient::tick);
    connect(window, &QQuickWindow::frameSwapped, this, [ring = m_frameDurations.get()] { ring->finish(); }, Qt::DirectConnection);
    // CIP onRenderThreadFrameSwapped: every swapped frame starts a frame of the FPS counter.
    connect(window, &QQuickWindow::frameSwapped, this, [this] {
        if (m_fpsHistory)
            m_fpsHistory->fpsCounter()->startNewFrame();
    }, Qt::DirectConnection);

    // Render-thread cost per frame, sync and render, against the plan's budgets.
    if (!lcFrameCost().isDebugEnabled())
        return;
    // The render stage also waits for the swapchain's drawable (vSync), so the
    // main pass recording is timed on its own.
    struct Cost
    {
        QElapsedTimer stage;
        QElapsedTimer pass;
        QElapsedTimer window;
        qint64 syncNs = 0;
        qint64 passNs = 0;
        qint64 stageNs = 0;
        qint64 maxNs = 0;
        qint64 frameSyncNs = 0;
        int frames = 0;
    };
    auto cost = std::make_shared<Cost>();
    connect(window, &QQuickWindow::beforeSynchronizing, this, [cost] { cost->stage.start(); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterSynchronizing, this, [cost] {
        cost->frameSyncNs = cost->stage.isValid() ? cost->stage.nsecsElapsed() : 0;
        cost->syncNs += cost->frameSyncNs;
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::beforeRendering, this, [cost] { cost->stage.start(); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::beforeRenderPassRecording, this, [cost] { cost->pass.start(); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterRenderPassRecording, this, [cost] {
        const qint64 ns = cost->pass.isValid() ? cost->pass.nsecsElapsed() : 0;
        cost->passNs += ns;
        cost->maxNs = std::max(cost->maxNs, cost->frameSyncNs + ns);
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterRendering, this, [cost] {
        if (!cost->stage.isValid())
            return;
        cost->stageNs += cost->stage.nsecsElapsed();
        ++cost->frames;
        if (!cost->window.isValid())
            cost->window.start();
        if (cost->window.elapsed() >= 1000) {
            qCDebug(lcFrameCost) << "frames" << cost->frames << "render thread us: sync" << cost->syncNs / cost->frames / 1000
                                 << "pass" << cost->passNs / cost->frames / 1000 << "max sync+pass" << cost->maxNs / 1000
                                 << "render stage incl. vsync wait" << cost->stageNs / cost->frames / 1000;
            *cost = Cost{};
            cost->window.start();
        }
    }, Qt::DirectConnection);
}

void TGameClient::initializeSound()
{
    const sound::TSoundSettings settings;
    m_soundConfiguration = std::make_shared<sound::TSoundConfiguration>(settings, config::TClientOptions::instance());
    if (!settings.noSound)
        sound::TSoundEngine::instance().initialize(m_soundConfiguration);
    if (!sound::TSoundEngine::instance().isInitialized())
        return;
    m_soundMetadataManager = std::make_shared<sound::TSoundMetadataManager>(nullptr, settings.ambienceDelayFactor);
    m_soundManager = std::make_shared<sound::TSoundManager>(m_soundMetadataManager, "sounds/", settings.soundCacheSeconds);
    connect(m_soundMetadataManager.get(), &sound::TSoundMetadataManager::metadataChanged, m_soundManager.get(),
            &sound::TSoundManager::onSoundMetadataChanged);
    m_soundPlayerManager = std::make_shared<sound::TSoundPlayerManager>(settings.soundPlayerCount);
    m_soundController = std::make_shared<sound::TSoundController>(m_soundMetadataManager, m_soundManager,
                                                                  m_soundPlayerManager, m_soundConfiguration);
    sound::TSoundController::setInstance(m_soundController.get());
    connect(m_pGameActionRouter->internalRouter(), &game::TInternalGameActionRouter::publishGameActionInternal, m_soundController.get(),
            &sound::TSoundController::handleGameAction);
    connect(m_soundManager.get(), &sound::TSoundManager::playableSoundsChanged, m_soundPlayerManager.get(),
            &sound::TSoundPlayerManager::onSoundManagerPlayableSoundsChanged);
    // Out of the game the title music plays once the sound bank is known.
    connect(m_soundMetadataManager.get(), &sound::TSoundMetadataManager::metadataChanged, this, [this] {
        if (!tibia::game::isOnline())
            m_soundController->handleSoundTrigger(true);
    });
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
    if (m_soundController) {
        // CIP TGameClientBase::v27 (before each frame): the controllers and the cache.
        const auto now = static_cast<int64_t>(shared::TClockService::frameMillis());
        if (game::TGameSessionBase* session = game::TGameSessionBase::active()) {
            session->soundStorage()->update(now);
            if (session->objectSoundController())
                session->objectSoundController()->updateObjectCounts(
                    tibia::game::hasLocalPlayer() ? tibia::game::localPlayerPosition() : Position());
        }
        m_soundController->update(now);
        m_soundManager->updateWithTimestamp(now);
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
    if (m_updater && m_started)
        m_updater->v2(m_predictor->predict());
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
    if (m_suppressConnectError)
        return;
    qCCritical(lcGameClient).noquote() << message;
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

