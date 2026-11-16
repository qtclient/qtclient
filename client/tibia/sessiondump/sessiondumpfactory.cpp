#include "sessiondump/sessiondumpfactory.h"

#include "sessiondump/sessiondumpmessageprocessor.h"
#include "sessiondump/sessiondumppacketprocessor.h"
#include "sessiondump/sessiondumpprocessor.h"
#include "sessiondump/sessiondumpreader.h"

namespace tibia::sessiondump {

TSessiondumpFactory::~TSessiondumpFactory() = default;

std::shared_ptr<TSessiondumpReader> TSessiondumpFactory::createReader(const std::shared_ptr<shared::TIODeviceReader>& reader)
{
    return std::make_shared<TSessiondumpReader>(reader, shared_from_this());
}

std::shared_ptr<TSessiondumpMessageProcessor>
TSessiondumpFactory::createMessageProcessor(const std::shared_ptr<TSessiondumpMessageReader>& messageReader,
                                            const std::shared_ptr<TSessiondumpMessageConsumer>& consumer)
{
    return std::make_shared<TSessiondumpMessageProcessor>(messageReader, consumer);
}

std::shared_ptr<TSessiondumpPacketProcessor>
TSessiondumpFactory::createPacketProcessor(const std::shared_ptr<TSessiondumpPacketReader>& packetReader,
                                           const std::shared_ptr<TSessiondumpMessageProcessor>& messageProcessor)
{
    return std::make_shared<TSessiondumpPacketProcessor>(packetReader, messageProcessor);
}

std::shared_ptr<TSessiondumpProcessor>
TSessiondumpFactory::createProcessor(const std::shared_ptr<shared::TIODeviceReader>& reader,
                                     const std::shared_ptr<TSessiondumpMessageConsumer>& consumer)
{
    const auto packetReader = createPacketReader(reader);
    const auto messageReader = createMessageReader(reader);
    const auto dumpReader = createReader(reader);
    const auto messageProcessor = std::make_shared<TSessiondumpMessageProcessor>(messageReader, consumer);
    const auto packetProcessor = std::make_shared<TSessiondumpPacketProcessor>(packetReader, messageProcessor);
    return std::make_shared<TSessiondumpProcessor>(dumpReader, packetProcessor);
}

}
