#include "gameclient.h"
#include "teamfinder/teamfinderciptypes.h"
#include "taskboard/taskboardciptypes.h"
#include "systemsurvey/systemsurveyciptypes.h"
#include "spells/spellsciptypes.h"
#include "skillwheel/skillwheelciptypes.h"
#include "screenshot/screenshotciptypes.h"
#include "reporting/reportingciptypes.h"
#include "quickloot/quicklootciptypes.h"
#include "questlog/questlogciptypes.h"
#include "protocol/protocolciptypes.h"
#include "proficiency/proficiencyciptypes.h"
#include "prey/preyciptypes.h"
#include "news/newsciptypes.h"
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
#include "bossbestiary/bossbestiaryciptypes.h"
#include "authentication/authenticationciptypes.h"
#include "assets/assetsciptypes.h"
#include "shared/sharedciptypes.h"
#include "game/gameciptypes.h"
#include "trade/tradeciptypes.h"

#include "app/clientoptions.h"
#include "chat/gamemessagehandler.h"
#include "chat/textstorage.h"
#include "client/gamecore.h"
#include "config/sidebaroptions.h"
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
#include "sound/soundstorage.h"
#include "renderer/nextframeupdater.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/nextframetimestamppredictor.h"
#include "effects/numericaleffect.h"
#include "worldmap/numericaleffectstorage.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/worldmapstorage.h"

#include <client/client.h>
#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/map.h>
#include <client/outfit.h>
#include <client/protocolgame.h>
#include <framework/core/resourcemanager.h>

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

Q_LOGGING_CATEGORY(lcGameClient, "tibia.client")
Q_LOGGING_CATEGORY(lcFrameCost, "tibia.frame", QtWarningMsg)

namespace tibia::client {

namespace {

QColor outfitColor(int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

/// CIP's world type names, by the login service's pvptype.
QString worldRules(int pvpType)
{
    switch (pvpType) {
        case 0: return QStringLiteral("Open PvP");
        case 1: return QStringLiteral("Optional PvP");
        case 2: return QStringLiteral("Hardcore PvP");
        case 3: return QStringLiteral("Retro Open PvP");
        case 4: return QStringLiteral("Retro Hardcore PvP");
        default: return {};
    }
}

QString premiumStatus(const authentication::PlaySessionData& session)
{
    if (!session.isPremium)
        return qtTrId("characterselection_status_free");
    const qint64 seconds = session.premiumUntil - QDateTime::currentSecsSinceEpoch();
    // Premium without an end date: the product's free-premium worlds.
    if (seconds <= 0)
        return qtTrId("characterselection_status_free_premium");
    const qint64 days = (seconds + 86399) / 86400;
    const QString left = qtTrId(days == 1 ? "characterselection_status_premiumtime" : "characterselection_status_premiumtime_plural").arg(days);
    return QStringLiteral("%1 (%2)").arg(qtTrId("characterselection_status_premium"), left);
}

}

TGameClient::TGameClient(QObject* parent)
    : cip::tibia::client::TGameClient(parent)
    , m_login(std::make_unique<authentication::TLoginRequestUploader>())
    , m_characterSelection(std::make_unique<gamewindow::TCharacterSelectionController>())
    , m_worldMapRenderProvider(std::make_unique<qmlcomponents::TWorldMapRenderProvider>())
    , m_sidebarOptions(std::make_unique<config::TSidebarOptions>())
    , m_statusbarStorage(std::make_unique<game::TStatusbarStorage>())
    , m_frameDurations(std::make_unique<worldmap::TFrameDurationRing>())
    , m_predictor(std::make_unique<worldmap::TNextFrameTimestampPredictor>())
    , m_sessiondumpRecorder(this)
    , m_sessiondumpAsyncLoader(this)
    , m_sessiondumpCatalogLoader(this)
    , m_sessiondumpPlayer(this)
{
    shared::ownProductSurface(this);
    assets::ownProductSurface(this);
    authentication::ownProductSurface(this);
    bossbestiary::ownProductSurface(this);
    client::ownProductSurface(this);
    common::ownProductSurface(this);
    dailyreward::ownProductSurface(this);
    exaltation::ownProductSurface(this);
    exivaoptions::ownProductSurface(this);
    game::ownProductSurface(this);
    gameWidnow::ownProductSurface(this);
    highscores::ownProductSurface(this);
    imbuing::ownProductSurface(this);
    market::ownProductSurface(this);
    network::ownProductSurface(this);
    news::ownProductSurface(this);
    prey::ownProductSurface(this);
    proficiency::ownProductSurface(this);
    protocol::ownProductSurface(this);
    questlog::ownProductSurface(this);
    quickloot::ownProductSurface(this);
    reporting::ownProductSurface(this);
    screenshot::ownProductSurface(this);
    skillwheel::ownProductSurface(this);
    spells::ownProductSurface(this);
    systemsurvey::ownProductSurface(this);
    taskboard::ownProductSurface(this);
    teamfinder::ownProductSurface(this);
    trade::ownProductSurface(this);



    // CIP: FUN_100906df0. The frame itself is afterAnimating → tick → TNextFrameUpdater.
    m_frameLimitTimer.setTimerType(Qt::PreciseTimer);
    m_frameLimitTimer.setSingleShot(true);
    connect(&m_frameLimitTimer, &QTimer::timeout, this, &TGameClient::tick);
    m_idleTick.setSingleShot(true);
    connect(&m_idleTick, &QTimer::timeout, this, &TGameClient::tick);

    connect(m_login.get(), &authentication::TLoginRequestUploader::loginSuccessful, this, &TGameClient::onLoginSuccessful);
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailed, this, &TGameClient::fail);
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailedDueToMissingTwoFactorToken, this, [this] {
        fail(tr("This account needs an authenticator token."));
    });
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailedDueToInvalidServerResponse, this, [this] {
        fail(tr("The login server sent an invalid response."));
    });
    connect(m_login.get(), &authentication::TLoginRequestUploader::loginFailedClientOutOfDate, this, [this] {
        Q_EMIT clientOutOfDate();
        fail(tr("This client is out of date. Start %1 from the launcher to update it.").arg(QCoreApplication::applicationName()));
    });

    connect(m_characterSelection.get(), &gamewindow::TCharacterSelectionController::characterChosen, this, &TGameClient::onCharacterChosen);
    connect(m_characterSelection.get(), &gamewindow::TCharacterSelectionController::cancelled, this, [this] {
        Q_EMIT characterSelectionClosed();
        // CIP: Cancel while online (alt switch) only closes the dialog. From the
        // login / post-logout character list, return to the login mask — never quit.
        if (g_game.isOnline())
            return;
        Q_EMIT returnToLoginRequested();
    });
    // CIP CHARACER_SELECTION_AUTO_CLOSE: hide the list without treating it as Cancel.
    input::setActionHandler(input::EGameAction::CHARACER_SELECTION_AUTO_CLOSE,
                            [self = QPointer<TGameClient>(this)](const input::TGameAction&) {
                                if (self)
                                    Q_EMIT self->characterSelectionClosed();
                                return true;
                            });
}

