#include "network/gameserverpacketprocessor.h"

#include "network/gameservernetworkpacketreader.h"
#include "network/gameservernetworkpacketwriter.h"

#include <utility>

namespace tibia::network {

TGameserverNetworkPacketProcessor::TGameserverNetworkPacketProcessor(std::shared_ptr<TGameserverNetworkPacketReader> Reader,
                                                                     std::shared_ptr<TGameserverNetworkPacketWriter> Writer)
    : QObject(nullptr)
    , m_reader(std::move(Reader))
    , m_writer(std::move(Writer))
{
}

TGameserverNetworkPacketProcessor::~TGameserverNetworkPacketProcessor() = default;

void TGameserverNetworkPacketProcessor::sendPacket(const TGameserverNetworkPacket& Packet)
{
    m_writer->writePacket(Packet);
}

bool TGameserverNetworkPacketProcessor::processAvailablePackets()
{
    const bool wasProcessing = m_processing;
    m_processing = true;
    if (wasProcessing)
        return true;
    while (true) {
        if (!m_reader->isHeaderRead())
            m_reader->readHeader();
        if (!m_reader->isPacketComplete())
            break;
        Q_EMIT packetReceived(m_reader->takePacket());
    }
    const bool running = m_processing;
    m_processing = false;
    return running;
}

void TGameserverNetworkPacketProcessor::onNewPacketDataAvailable()
{
    processAvailablePackets();
}

}
