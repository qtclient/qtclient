#include "protocol/protocolwriter.h"

#include "shared/iodevicewriter.h"

namespace tibia::protocol {

TProtocolWriter::TProtocolWriter(std::shared_ptr<shared::TIODeviceWriter> MessageWriter)
    : m_messageWriter(std::move(MessageWriter))
{
}

TProtocolWriter::~TProtocolWriter() = default;

std::shared_ptr<shared::TIODeviceWriter> TProtocolWriter::messageWriter() const { return m_messageWriter; }

}
