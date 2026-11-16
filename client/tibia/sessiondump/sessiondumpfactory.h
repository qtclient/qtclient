#pragma once

#include "sessiondump/sessiondumpreadersfactory.h"

#include <memory>

namespace tibia {
namespace sessiondump {

class TSessiondumpMessageConsumer;
class TSessiondumpMessageProcessor;
class TSessiondumpPacketProcessor;
class TSessiondumpProcessor;
class TSessiondumpReader;

/// CIP `sessiondump::TSessiondumpFactory`: makes readers and the processor chain
/// of a dump. Must be owned by a std::shared_ptr (readers keep it as their factory).
class TSessiondumpFactory : public TSessiondumpReadersFactory, public std::enable_shared_from_this<TSessiondumpFactory>
{
public:
    ~TSessiondumpFactory() override;

    /// Throws std::bad_weak_ptr when the factory is not owned by a shared_ptr.
    virtual std::shared_ptr<TSessiondumpReader> createReader(const std::shared_ptr<shared::TIODeviceReader>& reader);
    virtual std::shared_ptr<TSessiondumpMessageProcessor>
    createMessageProcessor(const std::shared_ptr<TSessiondumpMessageReader>& messageReader,
                           const std::shared_ptr<TSessiondumpMessageConsumer>& consumer);
    virtual std::shared_ptr<TSessiondumpPacketProcessor>
    createPacketProcessor(const std::shared_ptr<TSessiondumpPacketReader>& packetReader,
                          const std::shared_ptr<TSessiondumpMessageProcessor>& messageProcessor);
    /// Reader, message and packet processors for `reader`, joined into a processor.
    virtual std::shared_ptr<TSessiondumpProcessor> createProcessor(const std::shared_ptr<shared::TIODeviceReader>& reader,
                                                                   const std::shared_ptr<TSessiondumpMessageConsumer>& consumer);
};

}
}
