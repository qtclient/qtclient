#include "protocol/protocolserverpacketprocessor.h"

#include "network/gameservernetworkpacket.h"
#include "network/unencryptedrawmessagestream.h"
#include "protocol/protocolservermessageprocessor.h"

namespace tibia::protocol {

TProtocolServerPacketProcessor::TProtocolServerPacketProcessor(
    std::shared_ptr<network::TUnencryptedRawMessageStream> MessageStream,
    std::shared_ptr<TProtocolServerMessageProcessor> ServerMessageProcessor)
    : m_messageStream(std::move(MessageStream))
    , m_serverMessageProcessor(std::move(ServerMessageProcessor))
{
}

void TProtocolServerPacketProcessor::processPacket(const network::TGameserverNetworkPacket& Packet)
{
    if (m_stopped)
        return;
    m_messageStream->appendData(Packet.data());
    const uint64_t timestamp = static_cast<uint64_t>(Packet.timestamp());
    const auto connectionType = static_cast<network::EConnectionType>(Packet.connectionIndex());
    do {
        if (m_messageStream->atEnd())
            break;
        m_serverMessageProcessor->processServerMessage(timestamp, Packet.sequenceNumber(), connectionType);
    } while (!m_stopped);
    m_messageStream->removeReadData();
}

}
