#include "game/sessiondumpdmpd.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

Q_LOGGING_CATEGORY(lcDmpd, "tibia.sessiondump.dmpd")

namespace tibia::game {

namespace {

uint16_t readU16Be(const QByteArray& b, qsizetype off)
{
    return uint16_t((uint8_t(b[off]) << 8) | uint8_t(b[off + 1]));
}

uint16_t readU16Le(const QByteArray& b, qsizetype off)
{
    return uint16_t(uint8_t(b[off]) | (uint16_t(uint8_t(b[off + 1])) << 8));
}

uint32_t readU32Le(const QByteArray& b, qsizetype off)
{
    return uint32_t(uint8_t(b[off]))
        | (uint32_t(uint8_t(b[off + 1])) << 8)
        | (uint32_t(uint8_t(b[off + 2])) << 16)
        | (uint32_t(uint8_t(b[off + 3])) << 24);
}

bool readProtobufVarint(const QByteArray& data, qsizetype& i, uint64_t& out)
{
    out = 0;
    int shift = 0;
    while (i < data.size()) {
        const auto c = uint8_t(data[i++]);
        out |= uint64_t(c & 0x7f) << shift;
        if ((c & 0x80) == 0)
            return true;
        shift += 7;
        if (shift > 63)
            return false;
    }
    return false;
}

/// Extract GameserverMessage opcode from protobuf field 1 (varint).
std::optional<uint8_t> protobufOpcode(const QByteArray& msg)
{
    qsizetype i = 0;
    uint64_t tag = 0;
    if (!readProtobufVarint(msg, i, tag))
        return std::nullopt;
    const auto field = int(tag >> 3);
    const auto wire = int(tag & 7);
    if (field != 1 || wire != 0)
        return std::nullopt;
    uint64_t opcode = 0;
    if (!readProtobufVarint(msg, i, opcode) || opcode > 0xff)
        return std::nullopt;
    return static_cast<uint8_t>(opcode);
}

/// Nested extension payload for field `opcode` (length-delimited), when present.
QByteArray protobufExtensionPayload(const QByteArray& msg, uint8_t opcode)
{
    qsizetype i = 0;
    while (i < msg.size()) {
        uint64_t tag = 0;
        if (!readProtobufVarint(msg, i, tag))
            break;
        const auto field = int(tag >> 3);
        const auto wire = int(tag & 7);
        if (wire == 0) {
            uint64_t v = 0;
            if (!readProtobufVarint(msg, i, v))
                break;
            continue;
        }
        if (wire == 2) {
            uint64_t len = 0;
            if (!readProtobufVarint(msg, i, len) || i + qsizetype(len) > msg.size())
                break;
            if (field == int(opcode))
                return msg.mid(i, qsizetype(len));
            i += qsizetype(len);
            continue;
        }
        if (wire == 5) {
            i += 4;
            continue;
        }
        if (wire == 1) {
            i += 8;
            continue;
        }
        break;
    }
    return {};
}

double readProtobufFixed64Double(const QByteArray& bytes)
{
    if (bytes.size() < 8)
        return 0.0;
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i)
        bits |= uint64_t(uint8_t(bytes[i])) << (8 * i);
    double value = 0.0;
    static_assert(sizeof(double) == 8);
    std::memcpy(&value, &bits, 8);
    return value;
}

void appendU8(std::vector<uint8_t>& out, uint8_t v) { out.push_back(v); }
void appendU16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(uint8_t(v & 0xff));
    out.push_back(uint8_t((v >> 8) & 0xff));
}
void appendU32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(uint8_t(v & 0xff));
    out.push_back(uint8_t((v >> 8) & 0xff));
    out.push_back(uint8_t((v >> 16) & 0xff));
    out.push_back(uint8_t((v >> 24) & 0xff));
}
void appendString(std::vector<uint8_t>& out, const QByteArray& utf8)
{
    const auto len = static_cast<uint16_t>(qMin(utf8.size(), qsizetype(0xffff)));
    appendU16(out, len);
    out.insert(out.end(), utf8.begin(), utf8.begin() + len);
}
void appendOtDouble(std::vector<uint8_t>& out, double value, uint8_t precision = 3)
{
    appendU8(out, precision);
    const auto scaled = int32_t(std::llround(value * std::pow(10.0, precision)));
    appendU32(out, uint32_t(scaled + std::numeric_limits<int32_t>::max()));
}

