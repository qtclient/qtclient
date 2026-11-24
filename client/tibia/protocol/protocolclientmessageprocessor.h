#pragma once

#include "network/gameservernetworkpacket.h"

#include <QByteArray>

namespace tibia::protobuf::protocol {
class GameclientMessage;
}

namespace tibia {
namespace protocol {

/// CIP protocol::TProtocolClientMessageProcessor: makes the game server
/// packet of a client message. The login messages are RSA packets for the
/// connection that logs in; every other message is an XTEA packet for all
/// connections, except a connection ping back, which goes only to the
/// connection whose ping it answers.
///
/// Host: qtclient's protocol queue has the message's bytes already (its client
/// translator wrote them when the message was enqueued), so they come in with
/// the message; CipSoft's processor has its translator write them into its
/// message stream and reads them back.
class TProtocolClientMessageProcessor
{
public:
    TProtocolClientMessageProcessor();
    virtual ~TProtocolClientMessageProcessor();

    virtual network::TGameserverNetworkPacket processClientMessage(
        const ::tibia::protobuf::protocol::GameclientMessage& GameclientMessage, const QByteArray& Data) const;
};

}
}
