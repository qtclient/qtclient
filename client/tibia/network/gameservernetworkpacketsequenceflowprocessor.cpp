#include "network/gameservernetworkpacketsequenceflowprocessor.h"

#include <protocol.pb.h>

#include <algorithm>
#include <stdexcept>

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
            m_receivedPackets.push_back(TReceivedPacket{ packet.connectionIndex(), packet.sequenceNumber(), 0 });
            if (packet.sequenceNumber() != m_expectedSequenceNumber)
                continue;
            m_expectedSequenceNumber = packet.sequenceNumber() + 1;
        } else {
            std::pop_heap(m_pendingPackets.begin(), m_pendingPackets.end(), laterInSequence);
            m_pendingPackets.pop_back();
        }
        Q_EMIT packetReady(packet);
    }
    answerConnectionPings();
}

void TGameserverNetworkPacketSequenceFlowProcessor::handleAnyMessage(
    std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> pGameserverMessage)
{
    using ::tibia::protobuf::protocol::GameserverMessagePing;
    if (!pGameserverMessage)
        return;
    const uint32_t packetNumber = pGameserverMessage->meta_data().packet_number();
    m_packetMessages.try_emplace(packetNumber, TPacketMessages{ packetNumber, 0, false });
    if (pGameserverMessage->HasExtension(GameserverMessagePing::gameserver_message_ping)) {
        ++m_packetMessages[packetNumber].connectionPings;
        answerConnectionPings();
    }
    for (auto& [sequenceNumber, messages] : m_packetMessages) {
        if (messages.sequenceNumber < packetNumber)
            messages.complete = true;
    }
}

void TGameserverNetworkPacketSequenceFlowProcessor::onConnectionsUsedChanged(const EConnectionsUsed ConnectionsUsed)
{
    std::map<int, uint32_t> highestSequenceNumbers;
    if (ConnectionsUsed == EConnectionsUsed::Secondary || ConnectionsUsed == EConnectionsUsed::Primary) {
        const int connectionIndex = ConnectionsUsed == EConnectionsUsed::Primary ? 0 : 1;
        if (!m_highestSequenceNumbers.empty())
            highestSequenceNumbers.emplace(connectionIndex, m_highestSequenceNumbers.at(connectionIndex));
        highestSequenceNumbers.try_emplace(connectionIndex, 0);
    } else if (ConnectionsUsed == EConnectionsUsed::PrimaryAndSecondary) {
        highestSequenceNumbers = m_highestSequenceNumbers;
        highestSequenceNumbers.try_emplace(1, 0);
        highestSequenceNumbers.try_emplace(0, 0);
    }
    m_highestSequenceNumbers = std::move(highestSequenceNumbers);
}

void TGameserverNetworkPacketSequenceFlowProcessor::answerConnectionPings()
{
    while (!m_receivedPackets.empty() && !m_packetMessages.empty()) {
        TReceivedPacket& packet = m_receivedPackets.front();
        const auto messages = m_packetMessages.find(packet.sequenceNumber);
        if (messages == m_packetMessages.end())
            break;
        while (packet.answeredPings < messages->second.connectionPings) {
            Q_EMIT sendConnectionPing(static_cast<EConnectionType>(packet.connectionIndex));
            ++packet.answeredPings;
        }
        if (!messages->second.complete)
            break;
        m_receivedPackets.pop_front();
    }

    // Packets below every connection's highest sequence number are not coming again.
    uint32_t lowest = 0;
    for (const auto& [connectionIndex, highest] : m_highestSequenceNumbers) {
        if (lowest == 0 || highest < lowest)
            lowest = highest;
    }
    for (auto messages = m_packetMessages.begin(); messages != m_packetMessages.end();) {
        if (messages->second.sequenceNumber < lowest)
            messages = m_packetMessages.erase(messages);
        else
            ++messages;
    }
}

}
