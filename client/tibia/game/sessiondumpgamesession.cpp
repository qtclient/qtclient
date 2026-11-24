#include "sessiondumpgamesession.h"
#include "game/gameactionrouter.h"

#include "chat/chatchannel.h"
#include "container/containerciptypes.h"
#include "game/sessiondumpdmpd.h"
#include "renderer/nextframeupdater.h"
#include "sessiondump/sessiondumpasyncloader.h"
#include "sessiondump/sessiondumpplayer.h"
#include "sessiondump/sessiondumpprogressclockservice.h"
#include "clockservice.h"
#include "tutorial/sessiondumphints.h"
#include "tutorial/sessiondumphintsprocessor.h"

#include "game/tgamenotify.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
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

TSessiondumpGameSession::TSessiondumpGameSession(QObject* parent)
    : TGameSessionBase(parent)
    , m_sessiondumpProgressClock(std::make_shared<sessiondump::TSessiondumpProgressClockService>(shared::TClockService::process()))
{
    m_replayTimer.setParent(this);
    m_replayTimer.setSingleShot(true);
    connect(&m_replayTimer, &QTimer::timeout, this, &TSessiondumpGameSession::pumpReplay);
    // The game connection publishes onConnect once the socket is open.
    m_hooks.emplace_back("onConnect", [this](const Args&) {
        Q_EMIT connected();
        return std::any{};
    });
    connect(TGameNotify::instance(), &TGameNotify::gameStarted, this, [this] { Q_EMIT worldEntered(); });
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
    stopHints();
}

void TSessiondumpGameSession::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    TGameSessionBase::initialize(metaInfo);
    m_sessiondumpMetaInfo = std::dynamic_pointer_cast<const TSessiondumpGameSessionMetaInfo>(metaInfo);
    if (!m_sessiondumpMetaInfo)
        throw std::invalid_argument("wrong type of TGameSessionMetaInfo submitted");
    createStorages();
    createProtocolMessageHandlers();
    createRenderProviders();
    createSoundProviders();
    createGameActionProvidersAndHandlers();
    connectProtocolMessageHandlers();
    connectGameActionHandlers();
}

void TSessiondumpGameSession::startSession()
{
    const QUrl& url = m_sessiondumpMetaInfo->sessiondump.url;
    replay(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

bool TSessiondumpGameSession::isTutorial() const
{
    return m_sessiondumpMetaInfo && m_sessiondumpMetaInfo->getType() == TGameSessionMetaInfo::EType::TutorialSessiondump;
}

void TSessiondumpGameSession::replay(const QString& file)
{
    stopReplay();
    stopHints();
    if (isSessiondumpDmpdFile(file)) {
        replayDmpd(file);
        return;
    }
    replayHexDump(file);
}

void TSessiondumpGameSession::replayDmpd(const QString& file)
{
    // Host wiring of CipSoft's v158: a loader for the file and a player on the
    // session's progress clock whose published messages reach the replay bridge.
    m_sessiondumpLoader = std::make_shared<sessiondump::TSessiondumpAsyncLoader>(QUrl::fromLocalFile(file));
    m_sessiondumpPlayer = std::make_shared<sessiondump::TSessiondumpPlayer>(m_sessiondumpLoader, m_sessiondumpProgressClock);
    auto converter = std::make_shared<TDmpdClassicConverter>(QByteArray());
    sessiondump::TSessiondumpPlayer* player = m_sessiondumpPlayer.get();
    connect(player, &sessiondump::TSessiondumpPlayer::gameserverMessageAvailable, this,
            [converter](const sessiondump::TGameserverMessagePointer& message) {
                const std::string bytes = message->SerializeAsString();
                for (const auto& packet : converter->convert(QByteArray::fromStdString(bytes)))
                    tibia::game::deliverReplayPacket(packet->data(), packet->size());
            });
    connect(m_sessiondumpLoader.get(), &sessiondump::TSessiondumpAsyncLoader::sessiondumpMetadataLoaded, this,
            [this, converter] {
                const auto& metadata = m_sessiondumpLoader->metadata();
                *converter = TDmpdClassicConverter(metadata.characterName.toUtf8());
                if (!metadata.sessionKey.isEmpty())
                    Q_EMIT sessionDumpStart(metadata.sessionKey);
            });
    connect(player, &sessiondump::TSessiondumpPlayer::requestResetOfStorages, this,
            &TSessiondumpGameSession::onRequestResetOfStorages);
    connect(player, &sessiondump::TSessiondumpPlayer::finished, this, [this] {
        stopDmpd();
        tibia::game::finishDecodedReplay();
        onSessiondumpFinished();
    });
    connect(player, &sessiondump::TSessiondumpPlayer::sessiondumpLoadingError, this,
            [this](const QString& message) { Q_EMIT connectionError(message); });
    m_sessiondumpTimer.setParent(this);
    // Host: a periodic timer stands in for CipSoft's per-frame restart of its
    // zero-interval single-shot timer.
    m_sessiondumpTimer.setInterval(16);
    connect(&m_sessiondumpTimer, &QTimer::timeout, this, &TSessiondumpGameSession::onSessiondumpTimerElapsed,
            Qt::UniqueConnection);
    tibia::game::startDecodedReplay();
    player->start();
    m_sessiondumpTimer.start();
    startHintsForDump(file);
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
            Q_EMIT connectionError(tr("Invalid record file: %1").arg(file));
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
            Q_EMIT connectionError(tr("Invalid record file: %1").arg(file));
            return;
        }
        startReplay(std::move(bodies));
        startHintsForDump(file);
    } catch (const std::exception& e) {
        Q_EMIT connectionError(QString::fromUtf8(e.what()));
    }
}

