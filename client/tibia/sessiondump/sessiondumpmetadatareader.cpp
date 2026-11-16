#include "sessiondump/sessiondumpmetadatareader.h"

#include "shared/iodevicereader.h"

#include <stdexcept>

namespace tibia::sessiondump {

TSessiondumpMetadataReader::TSessiondumpMetadataReader(std::shared_ptr<shared::TIODeviceReader> reader)
    : m_reader(std::move(reader))
{
    m_metadata.sessionKey = QStringLiteral("");
}

TSessiondumpMetadataReader::~TSessiondumpMetadataReader() = default;

bool TSessiondumpMetadataReader::isRead() const { return m_metadata.read; }

void TSessiondumpMetadataReader::read()
{
    if (!m_reader->isOpen())
        throw std::runtime_error("Sessiondump not openend");
    readMagic();
    if (m_metadata.size != 0 && m_reader->hasBytesAvailable(m_metadata.size)) {
        readFields();
        m_metadata.read = true;
    }
}

void TSessiondumpMetadataReader::readMagic()
{
    if (m_metadata.size != 0 || !m_reader->hasBytesAvailable(5))
        return;
    const uint8_t d = m_reader->readUInt8();
    const uint8_t m = m_reader->readUInt8();
    const uint8_t p = m_reader->readUInt8();
    if (d != 'd' || m != 'm' || p != 'p')
        throw std::runtime_error("Magic bytes of sessiondump are invalid");
    m_metadata.size = m_reader->readUInt16();
    m_metadataEnd = uint64_t(m_metadata.size) + 5;
}

void TSessiondumpMetadataReader::readFields()
{
    m_metadata.version = m_reader->readUInt16();
    m_metadata.clientVersion = m_reader->readUInt32();
    if (m_metadata.version > 5)
        m_metadata.recordingMode = m_reader->readUInt8();
    if (m_metadata.version >= 4 && m_metadata.version <= 6)
        m_metadata.sessionKey = QString::number(m_reader->readInt32());
    else if (m_metadata.version >= 7)
        m_metadata.sessionKey = m_reader->readString8();
    m_metadata.creationTime = QDateTime::fromSecsSinceEpoch(m_reader->readUInt32());
    m_metadata.characterName = m_reader->readString8();
    m_metadata.worldId = m_reader->readUInt32();
    m_metadata.worldName = m_reader->readString8();
    if (m_metadata.version > 4) {
        m_metadata.extraFlag = m_reader->readUInt8();
        m_metadata.extraText = m_reader->readString8();
        m_metadata.extraValue1 = m_reader->readUInt16();
        m_metadata.extraValue2 = m_reader->readUInt16();
    }
}

TSessiondumpMetadata TSessiondumpMetadataReader::metadata() const { return m_metadata; }

uint64_t TSessiondumpMetadataReader::metadataEnd() const { return m_metadataEnd; }

}