TGameClient::~TGameClient()
{
    input::setActionHandler(input::EGameAction::CHARACER_SELECTION_AUTO_CLOSE, nullptr);
    shutdown();
}

bool TGameClient::start(const QString& argv0, const QString& workDir, const Options& options)
{
    m_options = options;
    if (!gamecore::init(argv0, workDir)) {
        fail(tr("Could not start the game engine (no data/setup.otml under %1).").arg(workDir));
        return false;
    }
    m_started = true;

    // Engine objects exist only after init.
    m_opcodes = std::make_unique<network::ExtendedOpcodeRouter>();
    m_dualConnection = std::make_unique<network::TGameserverDualConnection>();
    m_soundProtocol = std::make_unique<sound::TSoundProtocolMessageHandler>();
    m_soundStorage = std::make_unique<sound::TSoundStorage>();
    m_ambientLight = std::make_unique<worldmap::TAmbientLightStorage>();
    m_worldMapStorage = std::make_unique<worldmap::TWorldMapStorage>();
    m_worldmapProtocol = std::make_unique<worldmap::TWorldmapProtocolMessageHandler>(m_worldMapStorage.get());
    m_textStorage = std::make_unique<chat::TTextStorage>();
    m_numericalEffects = std::make_unique<worldmap::TNumericalEffectStorage>();
    m_effectStorage = std::make_unique<effects::TEffectStorage>();
    m_effectGameActionHandler = std::make_unique<effects::TEffectGameActionHandler>();
    m_effectProtocolMessageHandler = std::make_unique<effects::TEffectProtocolMessageHandler>();
    m_messageHandler = std::make_unique<chat::TGameMessageHandler>(*m_textStorage, *m_numericalEffects);
    const QJsonObject clientOptions = app::ClientOptions::load().options();
    m_messageHandler->setOptions(clientOptions);
    m_frameRateLimitEnabled = clientOptions.value(QLatin1String("frameRateLimitEnabled")).toBool(false);
    m_frameRateLimit = std::clamp(clientOptions.value(QLatin1String("frameRateLimit")).toInt(60), 1, 255);
    const auto applyEffectOpacities = [](const QJsonObject& options) {
        const auto effectOpacity = [&](const char* key) {
            return static_cast<float>(std::clamp(options.value(QLatin1String(key)).toDouble(1.0), 0.0, 1.0));
        };
        g_client.setOwnSpellEffectAlpha(effectOpacity("ownEffectsOpacity"));
        g_client.setOtherPlayerSpellEffectAlpha(effectOpacity("otherPlayersEffectsOpacity"));
        g_client.setCreatureSpellEffectAlpha(effectOpacity("monsterEffectsOpacity"));
        g_client.setBossAreaCreatureEffectAlpha(effectOpacity("monsterBossAreaEffectsOpacity"));
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
    m_updater = std::make_unique<renderer::TNextFrameUpdater>(*m_worldMapRenderProvider, *m_textStorage, *m_numericalEffects);

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

void TGameClient::setMessageIgnoreList(chat::TWhiteBlacklist* list)
{
    if (m_messageHandler)
        m_messageHandler->setIgnoreList(list);
}

void TGameClient::shutdown()
{
    if (!m_started)
        return;
    m_started = false;
    m_idleTick.stop();
    m_frameLimitTimer.stop();
    onWorldLeft();
    m_updater.reset();
    m_messageHandler.reset();
    m_effectProtocolMessageHandler.reset();
    m_effectGameActionHandler.reset();
    m_effectStorage.reset();
    m_numericalEffects.reset();
    m_textStorage.reset();
    m_ambientLight.reset();
    m_opcodes.reset();
    m_dualConnection.reset();
    m_soundProtocol.reset();
    m_sessiondump.reset();
    m_session.reset();
    gamecore::terminate();
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
    m_login->login(env.loginUrl(), m_options.account, m_options.password, {}, env.revision);
}

void TGameClient::abortPendingEnter()
{
    m_suppressConnectError = true;
    m_login->abort();
    if (!g_game.isOnline()) {
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
    if (result.characters.isEmpty()) {
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
    // CipSoft login has no session.email; remember-email is fed from the typed mask / CLI.
    if (!m_options.account.isEmpty())
        Q_EMIT sessionEmailAvailable(m_options.account);
    QList<gamewindow::CharacterEntry> entries;
    entries.reserve(result.characters.size());
    // CIP: no saved selection yet, so prefer ismaincharacter. -1 lets the
    // sorted list highlight its first visible row instead of players.id order.
    int lastSelected = -1;
    for (const auto& c : result.characters) {
        const auto* world = result.world(c.worldId);
        gamewindow::CharacterEntry e;
        e.name = c.name;
        e.level = c.level;
        e.vocation = c.vocation;
        e.world = world ? world->name : QString();
        e.worldRules = world ? worldRules(world->pvpType) : QString();
        e.outfitId = c.outfitId;
        e.headColor = outfitColor(c.headColor);
        e.torsoColor = outfitColor(c.torsoColor);
        e.legsColor = outfitColor(c.legsColor);
        e.detailColor = outfitColor(c.detailColor);
        e.firstAddOn = c.addonsFlags & 1;
        e.secondAddOn = c.addonsFlags & 2;
        e.isMainCharacter = c.isMainCharacter;
        e.isHidden = c.isHidden;
        e.isOnline = c.isOnline;
        e.tutorial = c.tutorial;
        if (c.isMainCharacter)
            lastSelected = int(entries.size());
        entries.push_back(e);
    }
    qCInfo(lcGameClient) << "account has" << entries.size() << "characters on" << result.worlds.size() << "worlds";
    m_characterSelection->setAccount(premiumStatus(result.session), result.session.isPremium, entries, lastSelected);

    if (!m_options.autoCharacter.isEmpty()) {
        for (const auto& e : entries) {
            if (e.name.compare(m_options.autoCharacter, Qt::CaseInsensitive) == 0) {
                onCharacterChosen(e);
                return;
            }
        }
        fail(tr("No character named %1 on this account.").arg(m_options.autoCharacter));
        return;
    }
    Q_EMIT characterSelectionRequested();
}

void TGameClient::openCharacterSelectionForSwitch(const QJsonArray& alts)
{
    if (m_account.characters.isEmpty())
        return;
    QHash<QString, bool> onlineByName;
    for (const QJsonValue& v : alts) {
        const QJsonObject alt = v.toObject();
        const QString name = alt.value(QLatin1String("name")).toString();
        if (name.isEmpty())
            continue;
        const bool online = alt.value(QLatin1String("playing")).toBool()
            || alt.value(QLatin1String("online")).toBool()
            || name.compare(QString::fromStdString(g_game.getCharacterName()), Qt::CaseInsensitive) == 0;
        onlineByName.insert(name.toLower(), online);
    }
    QList<gamewindow::CharacterEntry> entries;
    entries.reserve(m_account.characters.size());
    // CIP Change Character: keep last confirmed selection (controller), fall back to
    // ismaincharacter, else first visible row — do not force the currently playing char.
    int preferred = -1;
    for (const auto& c : m_account.characters) {
        const auto* world = m_account.world(c.worldId);
        gamewindow::CharacterEntry e;
        e.name = c.name;
        e.level = c.level;
        e.vocation = c.vocation;
        e.world = world ? world->name : QString();
        e.worldRules = world ? worldRules(world->pvpType) : QString();
        e.outfitId = c.outfitId;
        e.headColor = outfitColor(c.headColor);
        e.torsoColor = outfitColor(c.torsoColor);
        e.legsColor = outfitColor(c.legsColor);
        e.detailColor = outfitColor(c.detailColor);
        e.firstAddOn = c.addonsFlags & 1;
        e.secondAddOn = c.addonsFlags & 2;
        e.isMainCharacter = c.isMainCharacter;
        e.isHidden = c.isHidden;
        e.isOnline = onlineByName.value(c.name.toLower(), c.isOnline);
        e.tutorial = c.tutorial;
        if (c.isMainCharacter && preferred < 0)
            preferred = int(entries.size());
        entries.push_back(e);
    }
    m_characterSelection->setAccount(premiumStatus(m_account.session), m_account.session.isPremium, entries, preferred);
    Q_EMIT characterSelectionRequested();
}

void TGameClient::closeCharacterSelection()
{
    Q_EMIT characterSelectionClosed();
}

void TGameClient::ensureLoginSurface()
{
    if (g_game.isOnline() || g_game.isSwitchingCharacter() || !m_options.autoCharacter.isEmpty())
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
    if (characterName.isEmpty() || !g_game.isOnline())
        return false;
    if (characterName.compare(QString::fromStdString(g_game.getCharacterName()), Qt::CaseInsensitive) == 0)
        return false;
    if (g_game.isSwitchingCharacter())
        return false;
    const ProtocolGamePtr protocol = g_game.getProtocolGame();
    if (!protocol)
        return false;
    protocol->sendCharacterSwitch(characterName.toStdString());
    Q_EMIT characterSwitchRequested(characterName);
    return true;
}

void TGameClient::onCharacterChosen(const gamewindow::CharacterEntry& character)
{
    if (g_game.isOnline()) {
        Q_EMIT characterSelectionClosed();
        if (!requestCharacterSwitch(character.name))
            fail(tr("Could not switch to %1.").arg(character.name));
        return;
    }

    const authentication::LoginCharacter* chosen = nullptr;
    for (const auto& c : m_account.characters) {
        if (c.name == character.name) {
            chosen = &c;
            break;
        }
    }
    const auto* world = chosen ? m_account.world(chosen->worldId) : nullptr;
    if (!world) {
        fail(tr("The world of %1 is not in the world list.").arg(character.name));
        return;
    }
    Q_EMIT characterSelectionClosed();
    m_worldType = world->pvpType;

    game::GameserverCredentials credentials;
    credentials.account = m_options.account;
    credentials.password = m_options.password;
    credentials.sessionKey = m_account.session.sessionKey;
    credentials.worldName = world->name;
    credentials.host = world->host;
    credentials.port = world->port;
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
    if (!m_session)
        m_session = std::make_unique<game::TGameserverGameSession>();
    m_session->connectToGameserver(credentials);
}

void TGameClient::startTutorialThenLive(const game::GameserverCredentials& live, const QString& dumpPath)
{
    m_pendingLiveCredentials = live;
    m_tutorialCinematic = true;
    m_sessiondump = std::make_unique<game::TSessiondumpGameSession>();
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::worldEntered, this, &TGameClient::onWorldEntered);
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::gameLoginErrorMessage, this, &TGameClient::fail);
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::connectionError, this, &TGameClient::fail);
    connect(m_sessiondump.get(), &game::TSessiondumpGameSession::disconnected, this, [this] {
        m_sessiondump.reset();
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
    if (const LocalPlayerPtr& player = g_game.getLocalPlayer()) {
        m_sidebarOptions->loadForCharacter(player->getId());
        m_statusbarStorage->loadForCharacter(player->getId());
    }
    const std::string assets = gamecore::assetsRoot(ClientVersion);
    // CipSoft: Resources/minimap is a sibling of Resources/assets (assetsRoot adds
    // Resources as a search path so short name `minimap/` resolves). Prefer that
    // over nested assets/minimap. Prefer getRealPath over getRealDir+concat so a
    // "/" search root is never re-joined into "//…" (wasm MEMFS rejects that).
    std::string defaultMinimapPath = tibia::files::data().getRealPath("minimap");
    if (defaultMinimapPath.empty())
        defaultMinimapPath = tibia::files::data().getRealPath(assets + "minimap");
    const std::string markersPath = tibia::files::data().getRealPath(assets + "minimap-markers.json");
    m_minimap = std::make_unique<minimap::TMinimapManager>(
        defaultMinimapPath.empty() ? QString() : QString::fromStdString(defaultMinimapPath),
        markersPath.empty() ? QString() : QString::fromStdString(markersPath));
    m_minimap->setProtocolHandler(m_worldmapProtocol.get());
    // CIP click-to-walk / minimap GoPath uses Indexed8 waypoint costs, not only
    // locally WasSeen g_minimap tiles — otherwise far clicks stop mid-path.
    g_map.setWaypointCostFactory([this]() -> std::shared_ptr<minimap::TWaypointCosts> {
        return minimap::snapshotWaypointCosts(m_minimap ? m_minimap->tileStorage() : nullptr);
    });
    Q_EMIT mainWindowInitialized();

    for (qsizetype i = 0; i < m_options.say.size(); ++i) {
        QTimer::singleShot(1000 * (i + 1), this, [text = m_options.say[i].toStdString()] {
            if (g_game.isOnline())
                g_game.talk(text);
        });
    }

    if (m_options.playForSeconds > 0) {
        QTimer::singleShot(m_options.playForSeconds * 1000, this, [this] {
            const auto player = g_game.getLocalPlayer();
            const Position pos = player ? player->getPosition() : Position();
            qCInfo(lcGameClient).noquote() << "after" << m_options.playForSeconds << "s:"
                                           << QStringLiteral("at %1,%2,%3").arg(pos.x).arg(pos.y).arg(pos.z)
                                           << "level" << (player ? player->getLevel() : 0)
                                           << "hp" << (player ? player->getHealth() : 0) << "/" << (player ? player->getMaxHealth() : 0)
                                           << "known creatures" << g_map.getCreatures().size()
                                           << "online" << g_game.isOnline();
            const bool online = g_game.isOnline();
            if (m_session)
                m_session->disconnectNow();
            else if (m_sessiondump)
                m_sessiondump->disconnectNow();
            QCoreApplication::exit(online ? 0 : 1);
        });
    }
}

void TGameClient::onWorldLeft()
{
    if (m_dualConnection)
        m_dualConnection->reset();
    g_map.clearWaypointCostFactory();
    m_minimap.reset();
    m_sidebarOptions->save();
    m_statusbarStorage->save();
    if (m_textStorage)
        m_textStorage->clear();
    if (m_numericalEffects)
        m_numericalEffects->clear();
    Q_EMIT worldLeft();
    // CIP: after logout return to character selection while the account session remains.
    ensureLoginSurface();
}

void TGameClient::setWindow(QQuickWindow* window)
{
    // QTCLIENT_PERF / QTCLIENT_GPU_PERF must enable categories before isDebugEnabled() gates:
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

    // CIP: FUN_1008f6510.
    connect(window, &QQuickWindow::afterAnimating, this, &TGameClient::tick);
    connect(window, &QQuickWindow::frameSwapped, this, [ring = m_frameDurations.get()] { ring->finish(); }, Qt::DirectConnection);

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

void TGameClient::tick()
{
    gamecore::poll();
    if (m_soundStorage)
        m_soundStorage->poll();
    m_idleTick.start(g_game.isOnline() ? 500 : 100);
    if (m_frameRateLimitEnabled && !frameSlotReached(gamecore::nowMicros()))
        return;

    // Shown one tick-to-swap time from now, on average over the last frames.
    const qint64 offsetMicros = m_frameDurations->averageNs() / 1000;
    m_frameDurations->start();
    m_predictor->setOffset(offsetMicros);
    if (m_updater && m_started)
        m_updater->advance(m_predictor->predict());
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
    if (!g_game.isOnline())
        ensureLoginSurface();
}

}
