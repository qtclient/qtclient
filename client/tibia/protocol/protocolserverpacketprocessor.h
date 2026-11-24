#pragma once

#include <cstdint>
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
/// Once stopped it takes no more packets.
class TProtocolServerPacketProcessor
{
public:
    TProtocolServerPacketProcessor(std::shared_ptr<network::TUnencryptedRawMessageStream> MessageStream,
                                   std::shared_ptr<TProtocolServerMessageProcessor> ServerMessageProcessor);

    void processPacket(const network::TGameserverNetworkPacket& Packet);
    /// A stopped processor takes no more packets.
    void setStopped(const bool Stopped) { m_stopped = Stopped; }

private:
    std::shared_ptr<network::TUnencryptedRawMessageStream> m_messageStream;
    std::shared_ptr<TProtocolServerMessageProcessor> m_serverMessageProcessor;
    bool m_stopped = false;
};

}
}
