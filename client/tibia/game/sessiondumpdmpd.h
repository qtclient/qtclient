#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace tibia {
namespace game {

/// True when `bytes` begins with the magic "dmp" (a CIP session dump).
bool isSessiondumpDmpd(const QByteArray& bytes);
bool isSessiondumpDmpdFile(const QString& path);

/// Host: CipSoft's session feeds the replayed protobuf GameserverMessages to its
/// protocol handlers directly; qtclient's world still reads classic packets, so
/// each message the sessiondump::TSessiondumpPlayer publishes is turned into
/// classic packets (opcode first) for the replay bridge.
class TDmpdClassicConverter
{
public:
    explicit TDmpdClassicConverter(QByteArray characterName);

    /// The classic packets for one serialized GameserverMessage (none when it
    /// does not map). WorldEntered without an earlier PendingStateEntered gets
    /// one first.
    std::vector<std::shared_ptr<std::vector<uint8_t>>> convert(const QByteArray& protobuf);

private:
    QByteArray m_characterName;
    uint32_t m_playerId = 0;
    bool m_emittedFullMap = false;
    bool m_sawPending = false;
};

/// Resolve tutorial fixture: `QTCLIENT_TUTORIAL_SESSIONDUMP`, then
/// `testdata/sessiondumps/tutorial-sessiondump.dmp`, then
/// `tmp/re/tibia/*/tutorial/tutorial-sessiondump.dmp`.
QString resolveTutorialSessiondumpPath();

}
}
