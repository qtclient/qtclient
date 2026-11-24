#pragma once

#include "network/gameservernetworkpacket.h"

#include <memory>

namespace tibia::protobuf::protocol {
class GameclientMessage;
}

namespace tibia::network {
class TUnencryptedRawMessageStream;
}

namespace tibia {
namespace protocol {

class TProtobufClientMessageTranslator;

/// CIP protocol::TProtocolClientMessageProcessor: makes the game server
/// packet of a client message. The login messages are RSA packets for the
/// connection that logs in; every other message is an XTEA packet for all
/// connections, except a connection ping back, which goes only to the
/// connection whose ping it answers. The packet's data is what its client
/// message translator wrote of the message into the message stream (the
/// stream the translator's writer writes to).
class TProtocolClientMessageProcessor
{
public:
    TProtocolClientMessageProcessor(std::shared_ptr<TProtobufClientMessageTranslator> ClientMessageTranslator,
                                    std::shared_ptr<network::TUnencryptedRawMessageStream> MessageStream);
    virtual ~TProtocolClientMessageProcessor();

    virtual network::TGameserverNetworkPacket processClientMessage(
        const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage) const;

private:
    std::shared_ptr<TProtobufClientMessageTranslator> m_clientMessageTranslator;
    std::shared_ptr<network::TUnencryptedRawMessageStream> m_messageStream;
};

}
}
