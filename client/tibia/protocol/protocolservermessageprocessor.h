#pragma once

#include "network/dualconnectionurlinfo.h"

#include <QtGlobal>

#include <cstdint>
#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia {
namespace protocol {

struct ClassicReader;
class TProtobufServerMessageTranslator;
class TProtocolMessageQueue;

/// CIP protocol::TProtocolServerMessageProcessor: reads one server message with
/// its translator, gives it the meta data of the packet it came in (the
/// packet's time, its sequence number and its connection) and puts it on the
/// protocol message queue. A strict processor (the live game server session's)
/// throws TUnknownGameserverException for a message of an unknown opcode
/// instead.
///
/// Host: the translator reads the classic wire through the caller's reader and
/// gives no message when one does not read; the processor then returns null.
class TProtocolServerMessageProcessor
{
public:
    TProtocolServerMessageProcessor(std::shared_ptr<TProtobufServerMessageTranslator> ServerMessageTranslator,
                                    std::shared_ptr<TProtocolMessageQueue> ProtocolMessageQueue, bool Strict);
    virtual ~TProtocolServerMessageProcessor();

    /// The message read and enqueued; null when it does not read.
    virtual std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> processServerMessage(
        ClassicReader& Reader, const qint64& Timestamp, uint32_t PacketNumber, network::EConnectionType ConnectionType);

private:
    std::shared_ptr<TProtobufServerMessageTranslator> m_serverMessageTranslator;
    std::shared_ptr<TProtocolMessageQueue> m_protocolMessageQueue;
    bool m_strict = false;
};

}
}