void TSessiondumpGameSession::startReplay(std::vector<ReplayBody> packets)
{
    tibia::game::startDecodedReplay();
    m_replayPackets = std::move(packets);
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
    m_sessiondumpPlayer.reset();
    m_sessiondumpLoader.reset();
    m_replaying = false;
    m_replayTimer.stop();
    m_replayPackets.clear();
    m_replayIndex = 0;
}

void TSessiondumpGameSession::startHintsForDump(const QString& dumpPath)
{
    const QString hintsPath = tutorial::hintsPathForDump(dumpPath);
    if (hintsPath.isEmpty())
        return;
    QFile file(hintsPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    std::shared_ptr<tutorial::TSessiondumpHints> hints;
    try {
        hints = std::make_shared<tutorial::TSessiondumpHints>(QJsonDocument::fromJson(file.readAll()));
    } catch (const std::exception& e) {
        qWarning() << "Invalid sessiondump hints" << hintsPath << e.what();
        return;
    }
    m_hints = std::make_unique<tutorial::TSessiondumpHintsProcessor>(hints);
    connect(m_hints.get(), &tutorial::TSessiondumpHintsProcessor::publishGameAction, gameActionRouter()->internalRouter(),
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(gameActionRouter()->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_hints.get(),
            &tutorial::TSessiondumpHintsProcessor::handleGameAction);
    // CipSoft's tutorial session tick: the dump pauses at the next hint until the
    // hints due by then are processed (a condition stays due until it is met).
    connect(&m_sessiondumpTimer, &QTimer::timeout, m_hints.get(), [this] {
        if (!m_hints || !m_sessiondumpPlayer)
            return;
        m_sessiondumpPlayer->setPauseAt(m_hints->nextHintTimestamp());
        const int64_t progress = m_sessiondumpPlayer->progress();
        if (m_hints->hasHintToProcess(progress)) {
            m_sessiondumpPlayer->pause();
            m_hints->processHints(progress);
        }
        if (!m_hints->hasHintToProcess(progress) && !m_sessiondumpPlayer->isRunning())
            m_sessiondumpPlayer->resume();
    });
    if (!m_sessiondumpPlayer) {
        // Text dumps: no player to pause; the hints follow the replay clock.
        connect(&m_replayTimer, &QTimer::timeout, m_hints.get(), [this] {
            if (m_hints && m_replaying)
                m_hints->processHints(m_replayClock.elapsed());
        });
    }
}

void TSessiondumpGameSession::stopHints()
{
    m_hints.reset();
}

void TSessiondumpGameSession::requestLogout()
{
    disconnectNow();
}

void TSessiondumpGameSession::disconnectNow()
{
    stopReplay();
    stopHints();
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
    stopHints();
}

}
