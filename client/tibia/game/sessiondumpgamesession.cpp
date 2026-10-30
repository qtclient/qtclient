#include "sessiondumpgamesession.h"

#include "game/sessiondumpdmpd.h"
#include "tutorial/sessiondumphintsprocessor.h"

#include <tibia/world/game.h>
#include <client/protocolgame.h>

#include <QFile>
#include <QLoggingCategory>

#include <filesystem>

Q_LOGGING_CATEGORY(lcSessiondump, "tibia.sessiondump")

namespace tibia::game {

using client::gamecore::argBool;
using client::gamecore::argInt;
using client::gamecore::argString;
using client::gamecore::Hook;
using Args = std::vector<std::any>;

namespace {

template<typename F>
Hook gameHook(const char* field, F&& f)
{
    return Hook("g_game", field, [f = std::forward<F>(f)](LuaObject*, const Args& args) {
        f(args);
        return std::any{};
    });
}

}

TSessiondumpGameSession::TSessiondumpGameSession(QObject* parent)
    : cip::tibia::game::TSessiondumpGameSession(parent)
{
    m_replayTimer.setParent(this);
    m_replayTimer.setSingleShot(true);
    connect(&m_replayTimer, &QTimer::timeout, this, &TSessiondumpGameSession::pumpReplay);
    m_hooks.emplace_back("object", "onConnect", [this](LuaObject* self, const Args&) {
        if (dynamic_cast<ProtocolGame*>(self))
            Q_EMIT connected();
        return std::any{};
    });
    m_hooks.push_back(gameHook("onLogin", [this](const Args&) { Q_EMIT gameLoginSuccessful(); }));
    m_hooks.push_back(gameHook("onGameStart", [this](const Args&) { Q_EMIT worldEntered(); }));
    m_hooks.push_back(gameHook("onGameEnd", [this](const Args&) {
        onSessiondumpFinished();
        Q_EMIT disconnected();
    }));
    m_hooks.push_back(gameHook("onLoginError", [this](const Args& a) { Q_EMIT gameLoginErrorMessage(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onLoginAdvice", [this](const Args& a) { Q_EMIT gameLoginAdviceMessage(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onLoginWait", [this](const Args& a) {
        Q_EMIT gameLoginWaitMessage(argString(a, 0), static_cast<int>(argInt(a, 1)));
    }));
    m_hooks.push_back(gameHook("onConnectionError", [this](const Args& a) { Q_EMIT connectionError(argString(a, 0)); }));
    m_hooks.push_back(gameHook("onSessionEnd", [this](const Args& a) { Q_EMIT sessionEndInformation(static_cast<int>(argInt(a, 0))); }));
    m_hooks.push_back(gameHook("onConnectionFailing", [this](const Args& a) {
        if (argBool(a, 0))
            Q_EMIT pingTimeout();
        else
            Q_EMIT pingTimeoutRecovered();
    }));
    m_hooks.push_back(gameHook("onDeath", [this](const Args&) { Q_EMIT deadMessage(); }));
    m_hooks.push_back(gameHook("onUpdateNeeded", [this](const Args&) { Q_EMIT clientOutOfDate(); }));
    m_hooks.push_back(gameHook("onSessionDumpStart", [this](const Args& a) {
        Q_EMIT sessionDumpStart(argString(a, 0));
    }));
    m_hooks.push_back(gameHook("onSessionDumpMarker", [this](const Args& a) {
        Q_EMIT sessionDumpMarker(argString(a, 0));
    }));
}

TSessiondumpGameSession::~TSessiondumpGameSession()
{
    stopReplay();
    stopHints();
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
    try {
        const auto dump = parseSessiondumpDmpdFile(file);
        if (!dump) {
            Q_EMIT connectionError(tr("Failed to parse dmpd session dump: %1").arg(file));
            return;
        }
        if (!dump->header.sessionKey.isEmpty())
            Q_EMIT sessionDumpStart(QString::fromLatin1(dump->header.sessionKey));

        int mapped = 0;
        int skipped = 0;
        auto packets = dmpdToClassicPackets(*dump, &mapped, &skipped);
        if (packets.empty()) {
            Q_EMIT connectionError(tr("dmpd dump produced no mappable classic packets: %1").arg(file));
            return;
        }

        qCInfo(lcSessiondump).noquote()
            << "replaying dmpd" << file << "mapped" << mapped << "skipped" << skipped
            << "character" << dump->header.characterName;

        std::vector<ReplayBody> bodies;
        bodies.reserve(packets.size());
        for (const auto& packet : packets) {
            if (!packet.second || packet.second->empty())
                continue;
            bodies.push_back(ReplayBody{
                packet.first,
                QByteArray(reinterpret_cast<const char*>(packet.second->data()), int(packet.second->size())),
            });
        }
        startReplay(std::move(bodies));
        startHintsForDump(file);
    } catch (const std::exception& e) {
        Q_EMIT connectionError(QString::fromUtf8(e.what()));
    }
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
        qCInfo(lcSessiondump).noquote() << "replaying OTC packet dump" << file << "packets" << bodies.size();
        startReplay(std::move(bodies));
        startHintsForDump(file);
    } catch (const std::exception& e) {
        Q_EMIT connectionError(QString::fromUtf8(e.what()));
    }
}

void TSessiondumpGameSession::startReplay(std::vector<ReplayBody> packets)
{
    g_game.startDecodedReplay();
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
        g_game.deliverReplayPacket(reinterpret_cast<const uint8_t*>(body.constData()), size_t(body.size()));
    }
    if (!m_replaying)
        return;
    if (m_replayIndex >= int(m_replayPackets.size())) {
        m_replaying = false;
        g_game.finishDecodedReplay();
        return;
    }
    const int wait = int(m_replayPackets[size_t(m_replayIndex)].atMs - now);
    m_replayTimer.start(qMax(1, wait));
}

void TSessiondumpGameSession::stopReplay()
{
    m_replaying = false;
    m_replayTimer.stop();
    m_replayPackets.clear();
    m_replayIndex = 0;
}

void TSessiondumpGameSession::startHintsForDump(const QString& dumpPath)
{
    const QString hintsPath = tutorial::hintsPathForDump(dumpPath);
    if (hintsPath.isEmpty()) {
        qCInfo(lcSessiondump).noquote() << "no .hints sidecar for" << dumpPath;
        return;
    }
    m_hints = std::make_unique<tutorial::TSessiondumpHintsProcessor>(this);
    if (!m_hints->load(hintsPath)) {
        m_hints.reset();
        return;
    }
    m_hints->start();
}

void TSessiondumpGameSession::stopHints()
{
    if (m_hints) {
        m_hints->stop();
        m_hints.reset();
    }
}

void TSessiondumpGameSession::requestLogout()
{
    disconnectNow();
}

void TSessiondumpGameSession::disconnectNow()
{
    stopReplay();
    stopHints();
    if (g_game.isOnline())
        g_game.forceLogout();
    else
        g_game.cancelLogin();
}

bool TSessiondumpGameSession::isOnline() const
{
    return g_game.isOnline();
}

QString TSessiondumpGameSession::characterName() const
{
    return QString::fromStdString(g_game.getCharacterName());
}

QString TSessiondumpGameSession::worldName() const
{
    return QString::fromStdString(g_game.getWorldName());
}

void TSessiondumpGameSession::onRequestResetOfStorages()
{
    // CIP resets creature/item storages between dump segments; OTC replay is one shot.
}

void TSessiondumpGameSession::onSessiondumpFinished()
{
    qCInfo(lcSessiondump) << "session dump finished";
    stopHints();
}

}