std::shared_ptr<std::vector<uint8_t>> makePacket(std::vector<uint8_t> body)
{
    return std::make_shared<std::vector<uint8_t>>(std::move(body));
}

QByteArray protobufStringField(const QByteArray& nested, int wantField)
{
    qsizetype i = 0;
    while (i < nested.size()) {
        uint64_t tag = 0;
        if (!readProtobufVarint(nested, i, tag))
            break;
        const auto field = int(tag >> 3);
        const auto wire = int(tag & 7);
        if (wire == 2) {
            uint64_t len = 0;
            if (!readProtobufVarint(nested, i, len) || i + qsizetype(len) > nested.size())
                break;
            const QByteArray chunk = nested.mid(i, qsizetype(len));
            i += qsizetype(len);
            if (field == wantField)
                return chunk;
            continue;
        }
        if (wire == 0) {
            uint64_t v = 0;
            if (!readProtobufVarint(nested, i, v))
                break;
            continue;
        }
        if (wire == 5) {
            i += 4;
            continue;
        }
        if (wire == 1) {
            i += 8;
            continue;
        }
        break;
    }
    return {};
}

bool protobufCoordinate(const QByteArray& bytes, uint16_t& x, uint16_t& y, uint8_t& z)
{
    // CIP Coordinate: field1=x, field2=y, field3=z (varints).
    qsizetype i = 0;
    bool haveX = false, haveY = false, haveZ = false;
    while (i < bytes.size()) {
        uint64_t tag = 0;
        if (!readProtobufVarint(bytes, i, tag))
            break;
        const auto field = int(tag >> 3);
        const auto wire = int(tag & 7);
        if (wire != 0) {
            if (wire == 2) {
                uint64_t len = 0;
                if (!readProtobufVarint(bytes, i, len) || i + qsizetype(len) > bytes.size())
                    break;
                i += qsizetype(len);
                continue;
            }
            break;
        }
        uint64_t v = 0;
        if (!readProtobufVarint(bytes, i, v))
            break;
        if (field == 1) {
            x = uint16_t(v);
            haveX = true;
        } else if (field == 2) {
            y = uint16_t(v);
            haveY = true;
        } else if (field == 3) {
            z = uint8_t(v);
            haveZ = true;
        }
    }
    return haveX && haveY && haveZ;
}

void appendSkipTiles(std::vector<uint8_t>& out, int count)
{
    // Each marker covers 1 + (low byte) empty tiles; max low byte = 255.
    while (count > 0) {
        const int batch = std::min(count, 256);
        appendU16(out, uint16_t(0xff00 | uint8_t(batch - 1)));
        count -= batch;
    }
}

void appendUnknownCreature(std::vector<uint8_t>& out, uint32_t playerId, const QByteArray& nameUtf8)
{
    // Classic UnknownCreature (0x61) for modern OTC (15.x features the tibia
    // client enables: mounts, wings/auras/effects/shader, dual icon lists).
    appendU16(out, 0x61);
    appendU32(out, playerId); // removeId
    appendU32(out, playerId); // id
    appendU8(out, 0); // CreatureTypePlayer
    appendString(out, nameUtf8.isEmpty() ? QByteArrayLiteral("Tutorial") : nameUtf8);
    appendU8(out, 100); // health%
    appendU8(out, 2); // direction South
    // Outfit lookType 128 (male citizen) + colors + addons + mount0 + wings/auras/effects/shader
    appendU16(out, 128);
    appendU8(out, 0);
    appendU8(out, 0);
    appendU8(out, 0);
    appendU8(out, 0);
    appendU8(out, 0); // addons
    appendU16(out, 0); // mount
    appendU16(out, 0); // wings
    appendU16(out, 0); // auras
    appendU16(out, 0); // effects
    appendString(out, {}); // outfit shader
    appendU8(out, 0); // light intensity
    appendU8(out, 0); // light color
    appendU16(out, 220); // speed
    appendU8(out, 0); // icon list 1
    appendU8(out, 0); // icon list 2 (15.30+)
    appendU8(out, 0); // skull
    appendU8(out, 0); // party
    appendU8(out, 0); // emblem (unknown only)
    appendU8(out, 0); // creatureType again
    appendU8(out, 0); // vocation (player)
    appendU8(out, 0); // speech bubble / GameCreatureIcons
    appendU8(out, 0xff); // mark unmarked
    appendU8(out, 0); // inspection
    appendU8(out, 1); // unpass
    appendString(out, {}); // GameCreatureShader
    appendU8(out, 0); // GameCreatureAttachedEffect count
}

