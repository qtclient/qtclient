#include "sessiondump/sessiondumppacketreader.h"

#include "shared/iodevicereader.h"

#include <stdexcept>

namespace tibia::sessiondump {

TSessiondumpPacketReader::TSessiondumpPacketReader(std::shared_ptr<shared::TIODeviceReader> reader)
    : m_reader(std::move(reader))
{
}

TSessiondumpPacketReader::~TSessiondumpPacketReader() = default;

bool TSessiondumpPacketReader::isHeaderRead() const { return m_info.valid; }

void TSessiondumpPacketReader::readHeader()
{
    if (!m_reader->hasBytesAvailable(10))
        return;
    m_info.headerPosition = m_reader->pos();
    if (m_reader->readUInt8() != 'p')
        throw std::runtime_error("Magic byte of sessiondump packet is invalid");
    m_info.size = m_reader->readUInt32();
    m_info.end = uint32_t(m_reader->pos() + m_info.size);
    const uint8_t type = m_reader->readUInt8();
    if (type != 1 && type != 2)
        throw std::runtime_error("Packet type of sessiondump packet is invalid");
    m_info.type = type;
    m_info.timestamp = m_reader->readUInt32();
    m_info.start = m_reader->pos();
    m_info.payloadSize = uint32_t(m_info.end - m_info.start);
    m_info.valid = true;
}

TSessiondumpPacketInfo TSessiondumpPacketReader::info() const { return m_info; }

void TSessiondumpPacketReader::skipPacket()
{
    m_reader->skipTo(uint32_t(m_info.end));
    reset();
}

bool TSessiondumpPacketReader::atEnd() const { return m_info.end == m_reader->pos(); }

bool TSessiondumpPacketReader::hasCompletePayload() const
{
    if (m_info.valid && m_info.start == m_reader->pos())
        return m_reader->hasBytesAvailable(m_info.payloadSize);
    return false;
}

void TSessiondumpPacketReader::reset() { m_info = TSessiondumpPacketInfo(); }

}
