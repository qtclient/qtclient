#include "sessiondump/sessiondumpprocessor.h"

#include "sessiondump/sessiondumpmessagereader.h"
#include "sessiondump/sessiondumppacketprocessor.h"
#include "sessiondump/sessiondumpreader.h"

#include <stdexcept>

namespace tibia::sessiondump {

TSessiondumpProcessor::TSessiondumpProcessor(std::shared_ptr<TSessiondumpReader> reader,
                                             std::shared_ptr<TSessiondumpPacketProcessor> packetProcessor)
    : m_reader(std::move(reader))
    , m_packetProcessor(std::move(packetProcessor))
{
    if (m_reader) {
        m_metadataReader = m_reader->metadataReader();
        m_packetReader = m_reader->packetReader();
        m_messageReader = m_reader->messageReader();
    }
}

TSessiondumpProcessor::~TSessiondumpProcessor() = default;

void TSessiondumpProcessor::start()
{
    m_reader->open();
    m_metadata = m_reader->metadata();
    m_packetProcessor->setMode(m_metadata.recordingMode);
}

bool TSessiondumpProcessor::atEnd() const { return m_reader->atEnd(); }

void TSessiondumpProcessor::processUntil(const int64_t timestamp)
{
    while (hasNextPacket()) {
        if (timestamp < int64_t(nextPacketInfo().timestamp))
            return;
        processNextPacket();
    }
}

void TSessiondumpProcessor::processNextPacket() { m_packetProcessor->process(); }

void TSessiondumpProcessor::seekKeyframeBegin()
{
    do {
        if (!m_messageReader->isHeaderRead())
            m_messageReader->readHeader();
        const TSessiondumpMessageInfo info = m_messageReader->info();
        if (!info.isServerMessage())
            throw std::runtime_error("We can only search for KeyframeBegin in server packages");
        if (info.firstByte == 1)
            return;
        m_messageReader->skipMessage();
    } while (!m_packetReader->atEnd());
    throw std::runtime_error("Reached the end of the packet while searching for the KeyframeBegin message");
}

bool TSessiondumpProcessor::hasNextPacket() const
{
    if (m_reader && m_metadataReader && m_metadataReader->isRead() && !m_reader->atEnd())
        return m_packetProcessor->hasNextPacket();
    return false;
}

TSessiondumpPacketInfo TSessiondumpProcessor::nextPacketInfo() const { return m_packetProcessor->nextPacketInfo(); }

TSessiondumpPacketInfo TSessiondumpProcessor::currentPacketInfo() const { return m_packetProcessor->currentPacketInfo(); }

TSessiondumpMetadata TSessiondumpProcessor::metadata() const { return m_reader->metadata(); }

std::shared_ptr<TSessiondumpReader> TSessiondumpProcessor::reader() const { return m_reader; }

}
