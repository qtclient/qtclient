#pragma once

#include "game/gamesessionbase.h"
#include "game/sessiondumpgamesessionmetainfo.h"

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>

#include <memory>
#include <vector>

namespace tibia {
namespace sessiondump {
class TSessiondumpAsyncLoader;
class TSessiondumpPlayer;
class TSessiondumpProgressClockService;
}
namespace tutorial {
class TSessiondumpHintsProcessor;
}
}

namespace tibia {
namespace game {

/// CIP-shaped session that replays a recorded game world (CIP: TSessiondumpGameSession).
///
/// Supports:
/// - qtclient text hex dumps (`--replay`: `< millis hex>` / `> millis hex`), converted
/// into the same decoded messages the live socket parses
/// - CIP `dmp` dumps, played by a sessiondump::TSessiondumpPlayer over a
/// TSessiondumpAsyncLoader; each published GameserverMessage becomes classic
/// packets on that same parser (unknown types are skipped with a log)
/// - Sibling `.hints` JSON via `TSessiondumpHintsProcessor` (tutorial cues)
///
/// Live `--record` writes the decoded stream from the primary game connection.
class TSessiondumpGameSession : public TGameSessionBase
{
    Q_OBJECT
public:
    explicit TSessiondumpGameSession(QObject* parent = nullptr);
    ~TSessiondumpGameSession() override;

    /// Builds the session from a TSessiondumpGameSessionMetaInfo.
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;
    /// Plays the meta info's sessiondump.
    void startSession() override;
    /// True for the tutorial dump a new character watches before its live session.
    bool isTutorial() const;

    /// Plays a recorded OTC packet log or CIP `dmpd` dump through g_game.
    void replay(const QString& file);
    /// Drops the replay without waiting for the recording to finish.
    void disconnectNow() override;
    /// The session runs on the dump's progress clock: it stands still while the
    /// dump is paused and follows the playback speed.
    std::shared_ptr<shared::IClockService> clockService() const override;
    /// The clock of the dump's progress (CipSoft's session clock).
    const std::shared_ptr<sessiondump::TSessiondumpProgressClockService>& sessiondumpProgressClock() const
    {
        return m_sessiondumpProgressClock;
    }
    /// The player of the session's dump.
    const std::shared_ptr<sessiondump::TSessiondumpPlayer>& sessiondumpPlayer() const { return m_sessiondumpPlayer; }
    /// No server logout in replay; same as disconnectNow for UI wiring.
    void requestLogout();

    bool isOnline() const;
    QString characterName() const;
    QString worldName() const;

Q_SIGNALS:
    void clientOutOfDate();
    /// Named cue from CIP SessionDumpMarker (S 0x05) during a live dump or replay.
    void sessionDumpMarker(const QString& marker);
    /// CIP SessionDumpStart (S 0x04) when a server begins a dump (Crystal TBD).
    void sessionDumpStart(const QString& sessionKey);

protected Q_SLOTS:
    void onRequestResetOfStorages();
    void onSessiondumpFinished();

private:
    struct ReplayBody
    {
        qint64 atMs = 0;
        QByteArray bytes;
    };

    void replayDmpd(const QString& file);
    void stopDmpd();
    void replayHexDump(const QString& file);
    void startReplay(std::vector<ReplayBody> packets);
    void pumpReplay();
    void stopReplay();
    void startHintsForDump(const QString& dumpPath);
    void stopHints();

    std::shared_ptr<const TSessiondumpGameSessionMetaInfo> m_sessiondumpMetaInfo;
    QTimer m_replayTimer;
    QElapsedTimer m_replayClock;
    std::vector<ReplayBody> m_replayPackets;
    int m_replayIndex = 0;
    bool m_replaying = false;

    std::shared_ptr<sessiondump::TSessiondumpAsyncLoader> m_sessiondumpLoader;
    std::shared_ptr<sessiondump::TSessiondumpProgressClockService> m_sessiondumpProgressClock;
    std::shared_ptr<sessiondump::TSessiondumpPlayer> m_sessiondumpPlayer;
    QTimer m_sessiondumpTimer;

    std::vector<tibia::game::SoftHook> m_hooks;
    std::unique_ptr<tutorial::TSessiondumpHintsProcessor> m_hints;
};

}
}
