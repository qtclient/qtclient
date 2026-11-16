#include "sessiondump/sessiondumpmessageprocessor.h"

#include <stdexcept>

namespace tibia::sessiondump {

TSessiondumpMessageProcessor::TSessiondumpMessageProcessor(std::shared_ptr<TSessiondumpMessageReader> messageReader,
                                                           std::shared_ptr<TSessiondumpMessageConsumer> consumer)
    : m_messageReader(std::move(messageReader))
    , m_consumer(std::move(consumer))
{
}

TSessiondumpMessageProcessor::~TSessiondumpMessageProcessor() = default;

void TSessiondumpMessageProcessor::process(const TSessiondumpPacketInfo& packet)
{
    if (!m_messageReader->isHeaderRead())
        m_messageReader->readHeader();
    TSessiondumpMessageInfo info = m_messageReader->info();
    info.timestamp = packet.timestamp;
    if (!info.isServerMessage()) {
        if (m_mode == 0)
            m_consumer->processClientMessage(info.timestamp);
    } else {
        if (m_mode == 1)
            m_consumer->processServerMessage(info, false, true);
        else if (m_mode == 0)
            m_consumer->processServerMessage(info.timestamp, false, true);
        if (!m_messageReader->atEnd() && !m_messageReader->afterFirstByte())
            throw std::runtime_error("Sessiondump message was not consumed completely");
    }
    m_messageReader->skipMessage();
}

void TSessiondumpMessageProcessor::setMode(const uint32_t mode) { m_mode = mode; }

}
