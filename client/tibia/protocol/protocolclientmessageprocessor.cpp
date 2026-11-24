#include "protocol/protocolclientmessageprocessor.h"

#include "network/unencryptedrawmessagestream.h"
#include "protocol/protobufclientmessagetranslator.h"

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

constexpr int LoginMessageType = 10;
constexpr int SecondaryLoginMessageType = 11;
constexpr int ConnectionPingBackMessageType = 28;

}

TProtocolClientMessageProcessor::TProtocolClientMessageProcessor(
    std::shared_ptr<TProtobufClientMessageTranslator> ClientMessageTranslator,
    std::shared_ptr<network::TUnencryptedRawMessageStream> MessageStream)
    : m_clientMessageTranslator(std::move(ClientMessageTranslator))
    , m_messageStream(std::move(MessageStream))
{
}

TProtocolClientMessageProcessor::~TProtocolClientMessageProcessor() = default;

network::TGameserverNetworkPacket TProtocolClientMessageProcessor::processClientMessage(
    const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage) const
{
    using network::EConnectionsUsed;
    using network::TGameserverNetworkPacket;
    using ::tibia::protobuf::protocol::GameclientMessageConnectionPingBack;
    m_clientMessageTranslator->writeClientMessage(GameclientMessage);
    m_messageStream->seek(0);
    const QByteArray data = m_messageStream->readAllData();
    m_messageStream->removeReadData();
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameclient, data);
    const int type = GameclientMessage.type();
    if (type == LoginMessageType || type == SecondaryLoginMessageType) {
        packet.setEncryptionType(TGameserverNetworkPacket::LoginRsa);
        return packet;
    }
    packet.setEncryptionType(TGameserverNetworkPacket::Xtea);
    if (type != ConnectionPingBackMessageType) {
        packet.setConnectionsUsed(EConnectionsUsed::PrimaryAndSecondary);
        return packet;
    }
    const GameclientMessageConnectionPingBack message =
        GameclientMessage.GetExtension(GameclientMessageConnectionPingBack::gameclient_message_connection_ping_back);
    if (message.connection_type() == 1)
        packet.setConnectionsUsed(EConnectionsUsed::Secondary);
    else if (message.connection_type() == 0)
        packet.setConnectionsUsed(EConnectionsUsed::Primary);
    return packet;
}

}