/// Synthetic FullMap for CIP dumps: protobuf map tiles are not converted yet,
/// but the cinematic needs a centered camera + local player so the world is not
/// blank. Uses default OTC aware range (viewport 8x6 → 18×14).
std::optional<std::vector<uint8_t>> synthesizeFullMap(const QByteArray& nested, uint32_t playerId,
                                                     const QByteArray& characterName)
{
    const QByteArray coordBytes = protobufStringField(nested, 2);
    uint16_t x = 0, y = 0;
    uint8_t z = 0;
    if (coordBytes.isEmpty() || !protobufCoordinate(coordBytes, x, y, z))
        return std::nullopt;

    constexpr int kLeft = 8, kTop = 6, kRight = 9, kBottom = 7;
    constexpr int kWidth = kLeft + kRight + 1; // 18
    constexpr int kHeight = kTop + kBottom + 1; // 14
    constexpr int kTilesPerFloor = kWidth * kHeight; // 252
    constexpr int kSeaFloor = 7;
    constexpr uint16_t kGroundId = 100; // void/ground — always present in things packs

    std::vector<uint8_t> out;
    appendU8(out, 0x64);
    appendU16(out, x);
    appendU16(out, y);
    appendU8(out, z);

    // Surface-style floors: seaFloor … 0.
    if (z > kSeaFloor) {
        // Underground: ±2 floors around z — keep skip-only for now.
        const int startz = z - 2;
        const int endz = std::min(z + 2, 15);
        for (int nz = startz; nz <= endz; ++nz)
            appendSkipTiles(out, kTilesPerFloor);
    } else {
        for (int nz = kSeaFloor; nz >= 0; --nz) {
            if (nz != z) {
                appendSkipTiles(out, kTilesPerFloor);
                continue;
            }
            const int centerIndex = kLeft * kHeight + kTop; // nx=left, ny=top
            appendSkipTiles(out, centerIndex);
            appendU16(out, kGroundId);
            if (playerId != 0)
                appendUnknownCreature(out, playerId, characterName);
            appendU16(out, 0xff00); // end stack
            appendSkipTiles(out, kTilesPerFloor - centerIndex - 1);
        }
    }
    qCInfo(lcDmpd) << "synthetic FullMap at" << x << y << int(z) << "playerId" << playerId;
    return out;
}

