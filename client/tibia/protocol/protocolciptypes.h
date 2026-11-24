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

class TProtobufServerMessageTranslator final : public ::tibia::CipTyped {
public:
    TProtobufServerMessageTranslator() = default;

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

class TProtocolServerMessageProcessor final : public ::tibia::CipTyped {
public:
    TProtocolServerMessageProcessor() = default;
};

/// CIP `protocol::TProtocolWriter` — wire enqueue façade (bodies live on the queue).
class TProtocolWriter final : public ::tibia::CipTyped {
public:
    TProtocolWriter() = default;
};

class TUnknownGameserverException final : public ::tibia::CipTyped {
public:
    TUnknownGameserverException() = default;
};

TProtobufServerMessageTranslator* serverMessageTranslator();
TProtobufClientMessageTranslator* clientMessageTranslator();
/// The game client's current session's protocol message queue; null without a session.
TProtocolMessageQueue* protocolMessageQueue();


/// Empty structured server bodies (measure consumes 0 payload bytes on success).
bool emptyStructuredServerOpcode(int opcode);

struct ClassicReader;
/// Reads one message of `opcode` (the reader stands just past the opcode byte),
/// translates it and puts it on the queue. False when it does not read.
bool translateAndEnqueueServerMessage(int opcode, ClassicReader& packet);
/// CIP TProtocolServerMessageProcessor: every message of a decoded server packet.
void processServerPacket(const uint8_t* data, size_t size);
/// The packet the next server messages come from: its sequence number and the
/// connection it arrived on (0 primary, 1 secondary). Every message read from
/// it carries both in its meta data.
void setServerPacketOrigin(uint32_t PacketNumber, int ConnectionType);
/// Fills a server message's meta data from the packet it was read from.
void setServerMessageMetaData(::tibia::protobuf::protocol::GameserverMessage& message);
/// dispatch.gen.cpp: every CIP server message has a handler / every client message a builder.
bool isHandledServerMessage(uint8_t opcode);
bool bindClientMessage(uint8_t opcode);



/// CIP outbound: a game-built client message, encoded by the client
/// translator and queued for the wire.
void sendGameclientMessage(std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> message);
/// A client message of `opcode` with no fields set yet.
std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> newGameclientMessage(int opcode);

const ::tibia::protobuf::protocol::GameserverMessage* lastShadowServerMessage();

/// The message just enqueued, when it is of `opcode`.
const ::tibia::protobuf::protocol::GameserverMessage* nativeServerMessage(int opcode);

}
}
