#pragma once

// CIP-named protocol surfaces (Phase 6.0 load-split).
// Architecture: TR → TProtocolMessageQueue → handlers (classic bridge + native).

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
} // namespace protocol
} // namespace protobuf
} // namespace tibia

namespace tibia {
namespace protocol {

void ownProductSurface(QObject* parent);

class TGameActionContextHelper final : public ::tibia::CipTyped {
public:
    TGameActionContextHelper() = default;
};

class TProtobufServerMessageTranslator final : public ::tibia::CipTyped {
public:
    TProtobufServerMessageTranslator() = default;

    std::unique_ptr<::tibia::protobuf::protocol::GameserverMessage>
    translateFromClassic(const QByteArray& classicBody) const;

    std::unique_ptr<::tibia::protobuf::protocol::GameserverMessage>
    translateFromClassic(const uint8_t* data, size_t len) const;

    /// Returns bytes consumed after the opcode byte (0 if unknown / bridge).
    size_t measureStructuredPayload(int opcode, const uint8_t* payload, size_t len) const;

    QByteArray translateToClassic(
        const ::tibia::protobuf::protocol::GameserverMessage& message) const;

    static bool hasStructuredLayout(int opcode);
};

class TProtobufClientMessageTranslator final : public ::tibia::CipTyped {
public:
    TProtobufClientMessageTranslator() = default;

    std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage>
    translateFromClassic(const QByteArray& classicBody) const;

    std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage>
    translateFromClassic(const uint8_t* data, size_t len) const;

    QByteArray translateToClassic(
        const ::tibia::protobuf::protocol::GameclientMessage& message) const;
};

class TProtobufObjectPositionHelper final : public ::tibia::CipTyped {
public:
    TProtobufObjectPositionHelper() = default;
};

class TProtocolClientMessageProcessor final : public ::tibia::CipTyped {
public:
    TProtocolClientMessageProcessor() = default;
};

/// CIP `TProtocolMessageQueue`: single dispatch hub for live + dump + outbound.
class TProtocolMessageQueue final : public cip::tibia::protocol::TProtocolMessageQueue
{
    Q_OBJECT
public:
    using cip::tibia::protocol::TProtocolMessageQueue::TProtocolMessageQueue;

    /// Enqueue server message. When `applyClassic` is true, the classic bridge
    /// re-applies `classicFrame` via `ProtocolGame` (CIP: Q → H).
    void enqueueServer(std::unique_ptr<::tibia::protobuf::protocol::GameserverMessage> message,
                       const QByteArray& classicFrame, bool applyClassic = true);

    void enqueueClient(std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> message,
                       const QByteArray& classicFrame);

    /// CIP slot name: build client message path (classic bridge today).
    void prepareAndEnqueueGameclientMessage(const QByteArray& classicFrame);

    void fireEmitSignalForNewProtocolMessage();
    void fireEmitSignalForNewClientProtocolMessage();

    int serverCount() const { return m_serverCount; }
    int clientCount() const { return m_clientCount; }

Q_SIGNALS:
    void serverMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
    void clientMessageEnqueued(int opcode, const QByteArray& classicFrame);
    void receivedMessage(); // CIP-shaped: fire after server enqueue
    void clientMessageReadyToProcess();

private:
    int m_serverCount = 0;
    int m_clientCount = 0;
    int m_pendingServerOpcode = -1;
    QByteArray m_pendingServerClassic;
    bool m_pendingApplyClassic = true;
    int m_pendingClientOpcode = -1;
    QByteArray m_pendingClientClassic;
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
TProtocolMessageQueue* protocolMessageQueue();

/// Structured opcodes with native H apply — enqueue with applyClassic=false.
/// The list is the switch in protocolciptypes.cpp.
bool nativeApplySkipsClassic(int opcode);

/// Empty structured server bodies (measure consumes 0 payload bytes on success).
bool emptyStructuredServerOpcode(int opcode);

/// Live/dump: translate classic frame and enqueue (CIP TR → Q).
void enqueueTranslatedServerFrame(const QByteArray& classicFrame, bool applyClassic = true);

/// Outbound wire: classic → GameclientMessage → classic load-split bytes.
QByteArray translateOutboundClientFrame(const QByteArray& classicFrame);

/// Outbound enqueue only (CIP prepareAndEnqueue) — does not rewrite wire bytes.
void prepareOutboundClientFrame(const QByteArray& classicFrame);

/// Sessiondump native protobuf → queue (CIP dump → Q).
QByteArray enqueueNativeServerProtobuf(const QByteArray& gameserverProtobuf, bool applyClassic = true);

const ::tibia::protobuf::protocol::GameserverMessage* lastShadowServerMessage();

/// The translated message a native handler applies for `opcode`, or null when
/// the classic parser applies it (`applyClassic`) or the opcode is not native.
const ::tibia::protobuf::protocol::GameserverMessage* nativeServerMessage(int opcode, bool applyClassic);

}
}