std::optional<std::vector<uint8_t>> mapLoginSuccess(const QByteArray& nested, uint32_t* playerIdOut)
{
    // nested: 1=playerId, 2=beat, 3..5=speed doubles (fixed64), 8=pvp frame,
    // 9=store url, 10=coinsPacketSize, 12=exiva. Skip canReportBugs — modern
    // clients use GameDynamicBugReporter instead.
    qsizetype i = 0;
    uint32_t playerId = 0;
    uint16_t beat = 50;
    double speedA = 0.0, speedB = 0.0, speedC = 0.0;
    bool haveSpeed = false;
    uint8_t pvpFrame = 1;
    uint8_t expert = 0;
    QByteArray storeUrl;
    uint16_t coinsPacket = 25;
    uint8_t exiva = 1;

    while (i < nested.size()) {
        uint64_t tag = 0;
        if (!readProtobufVarint(nested, i, tag))
            break;
        const auto field = int(tag >> 3);
        const auto wire = int(tag & 7);
        if (wire == 0) {
            uint64_t v = 0;
            if (!readProtobufVarint(nested, i, v))
                break;
            if (field == 1)
                playerId = uint32_t(v);
            else if (field == 2)
                beat = uint16_t(v);
            else if (field == 8)
                pvpFrame = uint8_t(v);
            else if (field == 10)
                coinsPacket = uint16_t(v);
            else if (field == 12)
                exiva = uint8_t(v);
            continue;
        }
        if (wire == 1 && i + 8 <= nested.size()) {
            const auto chunk = nested.mid(i, 8);
            i += 8;
            const double d = readProtobufFixed64Double(chunk);
            if (field == 3) {
                speedA = d;
                haveSpeed = true;
            } else if (field == 4)
                speedB = d;
            else if (field == 5)
                speedC = d;
            continue;
        }
        if (wire == 2) {
            uint64_t len = 0;
            if (!readProtobufVarint(nested, i, len) || i + qsizetype(len) > nested.size())
                break;
            if (field == 9)
                storeUrl = nested.mid(i, qsizetype(len));
            i += qsizetype(len);
            continue;
        }
        break;
    }

    if (playerIdOut)
        *playerIdOut = playerId;

    std::vector<uint8_t> out;
    appendU8(out, 0x17);
    appendU32(out, playerId);
    appendU16(out, beat);
    if (haveSpeed) {
        appendOtDouble(out, speedA);
        appendOtDouble(out, speedB);
        appendOtDouble(out, speedC);
    } else {
        appendOtDouble(out, 0.0);
        appendOtDouble(out, 0.0);
        appendOtDouble(out, 0.0);
    }
    appendU8(out, pvpFrame);
    appendU8(out, expert);
    appendString(out, storeUrl);
    appendU16(out, coinsPacket);
    appendU8(out, exiva);
    return out;
}

std::optional<std::vector<uint8_t>> mapCreatureScalar(uint8_t opcode, const QByteArray& nested)
{
    // Health (0x8C), Light (0x8D), Speed (0x8F), Skull (0x90), Party (0x91),
    // Unpass (0x92): field1=creatureId, field2=value (varints).
    qsizetype i = 0;
    uint32_t creatureId = 0;
    uint64_t value = 0;
    bool haveId = false, haveValue = false;
    while (i < nested.size()) {
        uint64_t tag = 0;
        if (!readProtobufVarint(nested, i, tag))
            break;
        const auto field = int(tag >> 3);
        const auto wire = int(tag & 7);
        if (wire != 0) {
            if (wire == 2) {
                uint64_t len = 0;
                if (!readProtobufVarint(nested, i, len) || i + qsizetype(len) > nested.size())
                    break;
                i += qsizetype(len);
                continue;
            }
            break;
        }
        uint64_t v = 0;
        if (!readProtobufVarint(nested, i, v))
            break;
        if (field == 1) {
            creatureId = uint32_t(v);
            haveId = true;
        } else if (field == 2) {
            value = v;
            haveValue = true;
        }
    }
    if (!haveId || !haveValue)
        return std::nullopt;

    std::vector<uint8_t> out;
    appendU8(out, opcode);
    appendU32(out, creatureId);
    switch (opcode) {
        case 0x8C: // health percent
        case 0x90: // skull
        case 0x91: // party
        case 0x92: // unpass
            appendU8(out, uint8_t(value));
            break;
        case 0x8D: // light intensity + color — only intensity known; color 0
            appendU8(out, uint8_t(value));
            appendU8(out, 0);
            break;
        case 0x8F: // speed u16
            appendU16(out, uint16_t(value));
            break;
        default:
            return std::nullopt;
    }
    return out;
}

