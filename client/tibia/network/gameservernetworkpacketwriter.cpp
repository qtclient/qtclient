#include "network/gameservernetworkpacketwriter.h"

#include "network/gameservernetworkpacket.h"
#include "shared/iodevicewriter.h"

#include <utility>

namespace tibia::network {

TGameserverNetworkPacketWriter::TGameserverNetworkPacketWriter(std::shared_ptr<shared::TIODeviceWriter> Writer)
    : m_writer(std::move(Writer))
{
}

TGameserverNetworkPacketWriter::~TGameserverNetworkPacketWriter() = default;

void TGameserverNetworkPacketWriter::writePacket(const TGameserverNetworkPacket& Packet)
{
    const QByteArray data = Packet.data();
    m_writer->writeUInt16(static_cast<uint16_t>((data.size() + 7) >> 3));
    m_writer->writeUInt32(Packet.sequenceNumber());
    m_writer->writeRawBytes(data);
}

}
