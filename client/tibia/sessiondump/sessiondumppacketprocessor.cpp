#include "sessiondump/sessiondumppacketprocessor.h"

#include "sessiondump/sessiondumpmessageprocessor.h"

#include <stdexcept>

namespace tibia::sessiondump {

TSessiondumpPacketProcessor::TSessiondumpPacketProcessor(std::shared_ptr<TSessiondumpPacketReader> packetReader,
                                                         std::shared_ptr<TSessiondumpMessageProcessor> messageProcessor)
    : m_packetReader(std::move(packetReader))
    , m_messageProcessor(std::move(messageProcessor))
{
}

TSessiondumpPacketProcessor::~TSessiondumpPacketProcessor() = default;

void TSessiondumpPacketProcessor::process()
{
    if (!m_packetReader->isHeaderRead())
        throw std::runtime_error("Packet header has not been read yet");
    m_currentPacket = m_packetReader->info();
    while (!m_packetReader->atEnd())
        m_messageProcessor->process(m_currentPacket);
    m_packetReader->skipPacket();
}

bool TSessiondumpPacketProcessor::hasNextPacket()
{
    if (m_packetReader->hasCompletePayload())
        return true;
    if (!m_packetReader->isHeaderRead())
        m_packetReader->readHeader();
    return m_packetReader->isHeaderRead() && m_packetReader->hasCompletePayload();
}

bool TSessiondumpPacketProcessor::isInsidePacket() const { return !m_packetReader->atEnd(); }

TSessiondumpPacketInfo TSessiondumpPacketProcessor::nextPacketInfo() const
{
    if (!m_packetReader->isHeaderRead())
        throw std::runtime_error("Next packet header has not been read");
    return m_packetReader->info();
}

TSessiondumpPacketInfo TSessiondumpPacketProcessor::currentPacketInfo() const { return m_currentPacket; }

void TSessiondumpPacketProcessor::setMode(const uint32_t mode) { m_messageProcessor->setMode(mode); }

}
