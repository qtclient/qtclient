#include "sessiondumpgamesession.h"
#include "game/gameactionrouter.h"

#include "chat/chatchannel.h"
#include "container/containerciptypes.h"
#include "config/characterconfigurationmanager.h"
#include "game/sessiondumpdmpd.h"
#include "input/gameaction.h"
#include "protocol/protocolmessagequeue.h"
#include "renderer/nextframeupdater.h"
#include "sessiondump/sessiondumpasyncloader.h"
#include "sessiondump/sessiondumpplayer.h"
#include "sessiondump/sessiondumpprogressclockservice.h"
#include "clockservice.h"

#include "game/tgamenotify.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

#include <QFile>
#include <QUrl>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <tibia/game/gamesession.h>
#include "gameservergamesession.h"

namespace tibia::game {

using SoftHook = tibia::game::SoftHook;
using Args = TGameNotify::Args;

namespace {

/// How far the next frame updater runs ahead when the player resets the storages.
constexpr qint64 StorageResetLookaheadMillis = 60000;

template<typename F>
SoftHook gameHook(const char* field, F&& f)
{
    return SoftHook(field, [f = std::forward<F>(f)](const Args& args) {
        f(args);
        return std::any{};
    });
}

}

TSessiondumpGameSession::TSessiondumpGameSession(
    std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper,
    std::shared_ptr<appearances::TAppearancesManager> appearancesManager,
    std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory,
    std::shared_ptr<config::TClientOptions> clientOptions, std::shared_ptr<shared::TTibiaClipboardHelper> clipboardHelper,
    std::shared_ptr<shared::TTibiaUrlTools> urlTools, std::shared_ptr<shared::IClockService> clientClockService,
    std::shared_ptr<analysers::TItemPriceStorage> itemPriceStorage, std::shared_ptr<sound::TSoundController> soundController,
    std::shared_ptr<input::TKeyboardShortcutHandler> keyboardShortcutHandler, spells::TSpellInfoStorage* spellInfoStorage,
    TPassiveAbilityInfoStorage* passiveAbilityInfoStorage, proficiency::TProficiencyStorage* proficiencyStorage,
    QObject* parent)
    : TGameSessionBase(std::move(tibiaFileSystemHelper), std::move(appearancesManager), std::move(appearanceInstanceFactory),
                       std::move(clientOptions), std::move(clipboardHelper), std::move(urlTools),
                       std::move(clientClockService), std::move(itemPriceStorage), std::move(soundController),
                       std::move(keyboardShortcutHandler), spellInfoStorage, passiveAbilityInfoStorage, proficiencyStorage,
                       parent)
    , m_sessiondumpProgressClock(std::make_shared<sessiondump::TSessiondumpProgressClockService>(TGameSessionBase::clientClockService()))
{
    m_replayTimer.setParent(this);
    m_replayTimer.setSingleShot(true);
    connect(&m_replayTimer, &QTimer::timeout, this, &TSessiondumpGameSession::pumpReplay);
    m_sessiondumpTimer.setParent(this);
    // The base announces worldEntered for every WorldEntered message.
    connect(TGameNotify::instance(), &TGameNotify::gameEnded, this, [this] {
        onSessiondumpFinished();
        Q_EMIT disconnected();
    });
    connect(TGameNotify::instance(), &TGameNotify::connectionFailing, this, [this](bool failing) {
        if (failing)
            Q_EMIT pingTimeout();
        else
            Q_EMIT pingTimeoutRecovered();
    });
    m_hooks.push_back(gameHook("onLoginError", [this](const Args& a) { Q_EMIT gameLoginErrorMessage(argString(a, 0), 0); }));
    m_hooks.push_back(gameHook("onLoginAdvice", [this](const Args& a) { Q_EMIT gameLoginAdviceMessage(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onLoginWait", [this](const Args& a) {
        Q_EMIT gameLoginWaitMessage(argString(a, 0), static_cast<int>(argInt(a, 1)));
    }));
    m_hooks.push_back(gameHook("onConnectionError", [this](const Args& a) { Q_EMIT connectionError(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onUpdateNeeded", [this](const Args&) { Q_EMIT clientOutOfDate(); }));
    m_hooks.push_back(gameHook("onSessionDumpMarker", [this](const Args& a) {
        Q_EMIT sessionDumpMarker(argString(a, 0));
    }));
}

TSessiondumpGameSession::~TSessiondumpGameSession()
{
    stopReplay();
}

void TSessiondumpGameSession::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    TGameSessionBase::initialize(metaInfo);
    m_sessiondumpMetaInfo = std::dynamic_pointer_cast<const TSessiondumpGameSessionMetaInfo>(metaInfo);
    if (!m_sessiondumpMetaInfo)
        throw std::invalid_argument("wrong type of TGameSessionMetaInfo submitted");
    m_configurationCharacterId = m_sessiondumpMetaInfo->sessiondump.characterId;
    // A dump's protocol handlers are linked before the providers exist.
    createStorages();
    createProtocolMessageHandlers();
    connectProtocolMessageHandlers();
    createRenderProviders();
    createSoundProviders();
    createGameActionProvidersAndHandlers();
    connectGameActionHandlers();
    // The dump is loaded now and played when the session starts.
    startSessiondump(m_sessiondumpMetaInfo->sessiondump);
    // Host: a periodic timer stands in for CipSoft's zero-interval single-shot
    // timer that the game client restarts every tick and frame.
    m_sessiondumpTimer.setInterval(16);
    m_connectionManager.add(
        connect(&m_sessiondumpTimer, &QTimer::timeout, this, &TSessiondumpGameSession::onSessiondumpTimerElapsed));
}

void TSessiondumpGameSession::startSession()
{
    // The character configuration is saved and the dump's one loaded.
    const std::shared_ptr<config::TCharacterConfigurationManager>& manager = characterConfigurationManager();
    manager->saveConfiguration();
    manager->loadConfigurationForCharacter(m_configurationCharacterId);
    gameActionRouter()->handleGameAction(std::make_shared<input::gameactions::TGameActionSimple>(
        input::EGameAction::CLOSE_ALL_DISPLAYED_BUT_NOT_OPENED_CONTAINERS));
    if (m_sessiondumpPlayer) {
        // Host: the classic session state starts the replay.
        tibia::game::startDecodedReplay();
        m_sessiondumpPlayer->start();
        m_sessiondumpPlayer->setDuration(m_sessiondumpMetaInfo->sessiondump.durationMilliseconds);
        m_sessiondumpTimer.start();
    } else if (!m_replayLoadError.isEmpty()) {
        // Host: a qtclient text dump that did not load.
        Q_EMIT connectionError(m_replayLoadError);
        return;
    } else if (!m_replayPackets.empty()) {
        // Host: a qtclient text dump replays its packets.
        startReplay();
    }
    Q_EMIT connected();
}

void TSessiondumpGameSession::updatePlayerMovementIntentHandler()
{
    // A dump has no player movement intent handler.
}

void TSessiondumpGameSession::startSessiondump(const sessiondump::TSessiondumpCatalog::TEntry& entry)
{
    const QUrl& url = entry.url;
    replay(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

bool TSessiondumpGameSession::isTutorial() const
{
    return m_sessiondumpMetaInfo && m_sessiondumpMetaInfo->getType() == TGameSessionMetaInfo::EType::TutorialSessiondump;
}

void TSessiondumpGameSession::replay(const QString& file)
{
    stopReplay();
    if (isSessiondumpDmpdFile(file)) {
        replayDmpd(file);
        return;
    }
    replayHexDump(file);
}

void TSessiondumpGameSession::replayDmpd(const QString& file)
{
    // A loader for the dump and a player on the session's progress clock.
    m_sessiondumpLoader = std::make_shared<sessiondump::TSessiondumpAsyncLoader>(QUrl::fromLocalFile(file));
    m_sessiondumpPlayer = std::make_shared<sessiondump::TSessiondumpPlayer>(m_sessiondumpLoader, sessiondumpProgressClock());
    using sessiondump::TSessiondumpPlayer;
    TSessiondumpPlayer* player = m_sessiondumpPlayer.get();
    connect(player, &TSessiondumpPlayer::finished, this, &TSessiondumpGameSession::onSessiondumpFinished);
    // Host: the replay timer stops and the classic session state ends the replay
    // (after the session's own slot: stopDmpd drops the player's links to it).
    connect(player, &TSessiondumpPlayer::finished, this, [this] {
        stopDmpd();
        tibia::game::finishDecodedReplay();
    });
    // The player's server messages go into the session's protocol queue.
    // Host: the classic session state enters the game on PendingStateEntered;
    // a dump that starts at WorldEntered gets one first.
    m_sawPendingStateEntered = false;
    connect(player, &TSessiondumpPlayer::gameserverMessageAvailable, this,
            [this](const sessiondump::TGameserverMessagePointer& message) { enqueueGameserverMessage(message); });
    connect(player, &TSessiondumpPlayer::requestResetOfStorages, this, &TSessiondumpGameSession::onRequestResetOfStorages);
    connect(player, &TSessiondumpPlayer::sessiondumpLoadingError, this, &TSessiondumpGameSession::connectionError);
    // The game actions reach the player (pause, resume, skip to) and the
    // player's reach the router.
    TInternalGameActionRouter* router = gameActionRouter()->internalRouter();
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, player, &TSessiondumpPlayer::handleGameAction);
    connect(player, &TSessiondumpPlayer::publishGameAction, router, &TInternalGameActionRouter::handleGameActionInternal);
    // Host: a dump with a session key announces it (qtclient's sessionDumpStart).
    connect(m_sessiondumpLoader.get(), &sessiondump::TSessiondumpAsyncLoader::sessiondumpMetadataLoaded, this, [this] {
        const auto& metadata = m_sessiondumpLoader->metadata();
        if (!metadata.sessionKey.isEmpty())
            Q_EMIT sessionDumpStart(metadata.sessionKey);
    });
}

void TSessiondumpGameSession::enqueueGameserverMessage(const sessiondump::TGameserverMessagePointer& message)
{
    namespace pb = ::tibia::protobuf::protocol;
    constexpr int PendingStateEnteredType = 10;
    constexpr int WorldEnteredType = 15;
    if (!message)
        return;
    protocol::TProtocolMessageQueue* queue = protocolMessageQueue().get();
    if (message->type() == PendingStateEnteredType)
        m_sawPendingStateEntered = true;
    if (message->type() == WorldEnteredType && !m_sawPendingStateEntered) {
        m_sawPendingStateEntered = true;
        auto pending = std::make_shared<pb::GameserverMessage>();
        pending->set_type(PendingStateEnteredType);
        pending->MutableExtension(pb::GameserverMessagePendingStateEntered::gameserver_message_pending_state_entered);
        if (message->has_meta_data())
            *pending->mutable_meta_data() = message->meta_data();
        queue->enqueueGameserverMessage(pending);
    }
    queue->enqueueGameserverMessage(message);
}

void TSessiondumpGameSession::stopDmpd()
{
    m_sessiondumpTimer.stop();
    if (m_sessiondumpPlayer)
        m_sessiondumpPlayer->disconnect(this);
}

void TSessiondumpGameSession::replayHexDump(const QString& file)
{
    try {
        namespace fs = std::filesystem;
        fs::path path(file.toStdString());
        if (!path.is_absolute() && !fs::exists(path))
            path = fs::path("records") / path;
        QFile input(QString::fromStdString(path.string()));
        if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_replayLoadError = tr("Invalid record file: %1").arg(file);
            return;
        }
        std::vector<ReplayBody> bodies;
        while (!input.atEnd()) {
            const QString line = QString::fromUtf8(input.readLine()).trimmed();
            if (line.isEmpty())
                continue;
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() < 3 || parts[0] != QLatin1String("<"))
                continue;
            bool timeOk = false;
            const qint64 at = parts[1].toLongLong(&timeOk);
            if (!timeOk)
                continue;
            const QByteArray hex = parts[2].toLatin1();
            if (hex.size() < 2 || (hex.size() % 2) != 0)
                continue;
            QByteArray bytes;
            bytes.resize(hex.size() / 2);
            for (int i = 0; i < bytes.size(); ++i) {
                bool ok = false;
                const int value = QString::fromLatin1(hex.mid(i * 2, 2)).toInt(&ok, 16);
                if (!ok) {
                    bytes.clear();
                    break;
                }
                bytes[i] = char(value);
            }
            if (!bytes.isEmpty())
                bodies.push_back(ReplayBody{ at, bytes });
        }
        if (bodies.empty()) {
            m_replayLoadError = tr("Invalid record file: %1").arg(file);
            return;
        }
        // Played when the session starts.
        m_replayPackets = std::move(bodies);
    } catch (const std::exception& e) {
        m_replayLoadError = QString::fromUtf8(e.what());
    }
}

void TSessiondumpGameSession::startReplay()
{
    try {
        tibia::game::startDecodedReplay();
    } catch (const std::exception& e) {
        Q_EMIT connectionError(QString::fromUtf8(e.what()));
        return;
    }
    m_replayIndex = 0;
    m_replaying = true;
    m_replayClock.start();
    pumpReplay();
}

void TSessiondumpGameSession::pumpReplay()
{
    if (!m_replaying)
        return;
    const qint64 now = m_replayClock.elapsed();
    while (m_replaying && m_replayIndex < int(m_replayPackets.size()) && m_replayPackets[size_t(m_replayIndex)].atMs <= now) {
        const QByteArray body = m_replayPackets[size_t(m_replayIndex)].bytes;
        ++m_replayIndex;
        tibia::game::deliverReplayPacket(reinterpret_cast<const uint8_t*>(body.constData()), size_t(body.size()));
    }
    if (!m_replaying)
        return;
    if (m_replayIndex >= int(m_replayPackets.size())) {
        m_replaying = false;
        tibia::game::finishDecodedReplay();
        return;
    }
    const int wait = int(m_replayPackets[size_t(m_replayIndex)].atMs - now);
    m_replayTimer.start(qMax(1, wait));
}

void TSessiondumpGameSession::stopReplay()
{
    stopDmpd();
    m_replayLoadError.clear();
    m_sessiondumpPlayer.reset();
    m_sessiondumpLoader.reset();
    m_replaying = false;
    m_replayTimer.stop();
    m_replayPackets.clear();
    m_replayIndex = 0;
}

void TSessiondumpGameSession::requestLogout()
{
    disconnectNow();
}

void TSessiondumpGameSession::disconnectNow()
{
    // The dump's clock stands still.
    m_sessiondumpProgressClock->pause();
    // Host: the replay stops and the classic session state lets go of it.
    stopReplay();
    hubDisconnectNow();
}

std::shared_ptr<shared::IClockService> TSessiondumpGameSession::clockService() const
{
    return m_sessiondumpProgressClock;
}

bool TSessiondumpGameSession::isOnline() const
{
    return tibia::game::isOnline();
}

QString TSessiondumpGameSession::characterName() const
{
    return QString::fromStdString(tibia::game::characterName());
}

QString TSessiondumpGameSession::worldName() const
{
    return QString::fromStdString(tibia::game::getWorldName());
}

void TSessiondumpGameSession::onRequestResetOfStorages()
{
    // The player jumps (skip to a keyframe): the effects, texts and animations run
    // out a minute ahead, and the containers and chat channels start over.
    nextFrameUpdater()->update(clockService()->nowMillis() + StorageResetLookaheadMillis);
    containerStorage()->clear();
    chatChannelStorage()->clearAllChannels();
}

void TSessiondumpGameSession::onSessiondumpTimerElapsed()
{
    m_sessiondumpPlayer->processSessiondump();
}

void TSessiondumpGameSession::onSessiondumpFinished()
{
}

}
