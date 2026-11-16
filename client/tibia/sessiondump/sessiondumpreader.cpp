#include "sessiondump/sessiondumpreader.h"

#include "sessiondump/sessiondumpmessagereader.h"
#include "sessiondump/sessiondumppacketreader.h"
#include "sessiondump/sessiondumpreadersfactory.h"
#include "shared/iodevicereader.h"

namespace tibia::sessiondump {

TSessiondumpReader::TSessiondumpReader(std::shared_ptr<shared::TIODeviceReader> reader,
                                       std::shared_ptr<TSessiondumpReadersFactory> factory)
    : m_reader(std::move(reader))
    , m_factory(std::move(factory))
{
    if (m_factory) {
        m_metadataReader = m_factory->createMetadataReader(m_reader);
        m_packetReader = m_factory->createPacketReader(m_reader);
        m_messageReader = m_factory->createMessageReader(m_reader);
    }
}

TSessiondumpReader::~TSessiondumpReader() = default;

void TSessiondumpReader::open()
{
    if (m_state != Closed)
        return;
    if (!m_reader->isOpen())
        m_reader->open(QIODevice::ReadOnly);
    m_state = Open;
}

void TSessiondumpReader::close()
{
    m_reader->close();
    m_state = Closed;
    m_metadataReader.reset();
}

bool TSessiondumpReader::atEnd() const { return m_reader->atEnd(); }

TSessiondumpReader::EState TSessiondumpReader::state() const { return m_state; }

TSessiondumpMetadata TSessiondumpReader::metadata()
{
    if (m_state == Closed)
        open();
    if (!m_metadataReader->isRead()) {
        m_metadataReader->read();
        if (m_metadataReader->isRead())
            m_state = MetadataRead;
    }
    return m_metadataReader->metadata();
}

std::shared_ptr<TSessiondumpMetadataReader> TSessiondumpReader::metadataReader() const { return m_metadataReader; }

std::shared_ptr<TSessiondumpPacketReader> TSessiondumpReader::packetReader() const { return m_packetReader; }

std::shared_ptr<TSessiondumpMessageReader> TSessiondumpReader::messageReader() const { return m_messageReader; }

}
