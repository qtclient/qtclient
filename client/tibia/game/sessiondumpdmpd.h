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

/// CIP tutorial / cinematic session dump, read through the sessiondump::
/// readers (TSessiondumpMetadataReader, TSessiondumpPacketReader,
/// TSessiondumpMessageReader): "dmp" + u16 metadata size, little-endian
/// metadata, then packets ('p', u32 size, u8 type, u32 timestamp in ms) of
/// messages ('m' server / 'c' client, u16 length, protobuf GameserverMessage).
struct TSessiondumpDmpdHeader
{
    uint16_t version = 0;
    qsizetype headerBytes = 6;
    QByteArray sessionKey;
    QString characterName;
    QString worldName;
    /// Byte offset where the packets begin (after the metadata).
    qsizetype streamOffset = 0;
};

struct TSessiondumpDmpdMessage
{
    qint64 timestampMs = 0;
    /// The packet type (1 or 2).
    uint8_t flag = 0;
    uint8_t opcode = 0;
    /// Full protobuf GameserverMessage bytes (includes field 1 + extension).
    QByteArray protobuf;
};

struct TSessiondumpDmpdFile
{
    TSessiondumpDmpdHeader header;
    QVector<TSessiondumpDmpdMessage> messages;
};

/// True when `bytes` begins with the magic "dmp".
bool isSessiondumpDmpd(const QByteArray& bytes);
bool isSessiondumpDmpdFile(const QString& path);

std::optional<TSessiondumpDmpdHeader> parseSessiondumpDmpdHeader(const QByteArray& bytes);
std::optional<TSessiondumpDmpdHeader> parseSessiondumpDmpdHeaderFile(const QString& path);

std::optional<TSessiondumpDmpdFile> parseSessiondumpDmpd(const QByteArray& bytes);
std::optional<TSessiondumpDmpdFile> parseSessiondumpDmpdFile(const QString& path);

/// Phase 6.0-E: Parse each dmpd `GameserverMessage`, enqueue on the CIP
/// protocol queue, then emit classic bodies for the replay bridge.
/// Structured inverse is primary; `mapSimpleOpcode` remains fallback only.
/// Returns timed packets: `<millis, body>` (opcode-first decoded messages).
using DmpdClassicPacket = std::pair<int64_t, std::shared_ptr<std::vector<uint8_t>>>;
std::deque<DmpdClassicPacket> dmpdToClassicPackets(const TSessiondumpDmpdFile& dump, int* mappedOut = nullptr, int* skippedOut = nullptr);

/// Resolve tutorial fixture: `QTCLIENT_TUTORIAL_SESSIONDUMP`, then
/// `testdata/sessiondumps/tutorial-sessiondump.dmp`, then
/// `tmp/re/tibia/*/tutorial/tutorial-sessiondump.dmp`.
QString resolveTutorialSessiondumpPath();

}
}
