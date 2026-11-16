#include "sessiondump/lightweightsessiondumpmessageprocessor.h"

namespace tibia::sessiondump {

TLightweightSessiondumpMessageProcessor::TLightweightSessiondumpMessageProcessor(
    std::shared_ptr<TSessiondumpMessageReader> messageReader, TCallback callback)
    : TSessiondumpMessageProcessor(std::move(messageReader), nullptr)
    , m_callback(std::move(callback))
{
}

TLightweightSessiondumpMessageProcessor::~TLightweightSessiondumpMessageProcessor() = default;

void TLightweightSessiondumpMessageProcessor::process(const TSessiondumpPacketInfo& packet)
{
    if (!m_messageReader->isHeaderRead())
        m_messageReader->readHeader();
    const TSessiondumpMessageInfo info = m_messageReader->info();
    const std::shared_ptr<TSessiondumpMessageReader> reader = m_messageReader;
    m_callback(packet, info, [reader] { return reader->body(); });
    m_messageReader->skipMessage();
}

}
