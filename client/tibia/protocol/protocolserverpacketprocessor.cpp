#include "protocol/protocolserverpacketprocessor.h"

#include "network/gameservernetworkpacket.h"
#include "network/unencryptedrawmessagestream.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolclassicwire.h"
#include "protocol/protocolservermessageprocessor.h"

#include <protocol.pb.h>

#include <idleot/stats.h>

#include <format>

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
    using ::tibia::protobuf::protocol::GameserverMessageUnknown;
    if (m_stopped)
        return;
    m_messageStream->appendData(Packet.data());
    const QByteArray data = m_messageStream->readAllData();
    ClassicReader reader{ reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()), 0 };
    const auto connectionType = static_cast<network::EConnectionType>(Packet.connectionIndex());
    do {
        if (reader.remaining() == 0)
            break;
        const uint8_t opcode = reader.p[reader.i];
        AutoStat stat(STATS_PACKETS, std::format("{} (0x{:02X})", opcode, opcode));
        const EProductMessage productMessage = readProductMessage(reader);
        if (productMessage == EProductMessage::Unreadable)
            break;
        if (productMessage == EProductMessage::Read)
            continue;
        const auto message = m_serverMessageProcessor->processServerMessage(reader, Packet.timestamp(),
                                                                            Packet.sequenceNumber(), connectionType);
        if (!message || message->HasExtension(GameserverMessageUnknown::gameserver_message_unknown))
            break;
    } while (!m_stopped);
    m_messageStream->removeReadData();
}

}
