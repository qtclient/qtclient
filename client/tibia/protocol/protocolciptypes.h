#pragma once

// CIP-named protocol surfaces (Phase 6.0 load-split).
// Architecture: TR → TProtocolMessageQueue → handlers (classic bridge + native).

#include "protocol/protocolmessagequeue.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

#include <cstdint>
#include <memory>
#include <vector>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessage;
class GameclientMessage;
class ObjectIdentifier;
class ObjectPosition;
} // namespace protocol
} // namespace protobuf
} // namespace tibia

namespace tibia {
namespace protocol {

void ownProductSurface(QObject* parent);

struct ClassicReader;

/// CIP protocol::TProtobufServerMessageTranslator: reads one server message.
///
/// Host: CipSoft's translator reads through its protocol reader's stream;
/// qtclient's reads the classic wire through the caller's ClassicReader, and
/// returns null when a known message does not read (CipSoft's reader throws).
class TProtobufServerMessageTranslator {
public:
    TProtobufServerMessageTranslator() = default;
    virtual ~TProtobufServerMessageTranslator() = default;

    /// Reads the opcode and the message's fields. A message of an opcode it
    /// does not know has only the Unknown extension, with that opcode.
    virtual std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> translateServerMessage(
        ClassicReader& Reader) const;

    QByteArray translateToClassic(
        const ::tibia::protobuf::protocol::GameserverMessage& message) const;

    static bool hasStructuredLayout(int opcode);
};

class TProtobufClientMessageTranslator final : public ::tibia::CipTyped {
public:
    TProtobufClientMessageTranslator() = default;

    QByteArray translateToClassic(
        const ::tibia::protobuf::protocol::GameclientMessage& message) const;

    /// The classic stack position byte of an object: the stack position on a map
    /// field, the inventory slot, the plain container slot, the liquid or tier of
    /// an object "anywhere"; 0 otherwise.
    static uint8_t stackPosition(const ::tibia::protobuf::protocol::ObjectPosition& objectPosition,
                                 const ::tibia::protobuf::protocol::ObjectIdentifier& objectIdentifier);
};

/// CIP `protocol::TProtocolReader` (pin layout size 0x18).
class TProtocolReader final : public ::tibia::CipTyped {
public:
    TProtocolReader() = default;
    explicit TProtocolReader(uint32_t a, uint32_t b, uint64_t c)
        : m_a(a)
        , m_b(b)
        , m_c(c)
    {
    }
    uint32_t a() const { return m_a; }
    uint32_t b() const { return m_b; }
    uint64_t c() const { return m_c; }

private:
    uint32_t m_a = 0; // +0x8
    uint32_t m_b = 0; // +0xc
    uint64_t m_c = 0; // +0x10
};

/// CIP `protocol::TProtocolWriter` — wire enqueue façade (bodies live on the queue).
class TProtocolWriter final : public ::tibia::CipTyped {
public:
    TProtocolWriter() = default;
};

TProtobufClientMessageTranslator* clientMessageTranslator();
/// The game client's current session's protocol message queue; null without a session.
TProtocolMessageQueue* protocolMessageQueue();


/// Empty structured server bodies (measure consumes 0 payload bytes on success).
bool emptyStructuredServerOpcode(int opcode);

/// Host: a decoded server packet that has no network stack behind it (a replay
/// of a classic packet log, or a session that hands its packets to the
/// classic session state): its messages are read like a live packet's and put
/// on the current session's queue.
void processServerPacket(const uint8_t* data, size_t size);
/// Host: the product messages outside the CIP protocol (the extended-opcode
/// transport and the same-socket character switch).
enum class EProductMessage
{
    /// The next message is a CIP message.
    None,
    Read,
    /// A product message that does not read: the rest of the packet cannot be read.
    Unreadable,
};
EProductMessage readProductMessage(ClassicReader& Reader);
/// dispatch.gen.cpp: every CIP server message has a handler / every client message a builder.
bool isHandledServerMessage(uint8_t opcode);
bool bindClientMessage(uint8_t opcode);



/// CIP outbound: a game-built client message, encoded by the client
/// translator and queued for the wire.
void sendGameclientMessage(std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> message);
/// A client message of `opcode` with no fields set yet.
std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> newGameclientMessage(int opcode);

}
}