std::optional<std::vector<uint8_t>> mapSimpleOpcode(uint8_t opcode, const QByteArray& nested, uint32_t* playerIdIo,
                                                   const QByteArray& characterName, bool* emittedFullMap)
{
    switch (opcode) {
        case 0x0A: // PendingGame
            return std::vector<uint8_t>{0x0A};
        case 0x0F: // EnterGame
            return std::vector<uint8_t>{0x0F};
        case 0x04: { // SessionDumpStart — optional string
            std::vector<uint8_t> out{0x04};
            const QByteArray key = protobufStringField(nested, 1);
            if (!key.isEmpty())
                appendString(out, key);
            return out;
        }
        case 0x05: { // SessionDumpMarker
            std::vector<uint8_t> out{0x05};
            const QByteArray marker = protobufStringField(nested, 1);
            appendString(out, marker.isEmpty() ? QByteArrayLiteral("dmpd") : marker);
            return out;
        }
        case 0x17:
            return mapLoginSuccess(nested, playerIdIo);
        case 0x1D: // PingBack
        case 0x1E: // Ping
            return std::vector<uint8_t>{opcode};
        case 0x64: { // FullMap — synthetic until protobuf tiles convert
            if (emittedFullMap && *emittedFullMap)
                return std::nullopt; // only first FullMap
            auto packet = synthesizeFullMap(nested, playerIdIo ? *playerIdIo : 0, characterName);
            if (packet && emittedFullMap)
                *emittedFullMap = true;
            return packet;
        }
        case 0x8C:
        case 0x8D:
        case 0x8F:
        case 0x90:
        case 0x91:
        case 0x92:
            return mapCreatureScalar(opcode, nested);
        case 0x6D: { // MoveCreature — field1 from Coordinate, field2 stackpos, field4 to Coordinate
            const QByteArray fromBytes = protobufStringField(nested, 1);
            const QByteArray toBytes = protobufStringField(nested, 4);
            uint16_t fx = 0, fy = 0, tx = 0, ty = 0;
            uint8_t fz = 0, tz = 0;
            if (fromBytes.isEmpty() || toBytes.isEmpty()
                || !protobufCoordinate(fromBytes, fx, fy, fz)
                || !protobufCoordinate(toBytes, tx, ty, tz))
                return std::nullopt;
            uint8_t stackpos = 0;
            qsizetype i = 0;
            while (i < nested.size()) {
                uint64_t tag = 0;
                if (!readProtobufVarint(nested, i, tag))
                    break;
                const auto field = int(tag >> 3);
                const auto wire = int(tag & 7);
                if (wire == 0) {
                    uint64_t v = 0;
                    if (!readProtobufVarint(nested, i, v))
                        break;
                    if (field == 2 && v <= 0xff)
                        stackpos = uint8_t(v);
                    continue;
                }
                if (wire == 2) {
                    uint64_t len = 0;
                    if (!readProtobufVarint(nested, i, len) || i + qsizetype(len) > nested.size())
                        break;
                    i += qsizetype(len);
                    continue;
                }
                break;
            }
            std::vector<uint8_t> out{0x6D};
            appendU16(out, fx);
            appendU16(out, fy);
            appendU8(out, fz);
            appendU8(out, stackpos);
            appendU16(out, tx);
            appendU16(out, ty);
            appendU8(out, tz);
            return out;
        }
        case 0x82: { // WorldLight: field1 intensity, field2 color
            qsizetype i = 0;
            uint8_t intensity = 0xFF, color = 215;
            while (i < nested.size()) {
                uint64_t tag = 0;
                if (!readProtobufVarint(nested, i, tag))
                    break;
                const auto field = int(tag >> 3);
                const auto wire = int(tag & 7);
                if (wire != 0) {
                    if (wire == 2) {
                        uint64_t len = 0;
                        if (!readProtobufVarint(nested, i, len) || i + qsizetype(len) > nested.size())
                            break;
                        i += qsizetype(len);
                        continue;
                    }
                    break;
                }
                uint64_t v = 0;
                if (!readProtobufVarint(nested, i, v))
                    break;
                if (field == 1)
                    intensity = uint8_t(v);
                else if (field == 2)
                    color = uint8_t(v);
            }
            return std::vector<uint8_t>{0x82, intensity, color};
        }
        case 0xDC: { // TutorialHint — classic: opcode + u8 id
            uint8_t hintId = 0;
            qsizetype i = 0;
            while (i < nested.size()) {
                uint64_t tag = 0;
                if (!readProtobufVarint(nested, i, tag))
                    break;
                const auto field = int(tag >> 3);
                const auto wire = int(tag & 7);
                if (wire == 0) {
                    uint64_t v = 0;
                    if (!readProtobufVarint(nested, i, v))
                        break;
                    if (field == 1 && v <= 0xff)
                        hintId = static_cast<uint8_t>(v);
                    continue;
                }
                if (wire == 2) {
                    uint64_t len = 0;
                    if (!readProtobufVarint(nested, i, len) || i + qsizetype(len) > nested.size())
                        break;
                    i += qsizetype(len);
                    continue;
                }
                break;
            }
            return std::vector<uint8_t>{0xDC, hintId};
        }
        default:
            return std::nullopt;
    }
}

