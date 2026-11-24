#pragma once

#include <memory>

namespace tibia::network {
class TGameserverNetworkPacket;
class TUnencryptedRawMessageStream;
}

namespace tibia {
namespace protocol {

class TProtocolServerMessageProcessor;

/// CIP protocol::TProtocolServerPacketProcessor: appends a decoded server
/// packet to the session's message stream and has the server message processor
/// read messages from it until it is read to the end, then drops what was read.
/// Every message carries the packet's time, sequence number and connection.
///
/// Host: the classic translator reads a byte span, so the stream's unread
/// bytes are handed to it at once; the product messages outside the CIP
/// protocol are read here, between the CIP ones; and a message that does not
/// read, or one of an unknown opcode, ends the packet (its length is unknown).
class TProtocolServerPacketProcessor
{
public:
    TProtocolServerPacketProcessor(std::shared_ptr<network::TUnencryptedRawMessageStream> MessageStream,
                                   std::shared_ptr<TProtocolServerMessageProcessor> ServerMessageProcessor);

    void processPacket(const network::TGameserverNetworkPacket& Packet);

private:
    std::shared_ptr<network::TUnencryptedRawMessageStream> m_messageStream;
    std::shared_ptr<TProtocolServerMessageProcessor> m_serverMessageProcessor;
    bool m_stopped = false;
};

}
}
