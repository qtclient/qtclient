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
    std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> message =
        m_serverMessageTranslator->translateServerMessage(Reader);
    if (!message)
        return nullptr;
    enqueueServerMessage(message, Timestamp, PacketNumber, ConnectionType);
    return message;
}

void TProtocolServerMessageProcessor::processServerMessage(const uint64_t& Timestamp, const uint32_t PacketNumber,
                                                           const network::EConnectionType ConnectionType)
{
    const std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> message =
        m_serverMessageTranslator->readGameserverMessage();
    if (message)
        enqueueServerMessage(message, static_cast<qint64>(Timestamp), PacketNumber, ConnectionType);
}

void TProtocolServerMessageProcessor::processClientMessage(const uint64_t& Timestamp)
{
    const std::shared_ptr<::tibia::protobuf::protocol::GameclientMessage> message =
        m_serverMessageTranslator->readGameclientMessage();
    message->mutable_meta_data()->set_timestamp(static_cast<qint64>(Timestamp));
    m_protocolMessageQueue->enqueueGameclientMessage(message);
}

void TProtocolServerMessageProcessor::processServerMessage(const sessiondump::TSessiondumpMessageInfo& MessageInfo,
                                                           const uint32_t PacketNumber,
                                                           const network::EConnectionType ConnectionType)
{
    const std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> message =
        m_serverMessageTranslator->readGameserverProtobufMessage(MessageInfo.length);
    enqueueServerMessage(message, static_cast<qint64>(MessageInfo.timestamp), PacketNumber, ConnectionType);
}

void TProtocolServerMessageProcessor::enqueueServerMessage(
    const std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage>& Message, const qint64& Timestamp,
    const uint32_t PacketNumber, const network::EConnectionType ConnectionType)
{
    using ::tibia::protobuf::protocol::GameserverMessageUnknown;
    auto* metaData = Message->mutable_meta_data();
    metaData->set_timestamp(Timestamp);
    metaData->set_packet_number(PacketNumber);
    metaData->set_value3(network::connectionTypeToProtocol(ConnectionType));
    if (Message->HasExtension(GameserverMessageUnknown::gameserver_message_unknown) && m_strict) {
        const auto opcode = static_cast<uint8_t>(
            Message->GetExtension(GameserverMessageUnknown::gameserver_message_unknown).opcode());
        throw TUnknownGameserverException(opcode);
    }
    m_protocolMessageQueue->enqueueGameserverMessage(Message);
}

}