bool parseMetadata(const QByteArray& bytes, qsizetype afterPipe, TSessiondumpDmpdHeader& header)
{
    // After `|`: subtype byte, u16le, u8len name, u32le id, u8len name,
    // u16le, u16le, u16le trailing — then `'p'` stream.
    qsizetype o = afterPipe;
    if (o >= bytes.size())
        return false;
    ++o; // subtype ('1' observed)
    if (o + 2 > bytes.size())
        return false;
    o += 2; // unknown u16
    if (o >= bytes.size())
        return false;
    const auto nameLen = uint8_t(bytes[o++]);
    if (o + nameLen > bytes.size())
        return false;
    header.characterName = QString::fromUtf8(bytes.constData() + o, nameLen);
    o += nameLen;
    if (o + 4 > bytes.size())
        return false;
    o += 4; // account / character id
    if (o >= bytes.size())
        return false;
    const auto name2Len = uint8_t(bytes[o++]);
    if (o + name2Len > bytes.size())
        return false;
    o += name2Len;
    if (o + 6 > bytes.size())
        return false;
    o += 4; // two u16s
    o += 2; // trailing u16 (0x037e observed) before stream
    // Seek to first 'p' if we overshot / undershot.
    while (o < bytes.size() && uint8_t(bytes[o]) != 0x70)
        ++o;
    if (o >= bytes.size() || uint8_t(bytes[o]) != 0x70)
        return false;
    header.streamOffset = o;
    // World name is not always a clean string in meta; leave empty unless found later.
    return true;
}

}

bool isSessiondumpDmpd(const QByteArray& bytes)
{
    return bytes.size() >= 4 && std::memcmp(bytes.constData(), "dmpd", 4) == 0;
}

bool isSessiondumpDmpdFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return isSessiondumpDmpd(file.read(4));
}

std::optional<TSessiondumpDmpdHeader> parseSessiondumpDmpdHeader(const QByteArray& bytes)
{
    if (!isSessiondumpDmpd(bytes) || bytes.size() < 6)
        return std::nullopt;
    TSessiondumpDmpdHeader header;
    header.version = readU16Be(bytes, 4);
    header.headerBytes = 6;
    const auto at = bytes.indexOf('@', 6);
    if (at < 0)
        return header;
    const auto pipe = bytes.indexOf('|', at + 1);
    if (pipe < 0)
        return header;
    header.sessionKey = bytes.mid(at + 1, pipe - at - 1);
    parseMetadata(bytes, pipe + 1, header);
    return header;
}

std::optional<TSessiondumpDmpdHeader> parseSessiondumpDmpdHeaderFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return parseSessiondumpDmpdHeader(file.readAll());
}

std::optional<TSessiondumpDmpdFile> parseSessiondumpDmpd(const QByteArray& bytes)
{
    auto header = parseSessiondumpDmpdHeader(bytes);
    if (!header)
        return std::nullopt;
    if (header->streamOffset <= 0 || header->streamOffset >= bytes.size()) {
        qCWarning(lcDmpd) << "dmpd metadata/stream boundary not found";
        return std::nullopt;
    }

    TSessiondumpDmpdFile dump;
    dump.header = *header;
    qsizetype off = header->streamOffset;
    qint64 currentTs = 0;
    uint8_t currentFlag = 0;
    uint32_t currentSeq = 0;

    while (off < bytes.size()) {
        const auto kind = uint8_t(bytes[off]);
        if (kind == 0x70) {
            if (off + 10 > bytes.size())
                break;
            currentTs = qint64(readU32Le(bytes, off + 1));
            currentFlag = uint8_t(bytes[off + 5]);
            currentSeq = readU32Le(bytes, off + 6);
            off += 10;
            continue;
        }
        if (kind == 0x6d) {
            if (off + 3 > bytes.size())
                break;
            const auto len = readU16Le(bytes, off + 1);
            if (off + 3 + len > bytes.size())
                break;
            const QByteArray payload = bytes.mid(off + 3, len);
            off += 3 + len;
            TSessiondumpDmpdMessage msg;
            msg.timestampMs = currentTs;
            msg.flag = currentFlag;
            msg.seq = currentSeq;
            msg.protobuf = payload;
            if (const auto op = protobufOpcode(payload))
                msg.opcode = *op;
            dump.messages.push_back(std::move(msg));
            continue;
        }
        qCWarning(lcDmpd) << "dmpd stream stopped at offset" << off << "byte" << Qt::hex << kind;
        break;
    }

    qCInfo(lcDmpd) << "parsed dmpd version" << dump.header.version
                    << "sessionKey len" << dump.header.sessionKey.size()
                    << "messages" << dump.messages.size()
                    << "character" << dump.header.characterName;
    return dump;
}

