#include "protocol/protocolservermessageprocessor.h"

#include "network/networkqualityprotocolmessagehandler.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"
#include "protocol/unknowngameserverexception.h"

#include <protocol.pb.h>

namespace tibia::protocol {

TProtocolServerMessageProcessor::TProtocolServerMessageProcessor(
    std::shared_ptr<TProtobufServerMessageTranslator> ServerMessageTranslator,
    std::shared_ptr<TProtocolMessageQueue> ProtocolMessageQueue, const bool Strict)
    : m_serverMessageTranslator(std::move(ServerMessageTranslator))
    , m_protocolMessageQueue(std::move(ProtocolMessageQueue))
    , m_strict(Strict)
{
}

TProtocolServerMessageProcessor::~TProtocolServerMessageProcessor() = default;

std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> TProtocolServerMessageProcessor::processServerMessage(
    ClassicReader& Reader, const qint64& Timestamp, const uint32_t PacketNumber,
    const network::EConnectionType ConnectionType)
{
    using ::tibia::protobuf::protocol::GameserverMessageUnknown;
    std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> message =
        m_serverMessageTranslator->translateServerMessage(Reader);
    if (!message)
        return nullptr;
    auto* metaData = message->mutable_meta_data();
    metaData->set_timestamp(Timestamp);
    metaData->set_packet_number(PacketNumber);
    metaData->set_value3(network::connectionTypeToProtocol(ConnectionType));
    if (message->HasExtension(GameserverMessageUnknown::gameserver_message_unknown) && m_strict) {
        const auto opcode = static_cast<uint8_t>(
            message->GetExtension(GameserverMessageUnknown::gameserver_message_unknown).opcode());
        throw TUnknownGameserverException(opcode);
    }
    m_protocolMessageQueue->enqueueGameserverMessage(message);
    return message;
}

}
