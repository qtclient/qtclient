#include "protocol/protocolclientmessageprocessor.h"

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

constexpr int LoginMessageType = 10;
constexpr int SecondaryLoginMessageType = 11;
constexpr int ConnectionPingBackMessageType = 28;

}

TProtocolClientMessageProcessor::TProtocolClientMessageProcessor() = default;

TProtocolClientMessageProcessor::~TProtocolClientMessageProcessor() = default;

network::TGameserverNetworkPacket TProtocolClientMessageProcessor::processClientMessage(
    const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage, const QByteArray& Data) const
{
    using network::EConnectionsUsed;
    using network::TGameserverNetworkPacket;
    using ::tibia::protobuf::protocol::GameclientMessageConnectionPingBack;
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameclient, Data);
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
