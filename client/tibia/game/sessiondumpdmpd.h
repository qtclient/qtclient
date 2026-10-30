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

namespace tibia::game {

/// CIP tutorial / cinematic session-dump container (`dmpd`).
///
/// Layout (from `tutorial-sessiondump.dmp` + CIP `TSessiondumpAsyncLoader`):
///   bytes 0..3   magic `'d''m''p''d'`
///   bytes 4..5   u16be version (observed `7`)
///   bytes 6..…   opaque pre-meta until `'@'` + 64-hex session key + `'|'`
///   then character/world metadata
///   then repeating:
///     `'p'` + 9 bytes packet info (u32le millis, u8 flag, u32le seq)
///     zero or more `'m'` + u16le len + protobuf `GameserverMessage` bytes
///
/// Each GameserverMessage starts with protobuf field 1 = opcode (CIP extension
/// field number). Nested field N==opcode holds the typed payload.
struct TSessiondumpDmpdHeader
{
    uint16_t version = 0;
    qsizetype headerBytes = 6;
    QByteArray sessionKey;
    QString characterName;
    QString worldName;
    /// Byte offset where the `'p'`/`'m'` stream begins.
    qsizetype streamOffset = 0;
};

struct TSessiondumpDmpdMessage
{
    qint64 timestampMs = 0;
    uint8_t flag = 0;
    uint32_t seq = 0;
    uint8_t opcode = 0;
    /// Full protobuf GameserverMessage bytes (includes field 1 + extension).
    QByteArray protobuf;
};

struct TSessiondumpDmpdFile
{
    TSessiondumpDmpdHeader header;
    QVector<TSessiondumpDmpdMessage> messages;
};

/// True when `bytes` begins with magic `dmpd`.
bool isSessiondumpDmpd(const QByteArray& bytes);
bool isSessiondumpDmpdFile(const QString& path);

std::optional<TSessiondumpDmpdHeader> parseSessiondumpDmpdHeader(const QByteArray& bytes);
std::optional<TSessiondumpDmpdHeader> parseSessiondumpDmpdHeaderFile(const QString& path);

std::optional<TSessiondumpDmpdFile> parseSessiondumpDmpd(const QByteArray& bytes);
std::optional<TSessiondumpDmpdFile> parseSessiondumpDmpdFile(const QString& path);

/// Map known protobuf opcodes into classic packet bodies for the live parser.
/// Unknown types are skipped (caller may log counts).
/// Returns timed packets: `<millis, body>` (opcode-first decoded messages).
using DmpdClassicPacket = std::pair<int64_t, std::shared_ptr<std::vector<uint8_t>>>;
std::deque<DmpdClassicPacket> dmpdToClassicPackets(const TSessiondumpDmpdFile& dump, int* mappedOut = nullptr, int* skippedOut = nullptr);

/// Resolve tutorial fixture: `QTCLIENT_TUTORIAL_SESSIONDUMP`, then
/// `testdata/sessiondumps/tutorial-sessiondump.dmp`, then
/// `tmp/re/tibia/*/tutorial/tutorial-sessiondump.dmp`.
QString resolveTutorialSessiondumpPath();

}
