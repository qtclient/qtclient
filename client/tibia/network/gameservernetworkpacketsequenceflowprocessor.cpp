#include "network/gameservernetworkpacketsequenceflowprocessor.h"

#include <algorithm>

namespace tibia::network {

namespace {

bool laterInSequence(const TGameserverNetworkPacket& Left, const TGameserverNetworkPacket& Right)
{
    return Left.sequenceNumber() > Right.sequenceNumber();
}

}

TGameserverNetworkPacketSequenceFlowProcessor::TGameserverNetworkPacketSequenceFlowProcessor()
    : QObject(nullptr)
{
}

TGameserverNetworkPacketSequenceFlowProcessor::~TGameserverNetworkPacketSequenceFlowProcessor() = default;

void TGameserverNetworkPacketSequenceFlowProcessor::processPacket(const TGameserverNetworkPacket& NetworkPacket)
{
    uint32_t& highest = m_highestSequenceNumbers[NetworkPacket.connectionIndex()];
    highest = std::max(NetworkPacket.sequenceNumber(), highest);

    m_pendingPackets.push_back(NetworkPacket);
    std::push_heap(m_pendingPackets.begin(), m_pendingPackets.end(), laterInSequence);

    if (!m_sequenceStarted && NetworkPacket.encryptionType() == TGameserverNetworkPacket::Xtea) {
        m_sequenceStarted = true;
        m_expectedSequenceNumber = NetworkPacket.sequenceNumber();
    }
    passOnPacketsInOrder();
}

void TGameserverNetworkPacketSequenceFlowProcessor::passOnPacketsInOrder()
{
    while (!m_pendingPackets.empty()) {
        const TGameserverNetworkPacket packet = m_pendingPackets.front();
        if (packet.encryptionType() == TGameserverNetworkPacket::Xtea) {
            if (m_expectedSequenceNumber < packet.sequenceNumber())
                break;
            std::pop_heap(m_pendingPackets.begin(), m_pendingPackets.end(), laterInSequence);
            m_pendingPackets.pop_back();
            if (packet.sequenceNumber() != m_expectedSequenceNumber)
                continue;
            m_expectedSequenceNumber = packet.sequenceNumber() + 1;
        } else {
            std::pop_heap(m_pendingPackets.begin(), m_pendingPackets.end(), laterInSequence);
            m_pendingPackets.pop_back();
        }
        Q_EMIT packetReady(packet);
    }
}

}
