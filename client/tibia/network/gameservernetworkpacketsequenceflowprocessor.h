#pragma once

#include "network/dualconnectionurlinfo.h"
#include "network/gameservernetworkpacket.h"

#include <QObject>

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia {
namespace network {

/// CIP network::TGameserverNetworkPacketSequenceFlowProcessor: puts the packets
/// of all game server connections back into the server's order. The first
/// XTEA packet sets the sequence number expected next; XTEA packets are passed
/// on in sequence order (a gap waits for the missing packet), a copy whose
/// sequence number was passed on already (from the other connection) is
/// dropped. Packets before the login are passed on as they come.
///
/// It also acknowledges the server's pings: each XTEA packet taken
/// off the wire is remembered with the connection it came on; once the
/// messages of that packet were handled (a message of a later packet arrived),
/// every connection ping in it is answered on that connection.
class TGameserverNetworkPacketSequenceFlowProcessor : public QObject
{
    Q_OBJECT
public:
    TGameserverNetworkPacketSequenceFlowProcessor();
    ~TGameserverNetworkPacketSequenceFlowProcessor() override;

    virtual void processPacket(const TGameserverNetworkPacket& NetworkPacket);
    /// Counts the pings of each packet (by the packet number in the
    /// message's meta data); a message of a later packet completes the earlier ones.
    virtual void handleAnyMessage(std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> pGameserverMessage);
    /// Only the connections in use count when old packets are forgotten.
    virtual void onConnectionsUsedChanged(tibia::network::EConnectionsUsed ConnectionsUsed);

Q_SIGNALS:
    void packetReady(tibia::network::TGameserverNetworkPacket NetworkPacket);
    /// Answer a connection ping on connection `ConnectionType`.
    void sendConnectionPing(tibia::network::EConnectionType ConnectionType);

private:
    /// A packet that was taken off connection `connectionIndex`.
    struct TReceivedPacket
    {
        int connectionIndex = 0;
        uint32_t sequenceNumber = 0;
        /// Connection pings of the packet answered so far.
        uint64_t answeredPings = 0;
    };
    /// The messages of one packet.
    struct TPacketMessages
    {
        uint32_t sequenceNumber = 0;
        uint64_t connectionPings = 0;
        /// A message of a later packet arrived: this packet has no more messages.
        bool complete = false;
    };

    /// Passes on every packet that is next in order.
    void passOnPacketsInOrder();
    /// Answers the connection pings of the received packets, oldest first, and
    /// forgets the packets every connection in use is past.
    void answerConnectionPings();

    uint32_t m_expectedSequenceNumber = 0;
    /// Packets not passed on yet: a heap with the lowest sequence number on top.
    std::vector<TGameserverNetworkPacket> m_pendingPackets;
    bool m_sequenceStarted = false;
    /// The packets taken off the wire, oldest first.
    std::list<TReceivedPacket> m_receivedPackets;
    /// The messages per packet sequence number.
    std::map<uint32_t, TPacketMessages> m_packetMessages;
    /// The highest sequence number seen per connection index in use.
    std::map<int, uint32_t> m_highestSequenceNumbers;
};

}
}
