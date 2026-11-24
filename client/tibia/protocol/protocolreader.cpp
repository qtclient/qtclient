#include "protocol/protocolreader.h"

#include "shared/iodevicereader.h"

#include "colorhelper.h"

namespace tibia::protocol {

TProtocolReader::TProtocolReader(std::shared_ptr<shared::TIODeviceReader> MessageReader)
    : m_messageReader(std::move(MessageReader))
{
}

TProtocolReader::~TProtocolReader() = default;

std::shared_ptr<shared::TIODeviceReader> TProtocolReader::messageReader() const { return m_messageReader; }

worldmap::TWorldMapCoordinate TProtocolReader::readCoordinate(uint32_t X)
{
    if (X == 0xffff)
        X = m_messageReader->readUInt16();
    const uint16_t y = m_messageReader->readUInt16();
    const uint8_t z = m_messageReader->readUInt8();
    return worldmap::TWorldMapCoordinate(static_cast<int>(X), y, z);
}

QColor TProtocolReader::readColor() { return ::tibia::shared::colorFromEightBit(m_messageReader->readUInt8()); }

}
