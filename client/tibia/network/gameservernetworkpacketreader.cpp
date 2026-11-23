#include "network/gameservernetworkpacketreader.h"

#include "shared/iodevicereader.h"

#include <stdexcept>
#include <utility>

namespace tibia::network {

TGameserverNetworkPacketReader::TGameserverNetworkPacketReader(std::shared_ptr<shared::TIODeviceReader> Reader)
    : m_reader(std::move(Reader))
{
}

TGameserverNetworkPacketReader::~TGameserverNetworkPacketReader() = default;

bool TGameserverNetworkPacketReader::isHeaderRead() const
{
    return m_header.read;
}

void TGameserverNetworkPacketReader::readHeader()
{
    if (!m_reader->hasBytesAvailable(6))
        return;
    const uint32_t blocks = m_reader->readUInt16();
    const uint32_t word = m_reader->readUInt32();
    m_header.packetSize = blocks * 8 + 6;
    m_header.oneShotCompressed = (word >> 31) != 0;
    m_header.streamingCompressed = ((word >> 30) & 1) != 0;
    m_header.sequenceNumber = word & 0x3fffffff;
    m_header.payloadSize = blocks << 3;
    m_header.read = true;
}

TGameserverNetworkPacketReader::THeader TGameserverNetworkPacketReader::header() const
{
    return m_header;
}

bool TGameserverNetworkPacketReader::isPacketComplete() const
{
    if (!m_header.read)
        return false;
    return m_reader->hasBytesAvailable(m_header.payloadSize);
}

TGameserverNetworkPacket TGameserverNetworkPacketReader::takePacket()
{
    if (!isPacketComplete())
        throw std::runtime_error("Packet is not ready");
    TGameserverNetworkPacket packet(TGameserverNetworkPacket::Gameserver, m_reader->readBytes(m_header.payloadSize));
    packet.setSequenceNumber(m_header.sequenceNumber);
    packet.setCompressionType(m_header.streamingCompressed ? TGameserverNetworkPacket::Streaming
                                                           : (m_header.oneShotCompressed ? TGameserverNetworkPacket::OneShot
                                                                                         : TGameserverNetworkPacket::Uncompressed));
    reset();
    return packet;
}

void TGameserverNetworkPacketReader::reset()
{
    m_header = THeader{};
}

}
