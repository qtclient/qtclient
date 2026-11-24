#pragma once

#include "network/dualconnectionurlinfo.h"
#include "sessiondump/sessiondumpmessageprocessor.h"

#include <QtGlobal>

#include <cstdint>
#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia {
namespace protocol {

class TProtobufServerMessageTranslator;
class TProtocolMessageQueue;

/// CIP protocol::TProtocolServerMessageProcessor: reads one message with its
/// translator, gives it the meta data of the packet it came in (the packet's
/// time, its sequence number and its connection) and puts it on the protocol
/// message queue. A strict processor (the live game server session's) throws
/// TUnknownGameserverException for a server message of an unknown opcode
/// instead. A session dump's message processor hands it the dump's messages:
/// classic server messages, protobuf server messages and classic client
/// messages, all read from the stream the translator reads.
///
/// Host: the translator gives no message after a product message (outside the
/// CIP protocol) or when the rest of the stream does not read; nothing is
/// enqueued then.
class TProtocolServerMessageProcessor : public sessiondump::TSessiondumpMessageConsumer
{
public:
    TProtocolServerMessageProcessor(std::shared_ptr<TProtobufServerMessageTranslator> ServerMessageTranslator,
                                    std::shared_ptr<TProtocolMessageQueue> ProtocolMessageQueue, bool Strict);
    ~TProtocolServerMessageProcessor() override;

    /// A classic server message read from the translator's stream (a live
    /// packet's, or a classic session dump's).
    void processServerMessage(const uint64_t& Timestamp, uint32_t PacketNumber,
                              network::EConnectionType ConnectionType) override;
    /// A classic client message read from the translator's stream.
    void processClientMessage(const uint64_t& Timestamp) override;
    /// A protobuf server message of the message's length, read from the
    /// translator's stream; it keeps the message's timestamp.
    void processServerMessage(const sessiondump::TSessiondumpMessageInfo& MessageInfo, uint32_t PacketNumber,
                              network::EConnectionType ConnectionType) override;

private:
    /// Gives a read server message its meta data and enqueues it (throws for an
    /// unknown one when strict).
    void enqueueServerMessage(const std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage>& Message,
                              const qint64& Timestamp, uint32_t PacketNumber, network::EConnectionType ConnectionType);

    std::shared_ptr<TProtobufServerMessageTranslator> m_serverMessageTranslator;
    std::shared_ptr<TProtocolMessageQueue> m_protocolMessageQueue;
    bool m_strict = false;
};

}
}
