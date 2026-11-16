#include "sessiondump/sessiondumpmessagereader.h"

#include "shared/iodevicereader.h"

#include <stdexcept>

namespace tibia::sessiondump {

TSessiondumpMessageReader::TSessiondumpMessageReader(std::shared_ptr<shared::TIODeviceReader> reader)
    : m_reader(std::move(reader))
{
}

TSessiondumpMessageReader::~TSessiondumpMessageReader() = default;

bool TSessiondumpMessageReader::isHeaderRead() const { return m_info.valid; }

void TSessiondumpMessageReader::readHeader()
{
    const uint8_t magic = m_reader->readUInt8();
    if (magic != 'm' && magic != 'c')
        throw std::runtime_error("Invalid magic bytes in message header");
    m_info.magic = magic;
    m_info.length = m_reader->readUInt16();
    m_info.firstByte = m_reader->peekUInt8();
    m_info.start = m_reader->pos();
    m_info.end = m_info.length + m_info.start;
    m_info.valid = true;
}

TSessiondumpMessageInfo TSessiondumpMessageReader::info() const { return m_info; }

void TSessiondumpMessageReader::skipMessage()
{
    m_reader->skipTo(uint32_t(m_info.end));
    reset();
}

bool TSessiondumpMessageReader::atStart() const { return m_info.start == m_reader->pos(); }

bool TSessiondumpMessageReader::atEnd() const { return m_info.end == m_reader->pos(); }

bool TSessiondumpMessageReader::afterFirstByte() const { return m_info.start + 1 == m_reader->pos(); }

void TSessiondumpMessageReader::reset() { m_info = TSessiondumpMessageInfo(); }

QByteArray TSessiondumpMessageReader::body() const
{
    if (!m_info.valid)
        return {};
    return m_reader->peekBytes(m_info.length);
}

}