std::optional<TSessiondumpDmpdFile> parseSessiondumpDmpdFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return parseSessiondumpDmpd(file.readAll());
}

std::deque<DmpdClassicPacket> dmpdToClassicPackets(const TSessiondumpDmpdFile& dump, int* mappedOut, int* skippedOut)
{
    std::deque<DmpdClassicPacket> out;
    int mapped = 0;
    int skipped = 0;
    // Ensure PendingGame before first EnterGame when dump starts mid-login.
    bool sawPending = false;
    bool sawEnter = false;
    uint32_t playerId = 0;
    bool emittedFullMap = false;
    const QByteArray characterName = dump.header.characterName.toUtf8();

    for (const auto& msg : dump.messages) {
        const auto nested = protobufExtensionPayload(msg.protobuf, msg.opcode);
        auto classic = mapSimpleOpcode(msg.opcode, nested, &playerId, characterName, &emittedFullMap);
        if (!classic) {
            ++skipped;
            qCInfo(lcDmpd) << "skip protobuf GameserverMessage opcode" << Qt::hex << msg.opcode
                           << "ts" << Qt::dec << msg.timestampMs << "bytes" << msg.protobuf.size();
            continue;
        }
        if (msg.opcode == 0x0A)
            sawPending = true;
        if (msg.opcode == 0x0F)
            sawEnter = true;
        out.emplace_back(msg.timestampMs, makePacket(std::move(*classic)));
        ++mapped;
    }

    if (sawEnter && !sawPending) {
        // OTC expects PendingGame before EnterGame on modern clients.
        auto pending = makePacket({0x0A});
        out.emplace_front(0, pending);
        ++mapped;
    }

    if (mappedOut)
        *mappedOut = mapped;
    if (skippedOut)
        *skippedOut = skipped;
    qCInfo(lcDmpd) << "mapped" << mapped << "classic packets; skipped" << skipped
                    << "playerId" << playerId << "syntheticFullMap" << emittedFullMap;
    return out;
}

QString resolveTutorialSessiondumpPath()
{
    const QString env = qEnvironmentVariable("QTCLIENT_TUTORIAL_SESSIONDUMP");
    if (!env.isEmpty() && QFileInfo::exists(env))
        return QFileInfo(env).absoluteFilePath();

    const QStringList candidates = {
        QStringLiteral("testdata/sessiondumps/tutorial-sessiondump.dmp"),
        QStringLiteral("apps/qtclient/testdata/sessiondumps/tutorial-sessiondump.dmp"),
        QStringLiteral("tmp/re/tibia/ui-art/tutorial/tutorial-sessiondump.dmp"),
        QStringLiteral("tmp/re/tibia/gfx/tutorial/tutorial-sessiondump.dmp"),
    };
    for (const auto& rel : candidates) {
        if (QFileInfo::exists(rel))
            return QFileInfo(rel).absoluteFilePath();
    }

    // Walk tmp/re/tibia/*/tutorial/
    const QDir root(QStringLiteral("tmp/re/tibia"));
    if (root.exists()) {
        for (const auto& entry : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString path = root.filePath(entry + QStringLiteral("/tutorial/tutorial-sessiondump.dmp"));
            if (QFileInfo::exists(path))
                return QFileInfo(path).absoluteFilePath();
        }
    }
    return {};
}

}
