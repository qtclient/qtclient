#pragma once

#include "network/gameservernetworkpacket.h"

#include <QObject>

#include <cstdint>
#include <map>
#include <vector>

namespace tibia {
namespace network {

/// CIP network::TGameserverNetworkPacketSequenceFlowProcessor: puts the packets
/// of all game server connections back into the server's order. The first
/// XTEA packet sets the sequence number expected next; XTEA packets are passed
/// on in sequence order (a gap waits for the missing packet), a copy whose
/// sequence number was passed on already (from the other connection) is
/// dropped. Packets before the login are passed on as they come.
class TGameserverNetworkPacketSequenceFlowProcessor : public QObject
{
    Q_OBJECT
public:
    TGameserverNetworkPacketSequenceFlowProcessor();
    ~TGameserverNetworkPacketSequenceFlowProcessor() override;

    virtual void processPacket(const TGameserverNetworkPacket& NetworkPacket);

Q_SIGNALS:
    void packetReady(tibia::network::TGameserverNetworkPacket NetworkPacket);

private:
    /// Passes on every packet that is next in order.
    void passOnPacketsInOrder();

    uint32_t m_expectedSequenceNumber = 0;
    /// Packets not passed on yet: a heap with the lowest sequence number on top.
    std::vector<TGameserverNetworkPacket> m_pendingPackets;
    bool m_sequenceStarted = false;
    /// The highest sequence number seen per connection index.
    std::map<int, uint32_t> m_highestSequenceNumbers;
};

}
}
